#include "MarqueeSaver.h"
#include "Gfx/Device.h"
#include "Gfx/SwapChain.h"
#include "Gfx/TextureFactory.h"
#include "Host/FontSettings.h"
#include "Util/Color.h"
#include "Util/MathUtil.h"
#include <algorithm>
#include <cmath>

using namespace DirectX;
using namespace rs;

// ---------------------------------------------------------------- settings

MarqueeSettings::MarqueeSettings() : font(TextureFactory::MakeLogFont(L"Segoe UI", 72, FW_BOLD)) {}

MarqueeSettings MarqueeSettings::Load(const Settings& s) {
    MarqueeSettings v;
    v.text = s.GetString(L"Text", v.text);
    v.font = LoadFont(s, L"Font", v.font);
    v.size = Clamp(s.GetInt(L"Size", v.size), 8, 200);
    v.speed = Clamp(s.GetInt(L"Speed", v.speed), 1, 10);
    v.position = Clamp(s.GetInt(L"Position", v.position), 0, 1);
    v.textColor = static_cast<COLORREF>(s.GetInt(L"TextColor", static_cast<int>(v.textColor)));
    v.background = static_cast<COLORREF>(s.GetInt(L"Background", static_cast<int>(v.background)));
    v.mirror = s.GetBool(L"Mirror", v.mirror);
    return v;
}

void MarqueeSettings::Save(Settings& s) const {
    s.SetString(L"Text", text);
    SaveFont(s, L"Font", font);
    s.SetInt(L"Size", size);
    s.SetInt(L"Speed", speed);
    s.SetInt(L"Position", position);
    s.SetInt(L"TextColor", static_cast<int>(textColor));
    s.SetInt(L"Background", static_cast<int>(background));
    s.SetBool(L"Mirror", mirror);
}

// ---------------------------------------------------------------- lifecycle

void MarqueeSaver::Initialize(Device& device, const SaverContext& ctx) {
    m_ctx = ctx;
    m_settings = MarqueeSettings::Load(*ctx.settings);
    m_sprites.Create(device);
    BuildText(device);
    NewPass();
    // First pass starts part-way across so the screen is not empty for the first seconds.
    m_x = m_ctx.width * 0.6f;
}

void MarqueeSaver::BuildText(Device& device) {
    std::wstring text = m_settings.text.empty() ? L"RetroSavers" : m_settings.text;
    LOGFONTW lf = m_settings.font;
    lf.lfHeight = -static_cast<LONG>(std::max(8.0f, m_settings.size * m_ctx.dpiScale));
    Image img = TextureFactory::TextImage(text, lf, m_settings.textColor, 4);
    m_textH = static_cast<float>(img.height);
    m_totalW = static_cast<float>(img.width);

    // Long strings exceed the texture width limit: cut into column chunks.
    m_chunks.clear();
    for (int x0 = 0; x0 < img.width; x0 += kMaxChunk) {
        int w = std::min(kMaxChunk, img.width - x0);
        Image part(w, img.height, 0);
        for (int y = 0; y < img.height; ++y)
            for (int x = 0; x < w; ++x) part.At(x, y) = img.At(x0 + x, y);
        Chunk c;
        c.texture.FromImage(device, part, false);
        c.offset = static_cast<float>(x0);
        c.width = static_cast<float>(w);
        m_chunks.push_back(std::move(c));
    }
}

void MarqueeSaver::NewPass() {
    m_x = static_cast<float>(m_ctx.width);
    float room = std::max(0.0f, m_ctx.height - m_textH);
    m_y = m_settings.position == MarqueeSettings::Centred ? room * 0.5f : m_ctx.rng->Range(0.0f, room);
}

void MarqueeSaver::Update(float dt, double) {
    float pixelsPerSec = (60.0f + 45.0f * m_settings.speed) * (m_ctx.height / 1080.0f) * m_ctx.dpiScale;
    m_x -= pixelsPerSec * dt;
    if (m_x + m_totalW < 0.0f) NewPass();
}

std::optional<XMFLOAT4> MarqueeSaver::ClearColor() const {
    return FromColorRef(m_settings.background);
}

void MarqueeSaver::Render(Device& device, SwapChain&) {
    const float x = std::floor(m_x), y = std::floor(m_y);
    for (const Chunk& c : m_chunks) {
        // Mirrored text reads right-to-left: flip each chunk and lay them out from the far end.
        float left = m_settings.mirror ? x + (m_totalW - c.offset - c.width) : x + c.offset;
        XMFLOAT4 uv = m_settings.mirror ? XMFLOAT4{ 1, 0, 0, 1 } : XMFLOAT4{ 0, 0, 1, 1 };
        m_sprites.Begin(m_ctx.width, m_ctx.height);
        m_sprites.Push(left + c.width * 0.5f, y + m_textH * 0.5f, c.width, m_textH, { 1, 1, 1, 1 }, uv);
        m_sprites.End(device, &c.texture, m_sprites.GetStates().AlphaBlend());
    }
}

void MarqueeSaver::Resize(int width, int height) {
    m_ctx.width = width;
    m_ctx.height = height;
    m_ctx.viewport = { 0, 0, width, height };
    NewPass();
}
