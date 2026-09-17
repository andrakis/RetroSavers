#include "RibbonsVistaSaver.h"
#include "Gfx/Device.h"
#include "Gfx/SwapChain.h"
#include "Gfx/TextureFactory.h"
#include "Util/Color.h"
#include "Util/MathUtil.h"
#include <algorithm>
#include <cmath>

using namespace DirectX;
using namespace rs;

RibbonsVistaSettings RibbonsVistaSettings::Load(const Settings& s) {
    RibbonsVistaSettings v;
    v.count = Clamp(s.GetInt(L"Count", v.count), 1, 10);
    v.width = Clamp(s.GetInt(L"Width", v.width), 1, 10);
    v.persistence = Clamp(s.GetInt(L"Persistence", v.persistence), 1, 10);
    v.speed = Clamp(s.GetInt(L"Speed", v.speed), 1, 10);
    v.colorMode = Clamp(s.GetInt(L"ColorMode", v.colorMode), 0, 2);
    return v;
}

void RibbonsVistaSettings::Save(Settings& s) const {
    s.SetInt(L"Count", count);
    s.SetInt(L"Width", width);
    s.SetInt(L"Persistence", persistence);
    s.SetInt(L"Speed", speed);
    s.SetInt(L"ColorMode", colorMode);
}

void RibbonsVistaSaver::Initialize(Device& device, const SaverContext& ctx) {
    m_ctx = ctx;
    m_settings = RibbonsVistaSettings::Load(*ctx.settings);
    m_lines.Create(device);
    m_trail.Resize(device, m_ctx.width, m_ctx.height);
    m_ribbons.resize(m_settings.count);
    for (auto& r : m_ribbons) SpawnRibbon(r, true);
}

void RibbonsVistaSaver::SpawnRibbon(Ribbon& r, bool anywhere) {
    Rng& rng = *m_ctx.rng;
    const float w = static_cast<float>(m_ctx.width), h = static_cast<float>(m_ctx.height);
    if (anywhere) {
        r.pos = { rng.Range(w * 0.2f, w * 0.8f), rng.Range(h * 0.2f, h * 0.8f) };
        r.heading = rng.Range(0.0f, kTwoPi);
    } else {
        // Enter from a random edge, heading inwards.
        int edge = rng.Int(0, 3);
        switch (edge) {
        case 0: r.pos = { -w * 0.05f, rng.Range(0.0f, h) }; r.heading = rng.Range(-0.6f, 0.6f); break;
        case 1: r.pos = { w * 1.05f, rng.Range(0.0f, h) }; r.heading = kPi + rng.Range(-0.6f, 0.6f); break;
        case 2: r.pos = { rng.Range(0.0f, w), -h * 0.05f }; r.heading = kPi * 0.5f + rng.Range(-0.6f, 0.6f); break;
        default: r.pos = { rng.Range(0.0f, w), h * 1.05f }; r.heading = -kPi * 0.5f + rng.Range(-0.6f, 0.6f); break;
        }
    }
    r.prevPos = r.pos;
    r.twist = rng.Range(0.0f, kTwoPi);
    r.twistRate = rng.Range(0.8f, 1.6f);
    r.hue = rng.Float();
    r.hueRate = rng.Range(0.01f, 0.03f);
    r.seed = rng.Range(0.0f, 1000.0f);
    r.prevHalfWidth = 0.0f;
    r.prevNormal = { 0, 0 };
    r.hasPrev = false;
}

XMFLOAT4 RibbonsVistaSaver::Shade(const Ribbon& r, float across, float facing) const {
    XMFLOAT4 base;
    switch (m_settings.colorMode) {
    case RibbonsVistaSettings::Pastel: base = HsvToRgb(r.hue, 0.45f, 1.0f); break;
    case RibbonsVistaSettings::Single: base = HsvToRgb(static_cast<float>(r.seed / 1000.0f), 0.8f, 1.0f); break;
    default: base = HsvToRgb(r.hue, 0.85f, 1.0f); break;
    }
    // Glossy shading across the width: a highlight band on the lit side, darker on the far
    // side; the back of the ribbon (facing < 0) is a little darker and desaturated.
    float t = across * (facing >= 0.0f ? 1.0f : -1.0f);   // -1 .. 1
    float light = 0.55f + 0.45f * Saturate(0.5f + 0.5f * t);
    float highlight = std::exp(-((t - 0.35f) * (t - 0.35f)) * 12.0f) * 0.5f;
    float back = facing >= 0.0f ? 1.0f : 0.72f;
    return { Saturate(base.x * light * back + highlight), Saturate(base.y * light * back + highlight), Saturate(base.z * light * back + highlight), 1.0f };
}

void RibbonsVistaSaver::Update(float dt, double) {
    m_time += dt;
    const float k = 0.5f + 0.1f * m_settings.speed;
    const float w = static_cast<float>(m_ctx.width), h = static_cast<float>(m_ctx.height);
    const float speed = h * 0.32f * k;
    const float t = static_cast<float>(m_time);

    for (auto& r : m_ribbons) {
        r.prevPos = r.pos;
        // Steer with smooth noise; when outside the screen, bias back towards the centre.
        float steer = (TextureFactory::Fbm(t * 0.35f * k + r.seed, r.seed * 0.37f, 7u, 3) - 0.5f) * 6.0f;
        float cx = w * 0.5f - r.pos.x, cy = h * 0.5f - r.pos.y;
        float outside = std::max({ -r.pos.x, r.pos.x - w, -r.pos.y, r.pos.y - h, 0.0f }) / (0.1f * h);
        if (outside > 0.0f) {
            float toCentre = std::atan2(cy, cx);
            float diff = std::remainder(toCentre - r.heading, kTwoPi);
            steer += diff * std::min(outside, 1.0f) * 3.0f;
        }
        r.heading += steer * dt;
        r.pos.x += std::cos(r.heading) * speed * dt;
        r.pos.y += std::sin(r.heading) * speed * dt;
        r.twist += r.twistRate * k * dt;
        r.hue = Wrap01(r.hue + r.hueRate * dt);
        // A ribbon that wandered far off is replaced by one entering from an edge.
        if (outside > 4.0f) SpawnRibbon(r, false);
    }

    // Periodic wipe so the screen never stays full.
    m_wipeTimer += dt;
    float period = 30.0f + 10.0f * m_settings.persistence;
    if (m_wipeTimer > period) { m_fadeOut = 2.5f; m_wipeTimer = 0.0f; }
    if (m_fadeOut > 0.0f) m_fadeOut = std::max(0.0f, m_fadeOut - dt);
}

void RibbonsVistaSaver::Render(Device& device, SwapChain& swap) {
    if (!m_trail.Valid()) m_trail.Resize(device, m_ctx.width, m_ctx.height);
    const float h = static_cast<float>(m_ctx.height);
    const float maxHalf = h * (0.012f + 0.006f * m_settings.width);

    // Very slow decay normally; a fast fade during the wipe.
    float fade = m_fadeOut > 0.0f ? 0.93f : 0.995f + 0.0005f * m_settings.persistence;
    m_trail.Begin(device, std::min(fade, 0.9995f));
    m_lines.Begin(m_trail.Width(), m_trail.Height());
    for (auto& r : m_ribbons) {
        float dx = r.pos.x - r.prevPos.x, dy = r.pos.y - r.prevPos.y;
        float len = std::sqrt(dx * dx + dy * dy);
        if (len < 0.01f) continue;
        XMFLOAT2 n{ -dy / len, dx / len };
        float facing = std::sin(r.twist);
        float half = std::max(1.5f, maxHalf * std::fabs(facing) + maxHalf * 0.08f);
        if (r.hasPrev) {
            XMFLOAT2 a{ r.prevPos.x + r.prevNormal.x * r.prevHalfWidth, r.prevPos.y + r.prevNormal.y * r.prevHalfWidth };
            XMFLOAT2 b{ r.pos.x + n.x * half, r.pos.y + n.y * half };
            XMFLOAT2 c{ r.pos.x - n.x * half, r.pos.y - n.y * half };
            XMFLOAT2 d{ r.prevPos.x - r.prevNormal.x * r.prevHalfWidth, r.prevPos.y - r.prevNormal.y * r.prevHalfWidth };
            XMFLOAT4 cl = Shade(r, 1.0f, facing), cr = Shade(r, -1.0f, facing);
            m_lines.Quad(a, b, c, d, cl, cl, cr, cr);
            // Edge lines hide the seams between consecutive quads.
            XMFLOAT4 edge = Shade(r, 0.0f, facing);
            m_lines.Line(a.x, a.y, b.x, b.y, edge);
            m_lines.Line(d.x, d.y, c.x, c.y, edge);
        }
        r.prevNormal = n;
        r.prevHalfWidth = half;
        r.hasPrev = true;
    }
    m_lines.End(device);

    swap.Bind();
    swap.SetViewport(m_ctx.viewport);
    m_trail.Present(device);
}

void RibbonsVistaSaver::Resize(int width, int height) {
    m_ctx.width = width;
    m_ctx.height = height;
    m_ctx.viewport = { 0, 0, width, height };
    m_trail = TrailBuffer{};
    for (auto& r : m_ribbons) SpawnRibbon(r, true);
}
