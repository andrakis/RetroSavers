#include "AttractorsSaver.h"
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

AttractorsSettings AttractorsSettings::Load(const Settings& s) {
    AttractorsSettings v;
    v.attractor = Clamp(s.GetInt(L"Attractor", v.attractor), 0, 5);
    v.particles = Clamp(s.GetInt(L"Particles", v.particles), 2000, 40000);
    v.trail = Clamp(s.GetInt(L"Trail", v.trail), 1, 10);
    v.speed = Clamp(s.GetInt(L"Speed", v.speed), 1, 10);
    v.colorMode = Clamp(s.GetInt(L"ColorMode", v.colorMode), 0, 2);
    return v;
}

void AttractorsSettings::Save(Settings& s) const {
    s.SetInt(L"Attractor", attractor);
    s.SetInt(L"Particles", particles);
    s.SetInt(L"Trail", trail);
    s.SetInt(L"Speed", speed);
    s.SetInt(L"ColorMode", colorMode);
}

// ---------------------------------------------------------------- flows

XMFLOAT3 AttractorsSaver::Flow(const XMFLOAT3& p) const {
    const float x = p.x, y = p.y, z = p.z;
    switch (m_current) {
    case AttractorsSettings::Rossler:
        return { -y - z, x + 0.2f * y, 0.2f + z * (x - 5.7f) };
    case AttractorsSettings::Aizawa: {
        const float a = 0.95f, b = 0.7f, c = 0.6f, d = 3.5f, e = 0.25f, f = 0.1f;
        return { (z - b) * x - d * y, d * x + (z - b) * y,
                 c + a * z - z * z * z / 3.0f - (x * x + y * y) * (1.0f + e * z) + f * z * x * x * x };
    }
    case AttractorsSettings::Thomas: {
        const float b = 0.208186f;
        return { std::sin(y) - b * x, std::sin(z) - b * y, std::sin(x) - b * z };
    }
    case AttractorsSettings::Halvorsen: {
        const float a = 1.89f;
        return { -a * x - 4 * y - 4 * z - y * y, -a * y - 4 * z - 4 * x - z * z, -a * z - 4 * x - 4 * y - x * x };
    }
    default: // Lorenz
        return { 10.0f * (y - x), x * (28.0f - z) - y, x * y - (8.0f / 3.0f) * z };
    }
}

void AttractorsSaver::Integrate(Particle& q, float h) {
    auto add = [](const XMFLOAT3& a, const XMFLOAT3& b, float k) { return XMFLOAT3{ a.x + b.x * k, a.y + b.y * k, a.z + b.z * k }; };
    XMFLOAT3 k1 = Flow(q.p);
    XMFLOAT3 k2 = Flow(add(q.p, k1, h * 0.5f));
    XMFLOAT3 k3 = Flow(add(q.p, k2, h * 0.5f));
    XMFLOAT3 k4 = Flow(add(q.p, k3, h));
    XMFLOAT3 d{ (k1.x + 2 * k2.x + 2 * k3.x + k4.x) / 6.0f, (k1.y + 2 * k2.y + 2 * k3.y + k4.y) / 6.0f, (k1.z + 2 * k2.z + 2 * k3.z + k4.z) / 6.0f };
    q.p = add(q.p, d, h);
    q.speed = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
}

void AttractorsSaver::SelectAttractor(int which) {
    m_current = which;
    switch (which) {
    case AttractorsSettings::Rossler:   m_sys = { { 0, 0, 5 }, 14.0f, 0.02f, 22.0f, 6.0f }; break;
    case AttractorsSettings::Aizawa:    m_sys = { { 0, 0, 0 }, 1.9f, 0.01f, 7.0f, 0.6f }; break;
    case AttractorsSettings::Thomas:    m_sys = { { 0, 0, 0 }, 4.5f, 0.05f, 1.6f, 2.0f }; break;
    case AttractorsSettings::Halvorsen: m_sys = { { -1.5f, -1.5f, -1.5f }, 11.0f, 0.008f, 60.0f, 3.0f }; break;
    default:                            m_sys = { { 0, 0, 26 }, 32.0f, 0.004f, 170.0f, 8.0f }; break;
    }
    for (auto& q : m_particles) Respawn(q);
    // Let the cloud settle onto the attractor before it is shown.
    for (int i = 0; i < 40; ++i)
        for (auto& q : m_particles) Integrate(q, m_sys.h);
}

void AttractorsSaver::Respawn(Particle& q) {
    Rng& rng = *m_ctx.rng;
    q.p = { m_sys.centre.x + rng.Range(-m_sys.spawn, m_sys.spawn), m_sys.centre.y + rng.Range(-m_sys.spawn, m_sys.spawn),
            m_sys.centre.z + rng.Range(-m_sys.spawn, m_sys.spawn) };
    q.speed = 0.0f;
}

// ---------------------------------------------------------------- lifecycle

void AttractorsSaver::Initialize(Device& device, const SaverContext& ctx) {
    m_ctx = ctx;
    m_settings = AttractorsSettings::Load(*ctx.settings);
    m_sprites.Create(device);
    m_dot.FromImage(device, TextureFactory::SoftDot(32, 0.2f), true);
    m_particles.resize(m_settings.particles);
    m_azimuth = ctx.rng->Range(0.0f, kTwoPi);
    m_hue = ctx.rng->Float();
    SelectAttractor(m_settings.attractor == AttractorsSettings::AutoCycle ? ctx.rng->Int(0, 4) : m_settings.attractor);
    m_trail.Resize(device, m_ctx.width, m_ctx.height);
}

void AttractorsSaver::Update(float dt, double) {
    m_time += dt;
    const float k = 0.3f + 0.14f * m_settings.speed;

    // Auto-cycle: fade out, switch, fade in.
    if (m_settings.attractor == AttractorsSettings::AutoCycle) {
        m_cycleTimer += dt;
        if (m_cycleTimer > 45.0f) {
            m_fade = std::max(0.0f, m_fade - dt * 0.7f);
            if (m_fade <= 0.0f) {
                SelectAttractor((m_current + 1) % 5);
                m_cycleTimer = 0.0f;
            }
        } else {
            m_fade = std::min(1.0f, m_fade + dt * 0.5f);
        }
    }

    // Camera orbit.
    m_azimuth += dt * 0.12f * k;
    m_elevation = 0.35f * std::sin(static_cast<float>(m_time) * 0.07f) + 0.15f;
    m_hue = Wrap01(m_hue + dt * 0.02f);

    // A few RK4 substeps per frame; the step is the attractor's natural one scaled by speed.
    int substeps = 1 + static_cast<int>(k * 2.0f);
    float h = m_sys.h * k * (60.0f * dt) / substeps;
    h = std::min(h, m_sys.h * 3.0f);
    const float limit = m_sys.extent * 6.0f;
    for (auto& q : m_particles) {
        for (int s = 0; s < substeps; ++s) Integrate(q, h);
        float dx = q.p.x - m_sys.centre.x, dy = q.p.y - m_sys.centre.y, dz = q.p.z - m_sys.centre.z;
        if (!std::isfinite(q.p.x) || !std::isfinite(q.p.y) || !std::isfinite(q.p.z) || dx * dx + dy * dy + dz * dz > limit * limit) Respawn(q);
    }
}

void AttractorsSaver::Render(Device& device, SwapChain& swap) {
    if (!m_trail.Valid()) m_trail.Resize(device, m_ctx.width, m_ctx.height);
    const States& states = m_sprites.GetStates();

    // Camera on a slow orbit around the attractor's centre.
    float dist = m_sys.extent * 2.3f;
    m_camera.eye = { m_sys.centre.x + dist * std::cos(m_elevation) * std::sin(m_azimuth),
                     m_sys.centre.y + dist * std::sin(m_elevation),
                     m_sys.centre.z + dist * std::cos(m_elevation) * std::cos(m_azimuth) };
    m_camera.target = m_sys.centre;
    m_camera.up = { 0, 1, 0 };
    // Lorenz and Rossler are usually shown with z up.
    if (m_current == AttractorsSettings::Lorenz || m_current == AttractorsSettings::Rossler) {
        m_camera.eye = { m_sys.centre.x + dist * std::cos(m_elevation) * std::sin(m_azimuth),
                         m_sys.centre.y + dist * std::cos(m_elevation) * std::cos(m_azimuth),
                         m_sys.centre.z + dist * std::sin(m_elevation) };
        m_camera.up = { 0, 0, 1 };
    }
    m_camera.fovY = ToRadians(40.0f);
    m_camera.aspect = static_cast<float>(m_ctx.width) / std::max(m_ctx.height, 1);
    m_camera.nearZ = 0.1f;
    m_camera.farZ = dist * 4.0f;
    XMMATRIX viewProj = m_camera.ViewProj();

    float fade = 0.80f + 0.019f * m_settings.trail;
    m_trail.Begin(device, fade);
    const float w = static_cast<float>(m_ctx.width), h = static_cast<float>(m_ctx.height);
    const float size = 2.6f * m_ctx.dpiScale * (h / 1080.0f + 0.5f);
    const float bright = 0.16f * m_fade;
    m_sprites.Begin(m_trail.Width(), m_trail.Height());
    for (const auto& q : m_particles) {
        XMVECTOR v = XMVector3Transform(XMLoadFloat3(&q.p), viewProj);
        float pw = XMVectorGetW(v);
        if (pw <= 0.01f) continue;
        float sx = (XMVectorGetX(v) / pw * 0.5f + 0.5f) * w, sy = (0.5f - XMVectorGetY(v) / pw * 0.5f) * h;
        if (sx < -4 || sy < -4 || sx > w + 4 || sy > h + 4) continue;
        XMFLOAT4 c;
        switch (m_settings.colorMode) {
        case AttractorsSettings::ByPosition:
            c = HsvToRgb(Wrap01((q.p.z - m_sys.centre.z) / (2.0f * m_sys.extent) + m_hue), 0.85f, 1.0f);
            break;
        case AttractorsSettings::CyclingHue:
            c = HsvToRgb(m_hue, 0.8f, 1.0f);
            break;
        default: {
            float s = Saturate(q.speed / m_sys.typicalSpeed);
            c = HsvToRgb(0.66f - 0.66f * s, 0.9f, 1.0f);   // blue slow -> red fast
            break;
        }
        }
        m_sprites.Push(sx, sy, size, size, { c.x * bright, c.y * bright, c.z * bright, bright });
    }
    m_sprites.End(device, &m_dot, states.Additive());

    swap.Bind();
    swap.SetViewport(m_ctx.viewport);
    m_trail.Present(device);
}

void AttractorsSaver::Resize(int width, int height) {
    m_ctx.width = width;
    m_ctx.height = height;
    m_ctx.viewport = { 0, 0, width, height };
    m_trail = TrailBuffer{};
}
