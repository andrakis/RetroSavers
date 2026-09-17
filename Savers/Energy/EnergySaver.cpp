#include "EnergySaver.h"
#include "Gfx/Device.h"
#include "Gfx/SwapChain.h"
#include "Gfx/TextureFactory.h"
#include "Util/Color.h"
#include "Util/MathUtil.h"
#include <algorithm>
#include <cmath>

using namespace DirectX;
using namespace rs;

EnergySettings EnergySettings::Load(const Settings& s) {
    EnergySettings v;
    v.streamers = Clamp(s.GetInt(L"Streamers", v.streamers), 1, 10);
    v.amplitude = Clamp(s.GetInt(L"Amplitude", v.amplitude), 1, 10);
    v.speed = Clamp(s.GetInt(L"Speed", v.speed), 1, 10);
    v.tint = Clamp(s.GetInt(L"Tint", v.tint), 0, 359);
    v.bloom = s.GetBool(L"Bloom", v.bloom);
    return v;
}

void EnergySettings::Save(Settings& s) const {
    s.SetInt(L"Streamers", streamers);
    s.SetInt(L"Amplitude", amplitude);
    s.SetInt(L"Speed", speed);
    s.SetInt(L"Tint", tint);
    s.SetBool(L"Bloom", bloom);
}

void EnergySaver::Initialize(Device& device, const SaverContext& ctx) {
    m_ctx = ctx;
    m_settings = EnergySettings::Load(*ctx.settings);
    m_lines.Create(device);
    m_sprites.Create(device);
    m_time = ctx.rng->Range(0.0f, 100.0f);
    CreateTargets(device);
    Spawn();
}

void EnergySaver::CreateTargets(Device& device) {
    m_trail.Resize(device, m_ctx.width, m_ctx.height);
    int bw = std::max(m_ctx.width / 2, 8), bh = std::max(m_ctx.height / 2, 8);
    m_bloomSrc.Resize(device, bw, bh);
    m_bloomTmp.Resize(device, bw, bh);
    m_bloom.Resize(device, bw, bh);
}

void EnergySaver::Spawn() {
    Rng& rng = *m_ctx.rng;
    m_streamers.clear();
    int n = 20 + 10 * m_settings.streamers;
    for (int i = 0; i < n; ++i) {
        Streamer s;
        s.phase = rng.Range(0.0f, kTwoPi);
        s.waves = rng.Range(0.8f, 2.6f);
        s.amp = rng.Range(0.35f, 1.0f);
        s.rate = rng.Range(0.6f, 1.6f) * (rng.Chance(0.5f) ? 1.0f : -1.0f);
        s.offset = rng.Range(-0.08f, 0.08f);
        s.bright = rng.Range(0.4f, 1.0f);
        s.noise = rng.Range(0.0f, 100.0f);
        m_streamers.push_back(s);
    }
}

void EnergySaver::Update(float dt, double) {
    m_time += dt * (0.4f + 0.12f * m_settings.speed);
}

void EnergySaver::Render(Device& device, SwapChain& swap) {
    ID3D11DeviceContext* ctx = device.Ctx();
    if (!m_trail.Valid()) CreateTargets(device);
    const States& states = m_lines.GetStates();
    const float w = static_cast<float>(m_ctx.width), h = static_cast<float>(m_ctx.height);
    const float t = static_cast<float>(m_time);
    const float ampPx = h * (0.06f + 0.035f * m_settings.amplitude);
    const float hue = m_settings.tint / 360.0f;
    const XMFLOAT4 deep = HsvToRgb(hue, 0.9f, 0.9f), light = HsvToRgb(Wrap01(hue - 0.08f), 0.45f, 1.0f);
    const int segments = 96;
    const float thick = std::max(1.0f, 1.6f * m_ctx.dpiScale * (h / 1080.0f + 0.5f));

    m_trail.Begin(device, 0.76f);
    m_lines.Begin(m_trail.Width(), m_trail.Height());
    std::vector<XMFLOAT2> pts(segments + 1);
    for (const Streamer& s : m_streamers) {
        for (int i = 0; i <= segments; ++i) {
            float u = static_cast<float>(i) / segments;
            // Pinched at both edges, plus a little turbulence so the bundle breathes.
            float env = std::pow(std::sin(u * kPi), 0.6f);
            float wave = std::sin(u * s.waves * kTwoPi + s.phase + t * s.rate);
            float turb = (TextureFactory::Fbm(u * 3.0f + s.noise, t * 0.25f, 11u, 3) - 0.5f) * 0.5f;
            pts[i] = { u * w, h * (0.5f + s.offset) + ampPx * s.amp * env * (wave + turb) };
        }
        // Two passes: a wide faint glow strip and a thin bright core, both additive.
        for (int pass = 0; pass < 2; ++pass) {
            float half = pass == 0 ? thick * 3.0f : thick;
            // Dozens of streamers overlap additively: each contributes only a little.
            float k = (pass == 0 ? 0.02f : 0.11f) * s.bright;
            const XMFLOAT4& base = pass == 0 ? deep : light;
            XMFLOAT4 c{ base.x * k, base.y * k, base.z * k, 1.0f };
            for (int i = 0; i < segments; ++i) {
                const XMFLOAT2& a = pts[i];
                const XMFLOAT2& b = pts[i + 1];
                float dx = b.x - a.x, dy = b.y - a.y;
                float len = std::sqrt(dx * dx + dy * dy);
                if (len < 1e-3f) continue;
                XMFLOAT2 n{ -dy / len * half, dx / len * half };
                m_lines.Quad({ a.x + n.x, a.y + n.y }, { b.x + n.x, b.y + n.y }, { b.x - n.x, b.y - n.y }, { a.x - n.x, a.y - n.y }, c, c, c, c);
            }
        }
    }
    m_lines.End(device, states.Additive());

    if (m_settings.bloom) {
        m_bloomSrc.Bind(ctx);
        m_trail.Post().Copy(ctx, m_trail.Current().SRV());
        m_trail.Post().GaussianBlur(device, m_bloomSrc, m_bloomTmp, m_bloom, 2);
    }
    swap.Bind();
    swap.SetViewport(m_ctx.viewport);
    m_trail.Present(device);
    if (m_settings.bloom) {
        m_sprites.Begin(m_ctx.width, m_ctx.height);
        m_sprites.Push(w * 0.5f, h * 0.5f, w, h, { 0.5f, 0.5f, 0.5f, 1 });
        m_sprites.EndWithSRV(device, m_bloom.SRV(), states.Additive());
    }
}

void EnergySaver::Resize(int width, int height) {
    m_ctx.width = width;
    m_ctx.height = height;
    m_ctx.viewport = { 0, 0, width, height };
    m_trail = TrailBuffer{};
}
