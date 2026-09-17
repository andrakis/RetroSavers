#include "PersianRugSaver.h"
#include "Gfx/Device.h"
#include "Gfx/SwapChain.h"
#include "Util/Color.h"
#include "Util/MathUtil.h"
#include <algorithm>
#include <cmath>

using namespace DirectX;
using namespace rs;

// ---------------------------------------------------------------- settings

PersianRugSettings PersianRugSettings::Load(const Settings& s) {
    PersianRugSettings v;
    v.detail = Clamp(s.GetInt(L"Detail", v.detail), 6, 9);
    v.colors = Clamp(s.GetInt(L"Colors", v.colors), 4, 64);
    v.speed = Clamp(s.GetInt(L"Speed", v.speed), 1, 10);
    v.palette = Clamp(s.GetInt(L"Palette", v.palette), 0, 4);
    v.layout = Clamp(s.GetInt(L"Layout", v.layout), 0, 2);
    v.cycle = s.GetBool(L"Cycle", v.cycle);
    v.hold = Clamp(s.GetInt(L"Hold", v.hold), 2, 60);
    return v;
}

void PersianRugSettings::Save(Settings& s) const {
    s.SetInt(L"Detail", detail);
    s.SetInt(L"Colors", colors);
    s.SetInt(L"Speed", speed);
    s.SetInt(L"Palette", palette);
    s.SetInt(L"Layout", layout);
    s.SetBool(L"Cycle", cycle);
    s.SetInt(L"Hold", hold);
}

// ---------------------------------------------------------------- lifecycle

void PersianRugSaver::Initialize(Device& device, const SaverContext& ctx) {
    m_ctx = ctx;
    m_settings = PersianRugSettings::Load(*ctx.settings);
    m_sprites.Create(device);
    m_n = (1 << m_settings.detail) + 1;
    m_cells.assign(static_cast<size_t>(m_n) * m_n, kUnwoven);
    m_image = Image(m_n, m_n, 0xFF000000u);
    NewRug();
}

void PersianRugSaver::BuildPalette() {
    Rng& rng = *m_ctx.rng;
    const int n = m_settings.colors;
    m_palette.resize(n);
    // Key colours are spread around a loop and interpolated so neighbouring indices blend;
    // the recursion averages indices, so a smooth palette reads as woven bands.
    std::vector<XMFLOAT4> keys;
    switch (m_settings.palette) {
    case PersianRugSettings::Persian:
        keys = { { 0.55f, 0.08f, 0.10f, 1 }, { 0.85f, 0.65f, 0.25f, 1 }, { 0.10f, 0.15f, 0.40f, 1 }, { 0.93f, 0.88f, 0.75f, 1 },
                 { 0.70f, 0.20f, 0.12f, 1 }, { 0.20f, 0.35f, 0.25f, 1 }, { 0.90f, 0.55f, 0.20f, 1 }, { 0.30f, 0.05f, 0.08f, 1 } };
        break;
    case PersianRugSettings::Cool:
        keys = { { 0.05f, 0.10f, 0.35f, 1 }, { 0.10f, 0.55f, 0.65f, 1 }, { 0.60f, 0.85f, 0.90f, 1 }, { 0.25f, 0.20f, 0.55f, 1 },
                 { 0.10f, 0.45f, 0.35f, 1 }, { 0.80f, 0.80f, 0.95f, 1 } };
        break;
    case PersianRugSettings::RandomKeys:
        for (int i = 0; i < 6; ++i) keys.push_back(HsvToRgb(rng.Float(), rng.Range(0.4f, 1.0f), rng.Range(0.35f, 1.0f)));
        break;
    case PersianRugSettings::Grey:
        keys = { { 0.05f, 0.05f, 0.05f, 1 }, { 0.95f, 0.95f, 0.95f, 1 }, { 0.4f, 0.4f, 0.4f, 1 }, { 0.75f, 0.75f, 0.75f, 1 } };
        break;
    default: // Rainbow
        for (int i = 0; i < n; ++i) m_palette[i] = HsvToRgb(static_cast<float>(i) / n, 0.9f, 1.0f);
        return;
    }
    const int k = static_cast<int>(keys.size());
    for (int i = 0; i < n; ++i) {
        float f = static_cast<float>(i) * k / n;
        int a = static_cast<int>(f) % k, b = (a + 1) % k;
        m_palette[i] = Lerp(keys[a], keys[b], f - std::floor(f));
    }
}

void PersianRugSaver::Recurse(int x0, int y0, int x1, int y1) {
    if (x1 - x0 < 2) return;
    auto at = [&](int x, int y) { return static_cast<int>(m_plan[static_cast<size_t>(y) * m_n + x]); };
    // The corners were coloured by the enclosing square's midlines (or the border).
    int sum = at(x0, y0) + at(x1, y0) + at(x0, y1) + at(x1, y1);
    int c = (sum / 4 + m_shift) % m_settings.colors;
    int xm = (x0 + x1) / 2, ym = (y0 + y1) / 2;
    Op h{ static_cast<uint16_t>(x0), static_cast<uint16_t>(ym), static_cast<uint16_t>(x1), static_cast<uint16_t>(ym), static_cast<uint8_t>(c) };
    Op v{ static_cast<uint16_t>(xm), static_cast<uint16_t>(y0), static_cast<uint16_t>(xm), static_cast<uint16_t>(y1), static_cast<uint8_t>(c) };
    for (const Op& op : { h, v }) {
        m_ops.push_back(op);
        for (int y = op.y0; y <= op.y1; ++y)
            for (int x = op.x0; x <= op.x1; ++x) m_plan[static_cast<size_t>(y) * m_n + x] = op.color;
    }
    Recurse(x0, y0, xm, ym);
    Recurse(xm, y0, x1, ym);
    Recurse(x0, ym, xm, y1);
    Recurse(xm, ym, x1, y1);
}

void PersianRugSaver::NewRug() {
    Rng& rng = *m_ctx.rng;
    BuildPalette();
    m_shift = rng.Int(1, m_settings.colors - 1);
    const int border = rng.Int(0, m_settings.colors - 1);
    // Plan the whole rug up front so drawing is just replaying lines.
    m_plan.assign(static_cast<size_t>(m_n) * m_n, 0);
    m_ops.clear();
    const uint16_t last = static_cast<uint16_t>(m_n - 1);
    const uint8_t bc = static_cast<uint8_t>(border);
    for (const Op& op : { Op{ 0, 0, last, 0, bc }, Op{ 0, last, last, last, bc }, Op{ 0, 0, 0, last, bc }, Op{ last, 0, last, last, bc } }) {
        m_ops.push_back(op);
        for (int y = op.y0; y <= op.y1; ++y)
            for (int x = op.x0; x <= op.x1; ++x) m_plan[static_cast<size_t>(y) * m_n + x] = op.color;
    }
    Recurse(0, 0, m_n - 1, m_n - 1);
    m_nextOp = 0;
    m_opAccumulator = 0.0f;
    m_holdLeft = static_cast<float>(m_settings.hold);
    m_cyclePhase = 0.0f;
}

void PersianRugSaver::ApplyOp(const Op& op) {
    for (int y = op.y0; y <= op.y1; ++y)
        for (int x = op.x0; x <= op.x1; ++x) m_cells[static_cast<size_t>(y) * m_n + x] = op.color;
}

void PersianRugSaver::Update(float dt, double) {
    if (m_nextOp < m_ops.size()) {
        // Weave at a rate that finishes a rug in ~4-24 s regardless of detail.
        float seconds = 26.0f - 2.2f * m_settings.speed;
        m_opAccumulator += m_ops.size() * dt / seconds;
        int n = static_cast<int>(m_opAccumulator);
        m_opAccumulator -= n;
        while (n-- > 0 && m_nextOp < m_ops.size()) ApplyOp(m_ops[m_nextOp++]);
        m_dirty = true;
        return;
    }
    if (m_settings.cycle) {
        m_cyclePhase += dt * (0.6f + 0.1f * m_settings.speed);
        m_dirty = true;
    }
    m_holdLeft -= dt;
    if (m_holdLeft <= 0.0f) NewRug();
}

void PersianRugSaver::Encode() {
    const int n = m_settings.colors;
    // Palette rotation is applied at encode time so the index buffer stays the rug itself.
    int offset = static_cast<int>(m_cyclePhase) % n;
    for (size_t i = 0; i < m_cells.size(); ++i) {
        if (m_cells[i] == kUnwoven) { m_image.pixels[i] = 0xFF000000u; continue; }
        const XMFLOAT4& c = m_palette[(m_cells[i] + offset) % n];
        m_image.pixels[i] = PackRgbaF(c.x, c.y, c.z, 1.0f);
    }
}

void PersianRugSaver::Render(Device& device, SwapChain&) {
    if (m_dirty) {
        Encode();
        m_texture.Update(device, m_image);
        m_dirty = false;
    }
    const States& states = m_sprites.GetStates();
    const float w = static_cast<float>(m_ctx.width), h = static_cast<float>(m_ctx.height);
    m_sprites.Begin(m_ctx.width, m_ctx.height);
    ID3D11SamplerState* sampler = states.PointClamp();
    switch (m_settings.layout) {
    case PersianRugSettings::Stretch:
        m_sprites.Push(w * 0.5f, h * 0.5f, w, h, { 1, 1, 1, 1 });
        break;
    case PersianRugSettings::Tile: {
        float s = std::min(w, h);
        m_sprites.Push(w * 0.5f, h * 0.5f, w, h, { 1, 1, 1, 1 }, { 0, 0, w / s, h / s });
        sampler = states.PointWrap();
        break;
    }
    default: {
        float s = std::min(w, h);
        m_sprites.Push(std::floor(w * 0.5f), std::floor(h * 0.5f), s, s, { 1, 1, 1, 1 });
        break;
    }
    }
    m_sprites.End(device, &m_texture, states.Opaque(), nullptr, sampler);
}

void PersianRugSaver::Resize(int width, int height) {
    m_ctx.width = width;
    m_ctx.height = height;
    m_ctx.viewport = { 0, 0, width, height };
}
