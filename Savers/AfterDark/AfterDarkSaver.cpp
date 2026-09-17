#include "AfterDarkSaver.h"
#include "Gfx/Device.h"
#include "Gfx/SwapChain.h"
#include "Gfx/TextureFactory.h"
#include "Host/Host.h"
#include "Util/Color.h"
#include "Util/MathUtil.h"
#include "Shaders/RainRipple_ps.h"
#include <algorithm>
#include <cmath>

using namespace DirectX;
using namespace rs;

// ---------------------------------------------------------------- settings

AfterDarkSettings AfterDarkSettings::Load(const Settings& s) {
    AfterDarkSettings v;
    v.mode = Clamp(s.GetInt(L"Mode", v.mode), 0, 3);
    v.density = Clamp(s.GetInt(L"Density", v.density), 1, 10);
    v.speed = Clamp(s.GetInt(L"Speed", v.speed), 1, 10);
    v.rainOnDesktop = s.GetBool(L"RainOnDesktop", v.rainOnDesktop);
    return v;
}

void AfterDarkSettings::Save(Settings& s) const {
    s.SetInt(L"Mode", mode);
    s.SetInt(L"Density", density);
    s.SetInt(L"Speed", speed);
    s.SetBool(L"RainOnDesktop", rainOnDesktop);
}

// ---------------------------------------------------------------- lifecycle

void AfterDarkSaver::Initialize(Device& device, const SaverContext& ctx) {
    m_ctx = ctx;
    m_settings = AfterDarkSettings::Load(*ctx.settings);
    m_sprites.Create(device);
    m_lines.Create(device);
    m_post.Create(device);
    m_dot.FromImage(device, TextureFactory::SoftDot(32, 0.3f), true);
    m_time = ctx.rng->Range(0.0f, 100.0f);
    SetupMode(device);
}

void AfterDarkSaver::SetupMode(Device& device) {
    Rng& rng = *m_ctx.rng;
    const float w = static_cast<float>(m_ctx.width), h = static_cast<float>(m_ctx.height);
    m_stars.clear(); m_buildings.clear(); m_meteors.clear(); m_warp.clear(); m_drops.clear(); m_rings.clear(); m_flyers.clear();
    switch (m_settings.mode) {
    case AfterDarkSettings::StarryNight: {
        int n = 120 + 60 * m_settings.density;
        for (int i = 0; i < n; ++i)
            m_stars.push_back({ rng.Range(0.0f, w), rng.Range(0.0f, h * 0.8f), rng.Range(1.0f, 2.6f), rng.Range(0.0f, kTwoPi), rng.Range(0.5f, 3.0f), rng.Range(0.3f, 1.0f) });
        // Skyline: buildings of random width/height, each with a window grid.
        float x = -rng.Range(0.0f, 40.0f);
        while (x < w) {
            Building b;
            b.w = rng.Range(0.03f, 0.09f) * w;
            b.h = rng.Range(0.08f, 0.38f) * h;
            b.x = x;
            float cell = std::max(6.0f, 0.012f * h);
            b.cols = std::max(1, static_cast<int>(b.w / cell / 1.6f));
            b.rows = std::max(1, static_cast<int>(b.h / cell / 1.5f));
            b.lit.resize(static_cast<size_t>(b.cols) * b.rows);
            for (auto& l : b.lit) l = rng.Chance(0.35f) ? 1 : 0;
            m_buildings.push_back(b);
            x += b.w + rng.Range(0.0f, 0.01f * w);
        }
        // Crescent moon sprite.
        m_moon.FromImage(device, TextureFactory::GdiMask(128, 128, [](HDC dc) {
            HBRUSH white = CreateSolidBrush(RGB(255, 255, 255)), black = CreateSolidBrush(RGB(0, 0, 0));
            HGDIOBJ old = SelectObject(dc, white);
            SelectObject(dc, GetStockObject(NULL_PEN));
            Ellipse(dc, 12, 12, 116, 116);
            SelectObject(dc, black);
            Ellipse(dc, 36, 4, 128, 108);
            SelectObject(dc, old);
            DeleteObject(white); DeleteObject(black);
        }, RGB(255, 245, 200)), true);
        break;
    }
    case AfterDarkSettings::Warp: {
        int n = 80 + 60 * m_settings.density;
        for (int i = 0; i < n; ++i) m_warp.push_back({ rng.Range(-1.0f, 1.0f), rng.Range(-1.0f, 1.0f), rng.Range(0.05f, 1.0f), rng.Float() });
        m_trail.Resize(device, m_ctx.width, m_ctx.height);
        break;
    }
    case AfterDarkSettings::Rain: {
        ThrowIfFailed(device.Get()->CreatePixelShader(g_RainRipple_ps, sizeof(g_RainRipple_ps), nullptr, &m_ripplePs), "CreatePixelShader(RainRipple)");
        m_rippleCb.Create(device);
        const Image* desk = m_settings.rainOnDesktop ? Host::DesktopImage() : nullptr;
        if (desk) {
            Image crop(m_ctx.width, m_ctx.height, 0xFF000000u);
            for (int y = 0; y < m_ctx.height; ++y) {
                int sy = m_ctx.viewport.top + y;
                if (sy < 0 || sy >= desk->height) continue;
                for (int xx = 0; xx < m_ctx.width; ++xx) {
                    int sx = m_ctx.viewport.left + xx;
                    if (sx < 0 || sx >= desk->width) continue;
                    crop.At(xx, y) = desk->At(sx, sy);
                }
            }
            m_desktop.FromImage(device, crop, false);
            m_hasDesktop = true;
        }
        break;
    }
    default: {
        m_toaster.FromImage(device, TextureFactory::Toaster(), true);
        m_toast.FromImage(device, TextureFactory::Toast(), true);
        int n = 4 + 2 * m_settings.density;
        m_flyers.resize(n);
        for (auto& f : m_flyers) SpawnFlyer(f, true);
        break;
    }
    }
}

std::optional<XMFLOAT4> AfterDarkSaver::ClearColor() const {
    switch (m_settings.mode) {
    case AfterDarkSettings::StarryNight: return XMFLOAT4{ 0.01f, 0.01f, 0.05f, 1 };
    case AfterDarkSettings::Warp: return std::nullopt;
    case AfterDarkSettings::Rain: return std::nullopt;
    default: return XMFLOAT4{ 0, 0, 0, 1 };
    }
}

// ---------------------------------------------------------------- Starry Night

void AfterDarkSaver::UpdateStarry(float dt) {
    Rng& rng = *m_ctx.rng;
    const float w = static_cast<float>(m_ctx.width), h = static_cast<float>(m_ctx.height);
    const float k = 0.5f + 0.1f * m_settings.speed;
    m_meteorTimer -= dt * k;
    if (m_meteorTimer <= 0.0f) {
        float ang = rng.Range(0.5f, 1.0f);
        m_meteors.push_back({ rng.Range(0.0f, w), rng.Range(0.0f, h * 0.3f), std::cos(ang) * h * 0.9f * (rng.Chance(0.5f) ? 1 : -1), std::sin(ang) * h * 0.9f, 1.0f });
        m_meteorTimer = rng.Range(4.0f, 12.0f);
    }
    for (auto& m : m_meteors) { m.x += m.vx * dt; m.y += m.vy * dt; m.life -= dt * 1.2f; }
    m_meteors.erase(std::remove_if(m_meteors.begin(), m_meteors.end(), [](const Meteor& m) { return m.life <= 0.0f; }), m_meteors.end());
    // A few windows change every so often.
    m_windowTimer -= dt * k;
    if (m_windowTimer <= 0.0f && !m_buildings.empty()) {
        for (int i = 0; i < 3; ++i) {
            Building& b = m_buildings[rng.Int(0, static_cast<int>(m_buildings.size()) - 1)];
            if (!b.lit.empty()) { uint8_t& l = b.lit[rng.Int(0, static_cast<int>(b.lit.size()) - 1)]; l = l ? 0 : 1; }
        }
        m_windowTimer = rng.Range(0.2f, 0.8f);
    }
}

void AfterDarkSaver::RenderStarry(Device& device) {
    const States& states = m_sprites.GetStates();
    const float w = static_cast<float>(m_ctx.width), h = static_cast<float>(m_ctx.height);
    const float t = static_cast<float>(m_time);
    const float s = m_ctx.dpiScale * (h / 1080.0f + 0.5f);
    // Sky gradient (a tall thin quad pair via the line renderer), stars, moon, meteors.
    m_lines.Begin(m_ctx.width, m_ctx.height);
    m_lines.Quad({ 0, 0 }, { w, 0 }, { w, h }, { 0, h }, { 0.0f, 0.0f, 0.03f, 1 }, { 0.0f, 0.0f, 0.03f, 1 }, { 0.05f, 0.04f, 0.14f, 1 }, { 0.05f, 0.04f, 0.14f, 1 });
    m_lines.End(device);
    m_sprites.Begin(m_ctx.width, m_ctx.height);
    for (const auto& st : m_stars) {
        float tw = 0.55f + 0.45f * std::sin(t * st.rate + st.phase);
        float b = st.bright * tw;
        m_sprites.Push(st.x, st.y, st.size * 2.2f * s, st.size * 2.2f * s, { b, b, b * 1.05f, b });
    }
    for (const auto& m : m_meteors) {
        for (int i = 0; i < 12; ++i) {
            float back = i * 0.012f;
            float a = m.life * (1.0f - i / 12.0f);
            m_sprites.Push(m.x - m.vx * back, m.y - m.vy * back, (5.0f - i * 0.3f) * s, (5.0f - i * 0.3f) * s, { a, a, a, a });
        }
    }
    m_sprites.End(device, &m_dot, states.Additive());
    if (m_moon.Valid()) {
        float ms = 0.09f * h;
        m_sprites.Begin(m_ctx.width, m_ctx.height);
        m_sprites.Push(w * 0.8f, h * 0.18f, ms, ms, { 1, 1, 1, 1 });
        m_sprites.End(device, &m_moon, states.AlphaBlend());
    }
    // Skyline and windows.
    m_lines.Begin(m_ctx.width, m_ctx.height);
    const XMFLOAT4 wall{ 0.03f, 0.03f, 0.05f, 1 }, lit{ 1.0f, 0.85f, 0.45f, 1 }, dark{ 0.08f, 0.08f, 0.1f, 1 };
    for (const auto& b : m_buildings) {
        float top = h - b.h;
        m_lines.Quad({ b.x, top }, { b.x + b.w, top }, { b.x + b.w, h }, { b.x, h }, wall, wall, wall, wall);
        float cw = b.w / (b.cols + 0.5f), ch = b.h / (b.rows + 1.0f);
        for (int r = 0; r < b.rows; ++r)
            for (int c = 0; c < b.cols; ++c) {
                float x0 = b.x + cw * (c + 0.35f), y0 = top + ch * (r + 0.5f);
                const XMFLOAT4& col = b.lit[static_cast<size_t>(r) * b.cols + c] ? lit : dark;
                m_lines.Quad({ x0, y0 }, { x0 + cw * 0.45f, y0 }, { x0 + cw * 0.45f, y0 + ch * 0.5f }, { x0, y0 + ch * 0.5f }, col, col, col, col);
            }
    }
    m_lines.End(device);
}

// ---------------------------------------------------------------- Warp

void AfterDarkSaver::UpdateWarp(float dt) {
    Rng& rng = *m_ctx.rng;
    const float speed = 0.06f * m_settings.speed;
    const float cx = m_ctx.width * 0.5f, cy = m_ctx.height * 0.5f;
    for (auto& s : m_warp) {
        s.z -= speed * dt;
        float sx = cx + s.x / s.z * cx, sy = cy + s.y / s.z * cx;
        if (s.z <= 0.02f || sx < -10 || sy < -10 || sx > m_ctx.width + 10 || sy > m_ctx.height + 10) {
            s = { rng.Range(-1.0f, 1.0f), rng.Range(-1.0f, 1.0f), 1.0f, rng.Float() };
        }
    }
}

void AfterDarkSaver::RenderWarp(Device& device, SwapChain& swap) {
    if (!m_trail.Valid()) m_trail.Resize(device, m_ctx.width, m_ctx.height);
    const States& states = m_sprites.GetStates();
    const float cx = m_ctx.width * 0.5f, cy = m_ctx.height * 0.5f;
    const float s = m_ctx.dpiScale * (m_ctx.height / 1080.0f + 0.5f);
    m_trail.Begin(device, 0.86f);
    m_sprites.Begin(m_trail.Width(), m_trail.Height());
    for (const auto& st : m_warp) {
        float sx = cx + st.x / st.z * cx, sy = cy + st.y / st.z * cx;
        float depth = 1.0f - st.z;
        float size = (1.5f + 4.0f * depth * depth) * s;
        XMFLOAT4 c = HsvToRgb(st.hue, 0.5f, 1.0f);
        float b = 0.25f + 0.75f * depth;
        m_sprites.Push(sx, sy, size, size, { c.x * b, c.y * b, c.z * b, b });
    }
    m_sprites.End(device, &m_dot, states.Additive());
    swap.Bind();
    swap.SetViewport(m_ctx.viewport);
    m_trail.Present(device);
}

// ---------------------------------------------------------------- Rain

void AfterDarkSaver::UpdateRain(float dt) {
    Rng& rng = *m_ctx.rng;
    const float w = static_cast<float>(m_ctx.width), h = static_cast<float>(m_ctx.height);
    const float k = 0.5f + 0.1f * m_settings.speed;
    m_dropTimer -= dt * k;
    while (m_dropTimer <= 0.0f) {
        m_drops.push_back({ rng.Range(0.0f, w), -10.0f, h * rng.Range(1.6f, 2.4f), rng.Range(h * 0.1f, h * 0.95f) });
        m_dropTimer += 1.0f / (2.0f + 2.5f * m_settings.density);
    }
    for (auto& d : m_drops) d.y += d.vy * dt * k;
    for (size_t i = 0; i < m_drops.size();) {
        if (m_drops[i].y >= m_drops[i].targetY) {
            if (m_rings.size() < 32) m_rings.push_back({ m_drops[i].x, m_drops[i].targetY, 0.0f });
            m_drops[i] = m_drops.back();
            m_drops.pop_back();
        } else ++i;
    }
    for (auto& r : m_rings) r.age += dt * k;
    m_rings.erase(std::remove_if(m_rings.begin(), m_rings.end(), [](const Ring& r) { return r.age > 2.2f; }), m_rings.end());
}

void AfterDarkSaver::RenderRain(Device& device) {
    ID3D11DeviceContext* ctx = device.Ctx();
    const States& states = m_sprites.GetStates();
    const float w = static_cast<float>(m_ctx.width), h = static_cast<float>(m_ctx.height);
    // Background: desktop refracted by the rings (or a dark gradient with the same rings).
    RippleCB cb{};
    cb.info = { static_cast<float>(m_rings.size()), w / h, m_hasDesktop ? 1.0f : 0.0f, 0 };
    for (size_t i = 0; i < m_rings.size() && i < 32; ++i) {
        const Ring& r = m_rings[i];
        cb.rings[i] = { r.x / w, r.y / h, r.age * 0.12f, std::max(0.0f, 1.0f - r.age / 2.2f) };
    }
    m_rippleCb.Update(ctx, cb);
    m_rippleCb.BindPS(ctx, 0);
    states.Set2D(ctx, states.Opaque());
    ID3D11SamplerState* samp = states.LinearClamp();
    ctx->PSSetSamplers(0, 1, &samp);
    if (m_hasDesktop) m_desktop.BindPS(ctx, 0);
    m_post.Draw(ctx, m_ripplePs.Get());
    ID3D11ShaderResourceView* null = nullptr;
    ctx->PSSetShaderResources(0, 1, &null);
    // Falling drops as short streaks.
    m_lines.Begin(m_ctx.width, m_ctx.height);
    for (const auto& d : m_drops) m_lines.Line(d.x, d.y - 0.02f * h, d.x, d.y, { 0.7f, 0.8f, 1.0f, 0.8f });
    m_lines.End(device, states.AlphaBlend());
}

// ---------------------------------------------------------------- Toasters

void AfterDarkSaver::SpawnFlyer(Flyer& f, bool anywhere) {
    Rng& rng = *m_ctx.rng;
    const float w = static_cast<float>(m_ctx.width), h = static_cast<float>(m_ctx.height);
    f.depth = rng.Range(0.35f, 1.0f);         // 1 = nearest
    f.speed = (0.08f + 0.06f * m_settings.speed) * h * f.depth;
    f.flap = rng.Range(0.0f, kTwoPi);
    f.tumble = rng.Range(0.0f, kTwoPi);
    f.toast = rng.Chance(0.35f);
    if (anywhere) { f.x = rng.Range(0.0f, w * 1.3f); f.y = rng.Range(-h * 0.3f, h); }
    else if (rng.Chance(0.5f)) { f.x = w + 0.15f * h; f.y = rng.Range(-h * 0.2f, h * 0.8f); }
    else { f.x = rng.Range(w * 0.2f, w + 0.15f * h); f.y = -0.15f * h; }
}

void AfterDarkSaver::UpdateToasters(float dt) {
    const float h = static_cast<float>(m_ctx.height);
    for (auto& f : m_flyers) {
        f.x -= f.speed * dt;
        f.y += f.speed * 0.75f * dt;
        f.flap += dt * 9.0f;
        f.tumble += dt * 2.0f;
        if (f.x < -0.2f * h || f.y > h + 0.2f * h) SpawnFlyer(f, false);
    }
}

void AfterDarkSaver::RenderToasters(Device& device) {
    const States& states = m_sprites.GetStates();
    const float h = static_cast<float>(m_ctx.height);
    std::vector<const Flyer*> order;
    for (const auto& f : m_flyers) order.push_back(&f);
    std::sort(order.begin(), order.end(), [](const Flyer* a, const Flyer* b) { return a->depth < b->depth; });   // far first
    // Toasters and toast use different textures: two batches, each in depth order.
    for (int pass = 0; pass < 2; ++pass) {
        m_sprites.Begin(m_ctx.width, m_ctx.height);
        for (const Flyer* f : order) {
            if (f->toast != (pass == 1)) continue;
            float shade = 0.45f + 0.55f * f->depth;
            if (f->toast) {
                float sz = 0.08f * h * f->depth;
                m_sprites.Push(f->x, f->y, sz, sz, { shade, shade, shade, 1 }, { 0, 0, 1, 1 }, f->tumble);
            } else {
                int frame = static_cast<int>(std::floor(f->flap / kTwoPi * 4.0f)) & 3;
                float u0 = frame * 0.25f;
                float sw = 0.24f * h * f->depth, sh = sw * 112.0f / 128.0f;
                m_sprites.Push(f->x, f->y, sw, sh, { shade, shade, shade, 1 }, { u0, 0, u0 + 0.25f, 1 });
            }
        }
        m_sprites.End(device, pass == 0 ? &m_toaster : &m_toast, states.AlphaBlend());
    }
}

// ---------------------------------------------------------------- dispatch

void AfterDarkSaver::Update(float dt, double) {
    m_time += dt;
    switch (m_settings.mode) {
    case AfterDarkSettings::StarryNight: UpdateStarry(dt); break;
    case AfterDarkSettings::Warp: UpdateWarp(dt); break;
    case AfterDarkSettings::Rain: UpdateRain(dt); break;
    default: UpdateToasters(dt); break;
    }
}

void AfterDarkSaver::Render(Device& device, SwapChain& swap) {
    if (m_pendingSetup) { SetupMode(device); m_pendingSetup = false; }
    switch (m_settings.mode) {
    case AfterDarkSettings::StarryNight: RenderStarry(device); break;
    case AfterDarkSettings::Warp: RenderWarp(device, swap); break;
    case AfterDarkSettings::Rain: RenderRain(device); break;
    default: RenderToasters(device); break;
    }
}

void AfterDarkSaver::Resize(int width, int height) {
    m_ctx.width = width;
    m_ctx.height = height;
    m_ctx.viewport = { 0, 0, width, height };
    m_trail = TrailBuffer{};
    if (m_settings.mode != AfterDarkSettings::Rain) {
        // Re-lay out the mode for the new size (the desktop crop only exists in run mode).
        m_stars.clear(); m_buildings.clear(); m_warp.clear(); m_flyers.clear();
        m_pendingSetup = true;
    }
}
