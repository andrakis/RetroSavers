#include "BeziersSaver.h"
#include "Gfx/Device.h"
#include "Gfx/SwapChain.h"
#include "Util/Color.h"
#include "Util/MathUtil.h"

using namespace DirectX;
using namespace rs;

BeziersSettings BeziersSettings::Load(const Settings& s) {
    BeziersSettings v;
    v.curves = Clamp(s.GetInt(L"Curves", 4), 1, 10);
    v.trail = Clamp(s.GetInt(L"Trail", 30), 1, 100);
    v.speed = Clamp(s.GetInt(L"Speed", 5), 1, 10);
    return v;
}

void BeziersSettings::Save(Settings& s) const {
    s.SetInt(L"Curves", curves);
    s.SetInt(L"Trail", trail);
    s.SetInt(L"Speed", speed);
}

void BeziersSaver::Initialize(Device& device, const SaverContext& ctx) {
    m_ctx = ctx;
    m_settings = BeziersSettings::Load(*ctx.settings);
    m_lines.Create(device);
    Reset();
}

void BeziersSaver::Reset() {
    Rng& rng = *m_ctx.rng;
    float w = static_cast<float>(m_ctx.width), h = static_cast<float>(m_ctx.height);
    float scale = (h / 480.0f) * (0.4f + 0.16f * m_settings.speed);
    m_control.clear();
    for (int i = 0; i < m_settings.curves * 3; ++i) {
        Point p;
        p.pos = { rng.Range(0, w), rng.Range(0, h) };
        float speed = rng.Range(2.0f, 7.0f) * scale;
        p.vel = { (rng.Chance(0.5f) ? 1.0f : -1.0f) * speed * rng.Range(0.5f, 1.0f), (rng.Chance(0.5f) ? 1.0f : -1.0f) * speed * rng.Range(0.5f, 1.0f) };
        m_control.push_back(p);
    }
    m_trail.clear();
    m_hue = rng.Float();
}

void BeziersSaver::Tick() {
    float w = static_cast<float>(m_ctx.width), h = static_cast<float>(m_ctx.height);
    for (auto& p : m_control) {
        p.pos.x += p.vel.x;
        p.pos.y += p.vel.y;
        if (p.pos.x < 0) { p.pos.x = -p.pos.x; p.vel.x = std::fabs(p.vel.x); }
        if (p.pos.x > w - 1) { p.pos.x = 2 * (w - 1) - p.pos.x; p.vel.x = -std::fabs(p.vel.x); }
        if (p.pos.y < 0) { p.pos.y = -p.pos.y; p.vel.y = std::fabs(p.vel.y); }
        if (p.pos.y > h - 1) { p.pos.y = 2 * (h - 1) - p.pos.y; p.vel.y = -std::fabs(p.vel.y); }
    }

    // Segment i: anchor[i], handle[i], handle'[i], anchor[i+1] (wrapping) -> a closed loop.
    const int n = m_settings.curves;
    Snapshot snap;
    snap.points.reserve(n * kSubdivisions + 1);
    for (int i = 0; i < n; ++i) {
        const XMFLOAT2& p0 = m_control[i * 3].pos;
        const XMFLOAT2& p1 = m_control[i * 3 + 1].pos;
        const XMFLOAT2& p2 = m_control[i * 3 + 2].pos;
        const XMFLOAT2& p3 = m_control[((i + 1) % n) * 3].pos;
        for (int k = 0; k < kSubdivisions; ++k) {
            float t = static_cast<float>(k) / kSubdivisions;
            float it = 1.0f - t;
            float b0 = it * it * it, b1 = 3 * t * it * it, b2 = 3 * t * t * it, b3 = t * t * t;
            snap.points.push_back({ b0 * p0.x + b1 * p1.x + b2 * p2.x + b3 * p3.x, b0 * p0.y + b1 * p1.y + b2 * p2.y + b3 * p3.y });
        }
    }
    m_hue = Wrap01(m_hue + 0.008f);
    snap.color = HsvToRgb(m_hue, 0.9f, 1.0f);
    m_trail.push_back(std::move(snap));
    while (static_cast<int>(m_trail.size()) > m_settings.trail) m_trail.pop_front();
}

void BeziersSaver::Update(float dt, double) {
    m_accumulator += dt;
    const float step = 1.0f / kTickRate;
    int ticks = 0;
    while (m_accumulator >= step && ticks < 8) {
        m_accumulator -= step;
        ++ticks;
        Tick();
    }
    if (m_accumulator > step) m_accumulator = 0.0f;
}

void BeziersSaver::Render(Device& device, SwapChain&) {
    m_lines.Begin(m_ctx.width, m_ctx.height);
    const size_t count = m_trail.size();
    size_t idx = 0;
    for (const auto& snap : m_trail) {
        // Oldest lines are dimmed slightly so the head of the trail reads as the "current" curve.
        float age = count > 1 ? static_cast<float>(idx) / static_cast<float>(count - 1) : 1.0f;
        float k = 0.45f + 0.55f * age;
        XMFLOAT4 c{ snap.color.x * k, snap.color.y * k, snap.color.z * k, 1.0f };
        m_lines.Polyline(snap.points.data(), snap.points.size(), true, c);
        ++idx;
    }
    m_lines.End(device);
}

void BeziersSaver::Resize(int width, int height) {
    m_ctx.width = width;
    m_ctx.height = height;
    m_ctx.viewport = { 0, 0, width, height };
    Reset();
}
