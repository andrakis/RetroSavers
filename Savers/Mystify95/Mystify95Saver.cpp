#include "Mystify95Saver.h"
#include "Gfx/Device.h"
#include "Gfx/SwapChain.h"
#include "Host/Host.h"
#include "Util/Color.h"
#include "Util/MathUtil.h"

using namespace DirectX;
using namespace rs;

namespace {

void LoadShape(const Settings& s, int idx, MystifyShapeSettings& out) {
    wchar_t key[32];
    auto name = [&](const wchar_t* suffix) { swprintf_s(key, L"Shape%d%s", idx + 1, suffix); return key; };
    MystifyShapeSettings def;
    out.active = s.GetBool(name(L"Active"), def.active);
    out.lines = Clamp(s.GetInt(name(L"Lines"), def.lines), 1, 15);
    out.randomColors = s.GetBool(name(L"RandomColors"), def.randomColors);
    out.color1 = static_cast<COLORREF>(s.GetInt(name(L"Color1"), static_cast<int>(def.color1)));
    out.color2 = static_cast<COLORREF>(s.GetInt(name(L"Color2"), static_cast<int>(def.color2)));
}

void SaveShape(Settings& s, int idx, const MystifyShapeSettings& in) {
    wchar_t key[32];
    auto name = [&](const wchar_t* suffix) { swprintf_s(key, L"Shape%d%s", idx + 1, suffix); return key; };
    s.SetBool(name(L"Active"), in.active);
    s.SetInt(name(L"Lines"), in.lines);
    s.SetBool(name(L"RandomColors"), in.randomColors);
    s.SetInt(name(L"Color1"), static_cast<int>(in.color1));
    s.SetInt(name(L"Color2"), static_cast<int>(in.color2));
}

} // namespace

Mystify95Settings Mystify95Settings::Defaults() {
    Mystify95Settings v;
    v.shape[1].color1 = RGB(0, 255, 0);
    v.shape[1].color2 = RGB(255, 255, 0);
    return v;
}

Mystify95Settings Mystify95Settings::Load(const Settings& s) {
    Mystify95Settings v = Defaults();
    LoadShape(s, 0, v.shape[0]);
    LoadShape(s, 1, v.shape[1]);
    // Shape 2 has different default colours than shape 1; re-apply when the key is absent.
    if (s.GetInt(L"Shape2Color1", -1) == -1) { v.shape[1].color1 = RGB(0, 255, 0); v.shape[1].color2 = RGB(255, 255, 0); }
    v.clearScreen = s.GetBool(L"ClearScreen", true);
    return v;
}

void Mystify95Settings::Save(Settings& s) const {
    SaveShape(s, 0, shape[0]);
    SaveShape(s, 1, shape[1]);
    s.SetBool(L"ClearScreen", clearScreen);
}

void Mystify95Saver::Initialize(Device& device, const SaverContext& ctx) {
    m_ctx = ctx;
    m_device = &device;
    m_settings = Mystify95Settings::Load(*ctx.settings);
    m_lines.Create(device);
    m_post.Create(device);
    ResetShapes();
    if (!m_settings.clearScreen) InitPersistentTarget(device);
}

void Mystify95Saver::InitPersistentTarget(Device& device) {
    m_persistent.Create(device, m_ctx.width, m_ctx.height, DXGI_FORMAT_B8G8R8A8_UNORM);
    m_persistent.Clear(device.Ctx(), { 0, 0, 0, 1 });
    // Seed with the desktop under this viewport so lines appear to draw straight onto it.
    if (const Image* desk = Host::DesktopImage()) {
        Image crop(m_ctx.width, m_ctx.height);
        for (int y = 0; y < m_ctx.height; ++y) {
            int sy = m_ctx.viewport.top + y;
            if (sy < 0 || sy >= desk->height) continue;
            for (int x = 0; x < m_ctx.width; ++x) {
                int sx = m_ctx.viewport.left + x;
                if (sx < 0 || sx >= desk->width) continue;
                crop.At(x, y) = desk->At(sx, sy);
            }
        }
        Texture tex;
        tex.FromImage(device, crop, false);
        m_persistent.Bind(device.Ctx());
        m_post.Copy(device.Ctx(), tex.SRV());
    }
    m_persistentValid = true;
    for (auto& s : m_shapes) s.pending.clear();
}

void Mystify95Saver::ResetShapes() {
    Rng& rng = *m_ctx.rng;
    float w = static_cast<float>(m_ctx.width), h = static_cast<float>(m_ctx.height);
    float scale = h / 480.0f;    // velocities are authored for a 640x480 screen
    for (int i = 0; i < 2; ++i) {
        Shape& s = m_shapes[i];
        s.trail.clear();
        s.pending.clear();
        for (int v = 0; v < kVertices; ++v) {
            s.pos[v] = { rng.Range(0, w), rng.Range(0, h) };
            float speed = rng.Range(3.0f, 9.0f) * scale;
            s.vel[v] = { (rng.Chance(0.5f) ? 1.0f : -1.0f) * speed * rng.Range(0.6f, 1.0f), (rng.Chance(0.5f) ? 1.0f : -1.0f) * speed * rng.Range(0.6f, 1.0f) };
        }
        s.hue = rng.Float();
        s.hueStep = rng.Range(0.004f, 0.008f);
        s.phase = rng.Float();
    }
}

void Mystify95Saver::Tick(Shape& s, const MystifyShapeSettings& cfg) {
    float w = static_cast<float>(m_ctx.width), h = static_cast<float>(m_ctx.height);
    for (int v = 0; v < kVertices; ++v) {
        s.pos[v].x += s.vel[v].x;
        s.pos[v].y += s.vel[v].y;
        if (s.pos[v].x < 0) { s.pos[v].x = -s.pos[v].x; s.vel[v].x = std::fabs(s.vel[v].x); }
        if (s.pos[v].x > w - 1) { s.pos[v].x = 2 * (w - 1) - s.pos[v].x; s.vel[v].x = -std::fabs(s.vel[v].x); }
        if (s.pos[v].y < 0) { s.pos[v].y = -s.pos[v].y; s.vel[v].y = std::fabs(s.vel[v].y); }
        if (s.pos[v].y > h - 1) { s.pos[v].y = 2 * (h - 1) - s.pos[v].y; s.vel[v].y = -std::fabs(s.vel[v].y); }
    }

    Snapshot snap;
    for (int v = 0; v < kVertices; ++v) snap.p[v] = { std::floor(s.pos[v].x), std::floor(s.pos[v].y) };
    if (cfg.randomColors) {
        s.hue = Wrap01(s.hue + s.hueStep);
        snap.color = HsvToRgb(s.hue, 1.0f, 1.0f);
    } else {
        // Triangle wave between the two colours, one full cycle every ~4 seconds.
        s.phase = Wrap01(s.phase + 1.0f / (kTickRate * 4.0f));
        float t = s.phase < 0.5f ? s.phase * 2.0f : 2.0f - s.phase * 2.0f;
        snap.color = Lerp(FromColorRef(cfg.color1), FromColorRef(cfg.color2), t);
    }
    s.trail.push_back(snap);
    while (static_cast<int>(s.trail.size()) > cfg.lines) s.trail.pop_front();
    if (!m_settings.clearScreen) s.pending.push_back(snap);
}

void Mystify95Saver::Update(float dt, double) {
    m_accumulator += dt;
    const float step = 1.0f / kTickRate;
    int ticks = 0;
    while (m_accumulator >= step && ticks < 8) {
        m_accumulator -= step;
        ++ticks;
        for (int i = 0; i < 2; ++i)
            if (m_settings.shape[i].active) Tick(m_shapes[i], m_settings.shape[i]);
    }
    if (m_accumulator > step) m_accumulator = 0.0f;   // never spiral after a stall
}

void Mystify95Saver::DrawSnapshot(const Snapshot& s) {
    m_lines.Polyline(s.p, kVertices, true, s.color);
}

std::optional<XMFLOAT4> Mystify95Saver::ClearColor() const {
    if (m_settings.clearScreen) return XMFLOAT4{ 0, 0, 0, 1 };
    return std::nullopt;   // the persistent target is copied over the whole viewport
}

void Mystify95Saver::Render(Device& device, SwapChain& swap) {
    ID3D11DeviceContext* ctx = device.Ctx();
    if (m_settings.clearScreen) {
        m_lines.Begin(m_ctx.width, m_ctx.height);
        for (const auto& shape : m_shapes)
            for (const auto& snap : shape.trail) DrawSnapshot(snap);
        m_lines.End(device);
        return;
    }

    if (!m_persistentValid) InitPersistentTarget(device);
    // Accumulate the new polygons into the persistent target, then blit it to the screen.
    m_persistent.Bind(ctx);
    m_lines.Begin(m_ctx.width, m_ctx.height);
    for (auto& shape : m_shapes) {
        for (const auto& snap : shape.pending) DrawSnapshot(snap);
        shape.pending.clear();
    }
    m_lines.End(device);

    swap.Bind();
    swap.SetViewport(m_ctx.viewport);
    m_post.Copy(ctx, m_persistent.SRV());
}

void Mystify95Saver::Resize(int width, int height) {
    m_ctx.width = width;
    m_ctx.height = height;
    m_ctx.viewport = { 0, 0, width, height };
    ResetShapes();
    m_persistentValid = false;
}
