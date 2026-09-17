#include "DVDBounceSaver.h"
#include "Gfx/Device.h"
#include "Gfx/ImageLoader.h"
#include "Gfx/SwapChain.h"
#include "Gfx/TextureFactory.h"
#include "Util/Color.h"
#include "Util/MathUtil.h"
#include <algorithm>
#include <cmath>

using namespace DirectX;
using namespace rs;

// ---------------------------------------------------------------- settings

DVDBounceSettings DVDBounceSettings::Load(const Settings& s) {
    DVDBounceSettings v;
    v.imagePath = s.GetString(L"ImagePath", L"");
    v.size = Clamp(s.GetInt(L"Size", v.size), 5, 50);
    v.speed = Clamp(s.GetInt(L"Speed", v.speed), 1, 10);
    v.colorMode = Clamp(s.GetInt(L"ColorMode", v.colorMode), 0, 2);
    v.cornerRate = Clamp(s.GetInt(L"CornerRate", v.cornerRate), 0, 2);
    v.showCounter = s.GetBool(L"ShowCounter", v.showCounter);
    v.background = static_cast<COLORREF>(s.GetInt(L"Background", static_cast<int>(v.background)));
    return v;
}

void DVDBounceSettings::Save(Settings& s) const {
    s.SetString(L"ImagePath", imagePath);
    s.SetInt(L"Size", size);
    s.SetInt(L"Speed", speed);
    s.SetInt(L"ColorMode", colorMode);
    s.SetInt(L"CornerRate", cornerRate);
    s.SetBool(L"ShowCounter", showCounter);
    s.SetInt(L"Background", static_cast<int>(background));
}

namespace {

const XMFLOAT4 kPalette[] = {
    { 1.0f, 0.15f, 0.15f, 1 }, { 1.0f, 0.55f, 0.1f, 1 }, { 1.0f, 0.95f, 0.2f, 1 }, { 0.2f, 0.9f, 0.3f, 1 },
    { 0.2f, 0.85f, 1.0f, 1 }, { 0.3f, 0.4f, 1.0f, 1 }, { 0.7f, 0.3f, 1.0f, 1 }, { 1.0f, 0.4f, 0.8f, 1 }, { 1, 1, 1, 1 },
};

// Round-trip periods are p*tau and q*tau with p, q coprime and p/q near the screen aspect, so the
// path runs at roughly 45 degrees and passes through a corner every p*q*tau/2 seconds.
void CornerRatio(int rate, int& p, int& q) {
    switch (rate) {
    case DVDBounceSettings::Frequent: p = 3; q = 2; break;
    case DVDBounceSettings::Rare: p = 12; q = 7; break;
    default: p = 7; q = 4; break;
    }
}

// Triangle wave 0 -> 1 -> 0 over one period.
float Tri(double t, float period, int& segment) {
    double phase = t / period;
    double half = std::floor(phase * 2.0);
    segment = static_cast<int>(half);
    double f = phase * 2.0 - half;   // 0..1 within the half period
    return static_cast<float>((segment & 1) ? 1.0 - f : f);
}

} // namespace

// ---------------------------------------------------------------- lifecycle

void DVDBounceSaver::Initialize(Device& device, const SaverContext& ctx) {
    m_ctx = ctx;
    m_device = &device;
    m_settings = DVDBounceSettings::Load(*ctx.settings);
    m_sprites.Create(device);
    LoadLogo(device);
    Layout();
    m_paletteIndex = ctx.rng->Int(0, static_cast<int>(std::size(kPalette)) - 1);
    NextColor();
}

void DVDBounceSaver::LoadLogo(Device& device) {
    std::optional<Image> img;
    if (!m_settings.imagePath.empty()) img = ImageLoader::Load(m_settings.imagePath, 2048);
    if (!img) img = TextureFactory::DiscLogo(L"RETRO", 512, 256);
    m_logo.FromImage(device, *img, true);
}

void DVDBounceSaver::Layout() {
    Rng& rng = *m_ctx.rng;
    const float w = static_cast<float>(m_ctx.width), h = static_cast<float>(m_ctx.height);
    m_spriteH = std::max(8.0f, h * m_settings.size / 100.0f);
    m_spriteW = m_spriteH * m_logo.Width() / std::max(m_logo.Height(), 1);
    if (m_spriteW > w * 0.9f) { m_spriteW = w * 0.9f; m_spriteH = m_spriteW * m_logo.Height() / std::max(m_logo.Width(), 1); }
    m_travelW = std::max(w - m_spriteW, 1.0f);
    m_travelH = std::max(h - m_spriteH, 1.0f);

    int p = 7, q = 4;
    CornerRatio(m_settings.cornerRate, p, q);
    // Speed in pixels/second, scaled with the screen so the crossing time is what varies.
    float pixelsPerSec = (60.0f + 40.0f * m_settings.speed) * (h / 1080.0f);
    float vx = 2.0f * m_travelW / p, vy = 2.0f * m_travelH / q;   // per tau
    float tau = std::sqrt(vx * vx + vy * vy) / pixelsPerSec;
    m_periodX = p * tau;
    m_periodY = q * tau;
    // Start somewhere along the cycle rather than in a corner.
    m_time = rng.Range(0.0f, 0.5f * p * q * tau);
    Tri(m_time, m_periodX, m_segX);
    Tri(m_time, m_periodY, m_segY);
    m_lastHitX = m_lastHitY = -10;
    m_flash = 0.0f;
    m_counterDirty = true;
}

void DVDBounceSaver::NextColor() {
    Rng& rng = *m_ctx.rng;
    switch (m_settings.colorMode) {
    case DVDBounceSettings::Random:
        m_color = HsvToRgb(rng.Float(), 0.85f, 1.0f);
        break;
    case DVDBounceSettings::Palette:
        m_paletteIndex = (m_paletteIndex + 1 + rng.Int(0, static_cast<int>(std::size(kPalette)) - 2)) % static_cast<int>(std::size(kPalette));
        m_color = kPalette[m_paletteIndex];
        break;
    default:
        m_color = { 1, 1, 1, 1 };
        break;
    }
}

void DVDBounceSaver::RefreshCounter(Device& device) {
    wchar_t buf[64];
    swprintf_s(buf, L"Corner hits: %d", m_corners);
    int px = static_cast<int>(std::max(12.0f, m_ctx.height * 0.022f) * m_ctx.dpiScale);
    m_counter.FromImage(device, TextureFactory::TextImage(buf, TextureFactory::MakeLogFont(L"Segoe UI", px, FW_SEMIBOLD)), false);
    m_counterDirty = false;
}

void DVDBounceSaver::Update(float dt, double) {
    m_time += dt;
    int sx = 0, sy = 0;
    float fx = Tri(m_time, m_periodX, sx);
    float fy = Tri(m_time, m_periodY, sy);
    m_pos = { m_spriteW * 0.5f + fx * m_travelW, m_spriteH * 0.5f + fy * m_travelH };

    bool hitX = sx != m_segX, hitY = sy != m_segY;
    m_segX = sx;
    m_segY = sy;
    if (hitX) m_lastHitX = m_time;
    if (hitY) m_lastHitY = m_time;
    if (hitX || hitY) {
        NextColor();
        // The two wall hits of a corner are simultaneous by construction; allow for the frame
        // boundary landing between them.
        if (std::fabs(m_lastHitX - m_lastHitY) < 0.06) {
            ++m_corners;
            m_flash = 1.0f;
            m_counterDirty = true;
            m_lastHitX = m_lastHitY = -10;
        }
    }
    if (m_flash > 0.0f) m_flash = std::max(0.0f, m_flash - dt);
}

std::optional<XMFLOAT4> DVDBounceSaver::ClearColor() const {
    return FromColorRef(m_settings.background);
}

void DVDBounceSaver::Render(Device& device, SwapChain&) {
    const States& states = m_sprites.GetStates();
    // Logo, with a pulse and a flash to white while celebrating a corner.
    float pulse = 1.0f + 0.18f * std::sin(m_flash * kPi);
    XMFLOAT4 color = Lerp(m_color, XMFLOAT4{ 1, 1, 1, 1 }, m_flash > 0.0f ? 0.5f * std::sin(m_flash * kPi) : 0.0f);
    m_sprites.Begin(m_ctx.width, m_ctx.height);
    m_sprites.Push(std::floor(m_pos.x), std::floor(m_pos.y), m_spriteW * pulse, m_spriteH * pulse, color);
    m_sprites.End(device, &m_logo, states.AlphaBlend());

    if (m_settings.showCounter) {
        if (m_counterDirty) RefreshCounter(device);
        float margin = 12.0f * m_ctx.dpiScale;
        float w = static_cast<float>(m_counter.Width()), h = static_cast<float>(m_counter.Height());
        m_sprites.Begin(m_ctx.width, m_ctx.height);
        m_sprites.Push(margin + w * 0.5f, m_ctx.height - margin - h * 0.5f, w, h, { 0.7f, 0.7f, 0.7f, 0.8f });
        m_sprites.End(device, &m_counter, states.AlphaBlend());
    }
}

void DVDBounceSaver::Resize(int width, int height) {
    m_ctx.width = width;
    m_ctx.height = height;
    m_ctx.viewport = { 0, 0, width, height };
    Layout();
}
