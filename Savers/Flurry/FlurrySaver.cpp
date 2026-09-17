#include "FlurrySaver.h"
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

FlurrySettings FlurrySettings::Load(const Settings& s) {
    FlurrySettings v;
    v.streams = Clamp(s.GetInt(L"Streams", v.streams), 1, 12);
    v.preset = Clamp(s.GetInt(L"Preset", v.preset), 0, 5);
    v.speed = Clamp(s.GetInt(L"Speed", v.speed), 1, 10);
    v.brightness = Clamp(s.GetInt(L"Brightness", v.brightness), 1, 10);
    v.trail = Clamp(s.GetInt(L"Trail", v.trail), 1, 10);
    v.bloom = s.GetBool(L"Bloom", v.bloom);
    return v;
}

void FlurrySettings::Save(Settings& s) const {
    s.SetInt(L"Streams", streams);
    s.SetInt(L"Preset", preset);
    s.SetInt(L"Speed", speed);
    s.SetInt(L"Brightness", brightness);
    s.SetInt(L"Trail", trail);
    s.SetBool(L"Bloom", bloom);
}

// ---------------------------------------------------------------- lifecycle

void FlurrySaver::Initialize(Device& device, const SaverContext& ctx) {
    m_ctx = ctx;
    m_settings = FlurrySettings::Load(*ctx.settings);
    m_sprites.Create(device);
    m_dot.FromImage(device, TextureFactory::SoftDot(64, 0.0f), true);
    m_particles.reserve(kMaxParticles);
    CreateTargets(device);
    SpawnStreams();
}

void FlurrySaver::CreateTargets(Device& device) {
    m_trail.Resize(device, m_ctx.width, m_ctx.height);
    int bw = std::max(m_ctx.width / 2, 8), bh = std::max(m_ctx.height / 2, 8);
    m_bloomSrc.Resize(device, bw, bh);
    m_bloomTmp.Resize(device, bw, bh);
    m_bloom.Resize(device, bw, bh);
}

void FlurrySaver::SpawnStreams() {
    Rng& rng = *m_ctx.rng;
    m_streams.clear();
    int n = m_settings.preset == FlurrySettings::RGB ? 3 : m_settings.streams;
    for (int i = 0; i < n; ++i) {
        Stream s{};
        s.ax1 = rng.Range(0.35f, 0.7f); s.ax2 = rng.Range(0.1f, 0.3f);
        s.ay1 = rng.Range(0.35f, 0.7f); s.ay2 = rng.Range(0.1f, 0.3f);
        s.wx1 = rng.Range(0.15f, 0.35f); s.wx2 = rng.Range(0.5f, 1.1f);
        s.wy1 = rng.Range(0.15f, 0.35f); s.wy2 = rng.Range(0.5f, 1.1f);
        s.px1 = rng.Range(0.0f, kTwoPi); s.px2 = rng.Range(0.0f, kTwoPi);
        s.py1 = rng.Range(0.0f, kTwoPi); s.py2 = rng.Range(0.0f, kTwoPi);
        s.hue = static_cast<float>(i) / n + rng.Range(0.0f, 0.1f);
        s.hueRate = rng.Range(0.01f, 0.03f);
        s.pos = s.prev = StreamPos(s, 0.0);
        m_streams.push_back(s);
    }
}

XMFLOAT2 FlurrySaver::StreamPos(const Stream& s, double t) const {
    float hw = m_ctx.width * 0.5f, hh = m_ctx.height * 0.5f;
    float ft = static_cast<float>(t);
    float x = s.ax1 * std::sin(s.wx1 * ft + s.px1) + s.ax2 * std::sin(s.wx2 * ft + s.px2);
    float y = s.ay1 * std::sin(s.wy1 * ft + s.py1) + s.ay2 * std::sin(s.wy2 * ft + s.py2);
    return { hw + x * hw, hh + y * hh };
}

XMFLOAT4 FlurrySaver::StreamColor(const Stream& s, int index) const {
    float t = static_cast<float>(m_time);
    switch (m_settings.preset) {
    case FlurrySettings::RGB: {
        static const XMFLOAT4 rgb[3] = { { 1, 0.15f, 0.1f, 1 }, { 0.15f, 1, 0.2f, 1 }, { 0.2f, 0.35f, 1, 1 } };
        return rgb[index % 3];
    }
    case FlurrySettings::Fire: return HsvToRgb(0.02f + 0.08f * (0.5f + 0.5f * std::sin(t * 0.7f + index)), 1.0f, 1.0f);
    case FlurrySettings::Water: return HsvToRgb(0.5f + 0.12f * (0.5f + 0.5f * std::sin(t * 0.5f + index)), 0.85f, 1.0f);
    case FlurrySettings::Psychedelic: return HsvToRgb(Wrap01(s.hue + t * 0.4f), 1.0f, 1.0f);
    case FlurrySettings::Binary: { float v = (index & 1) ? 1.0f : 0.55f; return { v, v, v, 1 }; }
    default: return HsvToRgb(Wrap01(s.hue + t * s.hueRate), 0.9f, 1.0f);
    }
}

void FlurrySaver::Update(float dt, double) {
    Rng& rng = *m_ctx.rng;
    const float k = 0.5f + 0.1f * m_settings.speed;
    m_time += dt * k;
    const float h = static_cast<float>(m_ctx.height);
    const float scale = h / 1080.0f;

    int index = 0;
    for (auto& s : m_streams) {
        s.prev = s.pos;
        s.pos = StreamPos(s, m_time);
        XMFLOAT2 vel{ (s.pos.x - s.prev.x) / std::max(dt, 1e-4f), (s.pos.y - s.prev.y) / std::max(dt, 1e-4f) };
        float speed = std::sqrt(vel.x * vel.x + vel.y * vel.y);
        XMFLOAT2 tangent = speed > 1e-3f ? XMFLOAT2{ -vel.y / speed, vel.x / speed } : XMFLOAT2{ 1, 0 };
        XMFLOAT4 color = StreamColor(s, index++);
        // 40-80 particles per frame at 60 fps; scale with dt so slow frames keep the density.
        int count = static_cast<int>(rng.Range(40.0f, 80.0f) * std::min(dt * 60.0f, 3.0f));
        for (int i = 0; i < count; ++i) {
            if (m_particles.size() >= kMaxParticles) break;
            // Interpolate along this frame's path so fast streams stay continuous.
            float u = rng.Float();
            Particle p;
            p.x = Lerp(s.prev.x, s.pos.x, u) + rng.Range(-8.0f, 8.0f) * scale;
            p.y = Lerp(s.prev.y, s.pos.y, u) + rng.Range(-8.0f, 8.0f) * scale;
            float side = rng.Range(-1.0f, 1.0f);
            float swirl = rng.Range(0.4f, 2.4f) * std::max(speed, 120.0f * scale);
            p.vx = tangent.x * side * swirl + vel.x * 0.2f + rng.Range(-60.0f, 60.0f) * scale;
            p.vy = tangent.y * side * swirl + vel.y * 0.2f + rng.Range(-60.0f, 60.0f) * scale;
            p.life = p.maxLife = rng.Range(0.5f, 1.4f);
            p.size = rng.Range(5.0f, 14.0f) * scale * m_ctx.dpiScale;
            p.color = color;
            m_particles.push_back(p);
        }
    }
    for (auto& p : m_particles) {
        p.life -= dt;
        float drag = std::max(0.0f, 1.0f - 1.4f * dt);
        p.vx *= drag;
        p.vy *= drag;
        p.x += p.vx * dt;
        p.y += p.vy * dt;
    }
    for (size_t i = 0; i < m_particles.size();) {
        if (m_particles[i].life <= 0.0f) { m_particles[i] = m_particles.back(); m_particles.pop_back(); }
        else ++i;
    }
}

void FlurrySaver::Render(Device& device, SwapChain& swap) {
    ID3D11DeviceContext* ctx = device.Ctx();
    if (!m_trail.Valid()) CreateTargets(device);
    const States& states = m_sprites.GetStates();

    // Feedback: the previous frame zooms out a touch, blurs and fades, then this frame's
    // particles are added on top.
    float fade = 0.86f + 0.012f * m_settings.trail;
    m_trail.Begin(device, fade, 1.006f, 1.0f);
    // Thousands of particles overlap; each adds only a little so the strands stay coloured.
    float bright = 0.010f + 0.008f * m_settings.brightness;
    m_sprites.Begin(m_trail.Width(), m_trail.Height());
    for (const Particle& p : m_particles) {
        float t = Saturate(p.life / p.maxLife);
        float a = t * (2.0f - t) * bright;   // bright while young
        m_sprites.Push(p.x, p.y, p.size * (0.5f + 0.5f * t), p.size * (0.5f + 0.5f * t), { p.color.x * a, p.color.y * a, p.color.z * a, a });
    }
    m_sprites.End(device, &m_dot, states.Additive());

    if (m_settings.bloom) {
        m_bloomSrc.Bind(ctx);
        m_trail.Post().Copy(ctx, m_trail.Current().SRV());
        m_trail.Post().GaussianBlur(device, m_bloomSrc, m_bloomTmp, m_bloom, 1);
    }
    swap.Bind();
    swap.SetViewport(m_ctx.viewport);
    m_trail.Present(device);
    if (m_settings.bloom) {
        m_sprites.Begin(m_ctx.width, m_ctx.height);
        m_sprites.Push(m_ctx.width * 0.5f, m_ctx.height * 0.5f, static_cast<float>(m_ctx.width), static_cast<float>(m_ctx.height), { 0.6f, 0.6f, 0.6f, 1 });
        m_sprites.EndWithSRV(device, m_bloom.SRV(), states.Additive());
    }
}

void FlurrySaver::Resize(int width, int height) {
    m_ctx.width = width;
    m_ctx.height = height;
    m_ctx.viewport = { 0, 0, width, height };
    m_particles.clear();
    m_trail = TrailBuffer{};
    for (auto& s : m_streams) s.pos = s.prev = StreamPos(s, m_time);
}
