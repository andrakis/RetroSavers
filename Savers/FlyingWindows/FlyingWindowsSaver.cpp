#include "FlyingWindowsSaver.h"
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

FlyingWindowsSettings FlyingWindowsSettings::Load(const Settings& s) {
    FlyingWindowsSettings v;
    v.density = Clamp(s.GetInt(L"Density", v.density), 10, 200);
    v.warpSpeed = Clamp(s.GetInt(L"WarpSpeed", v.warpSpeed), 1, 10);
    v.imagePath = s.GetString(L"ImagePath", L"");
    static const wchar_t* names[4] = { L"Pane1", L"Pane2", L"Pane3", L"Pane4" };
    for (int i = 0; i < 4; ++i) v.pane[i] = static_cast<COLORREF>(s.GetInt(names[i], static_cast<int>(v.pane[i])));
    v.spin = s.GetBool(L"Spin", v.spin);
    v.stars = s.GetBool(L"Stars", v.stars);
    return v;
}

void FlyingWindowsSettings::Save(Settings& s) const {
    s.SetInt(L"Density", density);
    s.SetInt(L"WarpSpeed", warpSpeed);
    s.SetString(L"ImagePath", imagePath);
    static const wchar_t* names[4] = { L"Pane1", L"Pane2", L"Pane3", L"Pane4" };
    for (int i = 0; i < 4; ++i) s.SetInt(names[i], static_cast<int>(pane[i]));
    s.SetBool(L"Spin", spin);
    s.SetBool(L"Stars", stars);
}

// ---------------------------------------------------------------- lifecycle

void FlyingWindowsSaver::Initialize(Device& device, const SaverContext& ctx) {
    m_ctx = ctx;
    m_settings = FlyingWindowsSettings::Load(*ctx.settings);
    m_sprites.Create(device);
    LoadEmblem(device);

    m_items.resize(m_settings.density);
    for (auto& it : m_items) Respawn(it, true, kEmblemSpread);
    m_stars.resize(m_settings.stars ? 120 : 0);
    for (auto& s : m_stars) Respawn(s, true, 1.0f);
}

void FlyingWindowsSaver::LoadEmblem(Device& device) {
    std::optional<Image> img;
    if (!m_settings.imagePath.empty()) img = ImageLoader::Load(m_settings.imagePath, 1024);
    if (!img) {
        uint32_t colors[4];
        for (int i = 0; i < 4; ++i) colors[i] = PackRgba(GetRValue(m_settings.pane[i]), GetGValue(m_settings.pane[i]), GetBValue(m_settings.pane[i]));
        img = TextureFactory::Emblem(256, colors);
    }
    m_emblem.FromImage(device, *img, true);
    m_aspect = static_cast<float>(img->width) / std::max(img->height, 1);
}

void FlyingWindowsSaver::Respawn(Item& it, bool anywhere, float spread) {
    Rng& rng = *m_ctx.rng;
    it.x = rng.Range(-spread, spread);
    it.y = rng.Range(-spread, spread);
    it.z = anywhere ? rng.Range(0.1f, 1.0f) : 1.0f;
    it.angle = m_settings.spin ? rng.Range(0.0f, kTwoPi) : 0.0f;
    it.spin = m_settings.spin ? rng.Range(-1.5f, 1.5f) : 0.0f;
}

void FlyingWindowsSaver::Update(float dt, double) {
    const float speed = 0.05f * m_settings.warpSpeed;
    const float cx = m_ctx.width * 0.5f, cy = m_ctx.height * 0.5f;
    auto advance = [&](std::vector<Item>& list, float k, float spread) {
        for (auto& it : list) {
            it.z -= speed * k * dt;
            it.angle += it.spin * dt;
            if (it.z <= 0.03f) { Respawn(it, false, spread); continue; }
            float sx = cx + (it.x / it.z) * cx, sy = cy + (it.y / it.z) * cx;
            float margin = 0.6f * m_ctx.height / it.z;   // generous: the sprite is centred there
            if (sx < -margin || sy < -margin || sx > m_ctx.width + margin || sy > m_ctx.height + margin) Respawn(it, false, spread);
        }
    };
    advance(m_items, 1.0f, kEmblemSpread);
    advance(m_stars, 0.35f, 1.0f);
}

void FlyingWindowsSaver::Render(Device& device, SwapChain&) {
    const States& states = m_sprites.GetStates();
    const float cx = m_ctx.width * 0.5f, cy = m_ctx.height * 0.5f;

    if (!m_stars.empty()) {
        m_sprites.Begin(m_ctx.width, m_ctx.height);
        for (const auto& s : m_stars) {
            float sx = cx + (s.x / s.z) * cx, sy = cy + (s.y / s.z) * cx;
            float depth = 1.0f - s.z;
            float size = std::floor((1.0f + 1.5f * depth) * m_ctx.dpiScale + 0.5f);
            float b = 0.25f + 0.55f * depth;
            m_sprites.Push(std::floor(sx), std::floor(sy), size, size, { b, b, b, 1 });
        }
        m_sprites.End(device, nullptr, states.Opaque());
    }

    // Far emblems first so the near ones overlap them.
    m_order.clear();
    for (const auto& it : m_items) m_order.push_back(&it);
    std::sort(m_order.begin(), m_order.end(), [](const Item* a, const Item* b) { return a->z > b->z; });

    const float base = 0.045f * m_ctx.height;   // emblem height at z = 1
    m_sprites.Begin(m_ctx.width, m_ctx.height);
    for (const Item* it : m_order) {
        float sx = cx + (it->x / it->z) * cx, sy = cy + (it->y / it->z) * cx;
        float h = std::min(base / it->z, 2.5f * m_ctx.height);
        float w = h * m_aspect;
        // Fade in out of the vanishing point.
        float a = Smoothstep(1.0f, 0.85f, it->z);
        m_sprites.Push(sx, sy, w, h, { 1, 1, 1, a }, { 0, 0, 1, 1 }, it->angle);
    }
    m_sprites.End(device, &m_emblem, states.AlphaBlend());
}

void FlyingWindowsSaver::Resize(int width, int height) {
    m_ctx.width = width;
    m_ctx.height = height;
    m_ctx.viewport = { 0, 0, width, height };
}
