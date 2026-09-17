#include "FireworksSaver.h"
#include "Gfx/Device.h"
#include "Gfx/SwapChain.h"
#include "Gfx/TextureFactory.h"
#include "Util/Color.h"
#include "Util/MathUtil.h"
#include <algorithm>
#include <cmath>

using namespace DirectX;
using namespace rs;

// ---------------------------------------------------------------- settings

FireworksSettings FireworksSettings::Load(const Settings& s) {
    FireworksSettings v;
    v.launchRate = Clamp(s.GetInt(L"LaunchRate", v.launchRate), 1, 10);
    v.gravity = Clamp(s.GetInt(L"Gravity", v.gravity), 1, 10);
    v.trail = Clamp(s.GetInt(L"Trail", v.trail), 1, 10);
    v.bloom = Clamp(s.GetInt(L"Bloom", v.bloom), 0, 10);
    v.burstSize = Clamp(s.GetInt(L"BurstSize", v.burstSize), 1, 10);
    v.finaleEvery = Clamp(s.GetInt(L"FinaleEvery", v.finaleEvery), 0, 120);
    return v;
}

void FireworksSettings::Save(Settings& s) const {
    s.SetInt(L"LaunchRate", launchRate);
    s.SetInt(L"Gravity", gravity);
    s.SetInt(L"Trail", trail);
    s.SetInt(L"Bloom", bloom);
    s.SetInt(L"BurstSize", burstSize);
    s.SetInt(L"FinaleEvery", finaleEvery);
}

// ---------------------------------------------------------------- lifecycle

void FireworksSaver::Initialize(Device& device, const SaverContext& ctx) {
    m_ctx = ctx;
    m_settings = FireworksSettings::Load(*ctx.settings);
    m_sprites.Create(device);
    m_dot.FromImage(device, TextureFactory::SoftDot(64, 0.15f), true);
    m_particles.reserve(kMaxParticles);
    CreateTargets(device);
    m_launchTimer = 0.5f;
    m_finaleTimer = static_cast<float>(m_settings.finaleEvery);
}

void FireworksSaver::CreateTargets(Device& device) {
    m_scale = m_ctx.height / 1080.0f;
    m_gravity = (0.25f + 0.05f * m_settings.gravity) * m_ctx.height;
    m_trail.Resize(device, m_ctx.width, m_ctx.height);
    int bw = std::max(m_ctx.width / 2, 8), bh = std::max(m_ctx.height / 2, 8);
    m_bloomSrc.Resize(device, bw, bh);
    m_bloomTmp.Resize(device, bw, bh);
    m_bloom.Resize(device, bw, bh);
}

// ---------------------------------------------------------------- particles

FireworksSaver::Particle* FireworksSaver::Alloc() {
    // The pool is reserved to kMaxParticles up front, so appending never reallocates and the
    // references Explode/Update hold into the vector stay valid while they spawn.
    if (m_particles.size() >= kMaxParticles) return nullptr;
    m_particles.emplace_back();
    return &m_particles.back();
}

void FireworksSaver::LaunchRocket() {
    Rng& rng = *m_ctx.rng;
    Particle* p = Alloc();
    if (!p) return;
    const float w = static_cast<float>(m_ctx.width), h = static_cast<float>(m_ctx.height);
    // Aim for an apex between 35% and 80% of the way up the screen.
    float apex = h * rng.Range(0.35f, 0.8f);
    p->x = rng.Range(w * 0.15f, w * 0.85f);
    p->y = h + 4.0f;
    p->vy = -std::sqrt(2.0f * m_gravity * apex);
    p->vx = rng.Range(-0.06f, 0.06f) * h;
    p->life = p->maxLife = 10.0f;    // bursts at the apex, well before this
    p->size = 4.0f * m_scale * m_ctx.dpiScale;
    p->drag = 0.0f;
    p->color = { 1.0f, 0.85f, 0.6f, 1.0f };
    p->kind = Kind::Rocket;
    p->emits = true;
    p->twinkle = 0.0f;
}

void FireworksSaver::Emit(const Particle& from, int count, float speed, float life, float size, const XMFLOAT4& color) {
    Rng& rng = *m_ctx.rng;
    for (int i = 0; i < count; ++i) {
        Particle* p = Alloc();
        if (!p) return;
        // Uniform direction on a sphere projected to the screen gives the bright rim; a third of
        // the sparks are slower so the shell has a filled centre too.
        float z = rng.Range(-1.0f, 1.0f), a = rng.Range(0.0f, kTwoPi);
        float r = std::sqrt(1.0f - z * z);
        float s = speed * (rng.Chance(0.35f) ? rng.Range(0.3f, 0.8f) : rng.Range(0.85f, 1.0f));
        p->x = from.x;
        p->y = from.y;
        p->vx = from.vx * 0.3f + std::cos(a) * r * s;
        p->vy = from.vy * 0.3f + std::sin(a) * r * s;
        p->life = p->maxLife = life * rng.Range(0.75f, 1.15f);
        p->size = size;
        p->drag = 1.1f;
        p->color = color;
        p->kind = Kind::Spark;
        p->emits = false;
        p->twinkle = rng.Range(0.0f, kTwoPi);
    }
}

void FireworksSaver::Explode(const Particle& rocket) {
    Rng& rng = *m_ctx.rng;
    const float h = static_cast<float>(m_ctx.height);
    const int n = static_cast<int>((40 + 32 * m_settings.burstSize) * rng.Range(0.8f, 1.2f));
    const float speed = h * rng.Range(0.22f, 0.32f) * (0.7f + 0.06f * m_settings.burstSize);
    const float size = (5.0f + 0.4f * m_settings.burstSize) * m_scale * m_ctx.dpiScale;
    float hue = rng.Float();
    XMFLOAT4 color = HsvToRgb(hue, rng.Range(0.55f, 0.95f), 1.0f);

    Burst type = static_cast<Burst>(rng.Int(0, static_cast<int>(Burst::Count) - 1));
    size_t first = m_particles.size();
    switch (type) {
    case Burst::Peony:
        Emit(rocket, n, speed, 1.6f, size, color);
        if (rng.Chance(0.5f)) {   // two-tone core
            XMFLOAT4 inner = HsvToRgb(Wrap01(hue + rng.Range(0.3f, 0.7f)), 0.8f, 1.0f);
            Emit(rocket, n / 3, speed * 0.5f, 1.3f, size, inner);
        }
        break;
    case Burst::Chrysanthemum:
        Emit(rocket, n * 2 / 3, speed, 2.0f, size, color);
        for (size_t i = first; i < m_particles.size(); ++i) m_particles[i].emits = true;
        break;
    case Burst::Ring: {
        // A circle in a randomly tilted plane, projected: reads as an ellipse.
        float tilt = rng.Range(0.0f, kPi), squash = rng.Range(0.35f, 1.0f);
        for (int i = 0; i < n; ++i) {
            Particle* p = Alloc();
            if (!p) break;
            float a = kTwoPi * (i + rng.Range(-0.2f, 0.2f)) / n;
            float ex = std::cos(a), ey = std::sin(a) * squash;
            float s = speed * 0.9f;
            p->x = rocket.x;
            p->y = rocket.y;
            p->vx = (ex * std::cos(tilt) - ey * std::sin(tilt)) * s;
            p->vy = (ex * std::sin(tilt) + ey * std::cos(tilt)) * s;
            p->life = p->maxLife = 1.5f;
            p->size = size;
            p->drag = 1.1f;
            p->color = color;
            p->kind = Kind::Spark;
            p->emits = false;
            p->twinkle = 0.0f;
        }
        break;
    }
    case Burst::Willow:
        Emit(rocket, n, speed * 1.1f, 3.6f, size * 0.8f, { 1.0f, 0.78f, 0.35f, 1.0f });
        for (size_t i = first; i < m_particles.size(); ++i) {
            m_particles[i].kind = Kind::Willow;
            m_particles[i].drag = 2.6f;
            m_particles[i].emits = true;
        }
        break;
    default: // Crackle
        Emit(rocket, n * 3 / 2, speed * 0.8f, 1.4f, size * 0.6f, { 1.0f, 0.95f, 0.8f, 1.0f });
        for (size_t i = first; i < m_particles.size(); ++i) {
            m_particles[i].kind = Kind::Crackle;
            m_particles[i].drag = 1.8f;
        }
        break;
    }
    // Flash at the burst point.
    if (Particle* f = Alloc()) {
        *f = rocket;
        f->kind = Kind::Trail;
        f->vx = f->vy = 0.0f;
        f->life = f->maxLife = 0.12f;
        f->size = size * 14.0f;
        f->color = { 1, 1, 1, 1 };
        f->emits = false;
    }
}

void FireworksSaver::Update(float dt, double) {
    Rng& rng = *m_ctx.rng;
    const float h = static_cast<float>(m_ctx.height);

    // Launches: Poisson-ish at the configured rate, plus a finale burst every N seconds.
    float rate = 0.25f + 0.32f * m_settings.launchRate;
    if (m_settings.finaleEvery > 0) {
        m_finaleTimer -= dt;
        if (m_finaleTimer <= 0.0f) { m_finaleLeft = 3.0f; m_finaleTimer = static_cast<float>(m_settings.finaleEvery); }
    }
    if (m_finaleLeft > 0.0f) { m_finaleLeft -= dt; rate *= 8.0f; }
    m_launchTimer -= dt;
    while (m_launchTimer <= 0.0f) {
        LaunchRocket();
        m_launchTimer += -std::log(std::max(rng.Float(), 1e-4f)) / rate;   // exponential gap
    }

    // Integrate. New particles spawned during the pass (trails, bursts) are appended and
    // handled next frame, so iterate by index over the starting count.
    const size_t count = m_particles.size();
    for (size_t i = 0; i < count; ++i) {
        Particle& p = m_particles[i];
        p.life -= dt;
        p.vy += m_gravity * dt * (p.kind == Kind::Rocket ? 1.0f : 0.55f);
        float k = std::max(0.0f, 1.0f - p.drag * dt);
        p.vx *= k;
        p.vy *= k;
        p.x += p.vx * dt;
        p.y += p.vy * dt;
        if (p.kind == Kind::Rocket && (p.vy >= -0.08f * h || p.life <= 0.0f)) {
            Explode(p);
            p.life = 0.0f;
            continue;
        }
        if (p.emits && p.life > 0.0f) {
            int n = p.kind == Kind::Rocket ? 2 : (rng.Chance(0.5f) ? 1 : 0);
            for (int t = 0; t < n; ++t) {
                Particle* s = Alloc();
                if (!s) break;
                s->x = p.x + rng.Range(-1.0f, 1.0f) * m_scale;
                s->y = p.y + rng.Range(-1.0f, 1.0f) * m_scale;
                s->vx = rng.Range(-0.02f, 0.02f) * h;
                s->vy = rng.Range(-0.02f, 0.02f) * h + (p.kind == Kind::Rocket ? 0.05f * h : 0.0f);
                s->life = s->maxLife = p.kind == Kind::Rocket ? 0.35f : 0.6f;
                s->size = p.size * 0.6f;
                s->drag = 2.0f;
                s->color = p.kind == Kind::Rocket ? XMFLOAT4{ 1.0f, 0.6f, 0.25f, 1.0f } : XMFLOAT4{ p.color.x * 0.8f, p.color.y * 0.8f, p.color.z * 0.8f, 1.0f };
                s->kind = Kind::Trail;
                s->emits = false;
                s->twinkle = 0.0f;
            }
        }
    }
    // Compact: drop dead particles (order does not matter).
    for (size_t i = 0; i < m_particles.size();) {
        if (m_particles[i].life <= 0.0f) {
            m_particles[i] = m_particles.back();
            m_particles.pop_back();
        } else {
            ++i;
        }
    }
}

void FireworksSaver::Render(Device& device, SwapChain& swap) {
    ID3D11DeviceContext* ctx = device.Ctx();
    if (!m_trail.Valid()) CreateTargets(device);
    const States& states = m_sprites.GetStates();

    // 1. Decay the trail and add this frame's particles (additive soft dots).
    float fade = 0.80f + 0.018f * m_settings.trail;
    m_trail.Begin(device, fade);
    m_sprites.Begin(m_trail.Width(), m_trail.Height());
    for (const Particle& p : m_particles) {
        float t = Saturate(p.life / p.maxLife);
        float bright = t * t * (3.0f - 2.0f * t);
        float size = p.size;
        if (p.kind == Kind::Crackle) {
            // Silver sparks that blink on and off.
            float f = std::sin(p.twinkle + p.life * 40.0f);
            if (f < 0.0f) continue;
            bright *= 0.5f + 0.5f * f;
        }
        if (p.kind == Kind::Willow) bright *= 0.85f;
        if (p.kind == Kind::Trail) size *= 0.6f + 0.4f * t;
        XMFLOAT4 c{ p.color.x * bright, p.color.y * bright, p.color.z * bright, bright };
        m_sprites.Push(p.x, p.y, size, size, c);
    }
    m_sprites.End(device, &m_dot, states.Additive());

    // 2. Bloom from the trail at half resolution.
    const bool bloom = m_settings.bloom > 0;
    if (bloom) {
        m_bloomSrc.Bind(ctx);
        m_trail.Post().Copy(ctx, m_trail.Current().SRV());
        m_trail.Post().GaussianBlur(device, m_bloomSrc, m_bloomTmp, m_bloom, 2);
    }

    // 3. Composite into the viewport: trail, then bloom added on top.
    swap.Bind();
    swap.SetViewport(m_ctx.viewport);
    m_trail.Present(device);
    if (bloom) {
        float k = 0.12f * m_settings.bloom;
        m_sprites.Begin(m_ctx.width, m_ctx.height);
        m_sprites.Push(m_ctx.width * 0.5f, m_ctx.height * 0.5f, static_cast<float>(m_ctx.width), static_cast<float>(m_ctx.height), { k, k, k, 1 });
        m_sprites.End(device, m_bloom.SRV(), states.Additive());
    }
}

void FireworksSaver::Resize(int width, int height) {
    m_ctx.width = width;
    m_ctx.height = height;
    m_ctx.viewport = { 0, 0, width, height };
    m_particles.clear();
    m_trail = TrailBuffer{};   // recreated lazily in Render
}
