#include "MatrixSaver.h"
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

MatrixSettings MatrixSettings::Load(const Settings& s) {
    MatrixSettings v;
    v.speed = Clamp(s.GetInt(L"Speed", v.speed), 1, 10);
    v.density = Clamp(s.GetInt(L"Density", v.density), 1, 10);
    v.glyphSize = Clamp(s.GetInt(L"GlyphSize", v.glyphSize), 8, 48);
    v.color = static_cast<COLORREF>(s.GetInt(L"Color", static_cast<int>(v.color)));
    v.charset = Clamp(s.GetInt(L"Charset", v.charset), 0, 2);
    v.boldHeads = s.GetBool(L"BoldHeads", v.boldHeads);
    v.afterglow = s.GetBool(L"Afterglow", v.afterglow);
    return v;
}

void MatrixSettings::Save(Settings& s) const {
    s.SetInt(L"Speed", speed);
    s.SetInt(L"Density", density);
    s.SetInt(L"GlyphSize", glyphSize);
    s.SetInt(L"Color", static_cast<int>(color));
    s.SetInt(L"Charset", charset);
    s.SetBool(L"BoldHeads", boldHeads);
    s.SetBool(L"Afterglow", afterglow);
}

// ---------------------------------------------------------------- helpers

namespace {

std::wstring CharsetString(int charset) {
    std::wstring s;
    switch (charset) {
    case MatrixSettings::Katakana:
        for (wchar_t c = 0xFF66; c <= 0xFF9D; ++c) s.push_back(c);   // half-width katakana
        s += L"0123456789Z:\uFF65.\"=*+-<>\u00A6|";   // digits, punctuation, half-width middle dot, broken bar
        break;
    case MatrixSettings::Latin:
        s = L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789@#$%&*+=<>?/";
        break;
    default:
        s = L"01";
        break;
    }
    return s;
}

// True when GDI really has the face (otherwise it silently substitutes).
bool HasFace(const wchar_t* face) {
    LOGFONTW lf = TextureFactory::MakeLogFont(face, 16);
    HDC screen = GetDC(nullptr);
    HFONT font = CreateFontIndirectW(&lf);
    HGDIOBJ old = SelectObject(screen, font);
    wchar_t got[LF_FACESIZE]{};
    GetTextFaceW(screen, LF_FACESIZE, got);
    SelectObject(screen, old);
    DeleteObject(font);
    ReleaseDC(nullptr, screen);
    return _wcsicmp(got, face) == 0;
}

} // namespace

// ---------------------------------------------------------------- lifecycle

void MatrixSaver::Initialize(Device& device, const SaverContext& ctx) {
    m_ctx = ctx;
    m_device = &device;
    m_settings = MatrixSettings::Load(*ctx.settings);
    m_color = FromColorRef(m_settings.color);
    // Head: the base colour pushed most of the way to white.
    m_headColor = Lerp(m_color, XMFLOAT4{ 1, 1, 1, 1 }, 0.8f);
    m_chars = CharsetString(m_settings.charset);
    m_sprites.Create(device);
    BuildAtlases(device);
    Layout();
}

void MatrixSaver::BuildAtlases(Device& device) {
    const wchar_t* face = m_settings.charset == MatrixSettings::Katakana
        ? (HasFace(L"MS Gothic") ? L"MS Gothic" : L"Consolas")
        : (HasFace(L"Consolas") ? L"Consolas" : L"Lucida Console");
    int px = static_cast<int>(m_settings.glyphSize * m_ctx.dpiScale + 0.5f);
    m_atlas.Build(device, TextureFactory::MakeLogFont(face, px, FW_NORMAL), m_chars);
    m_boldAtlas.Build(device, TextureFactory::MakeLogFont(face, px, FW_BOLD), m_chars);
    // Half-width glyphs: pack columns at the width of the widest glyph, rows at the line height.
    m_cellW = std::max(m_atlas.CellWidth() - 2, 1);
    m_cellH = std::max(m_atlas.CellHeight() - 2, 1);
}

void MatrixSaver::Layout() {
    Rng& rng = *m_ctx.rng;
    m_cols = std::max(1, (m_ctx.width + m_cellW - 1) / m_cellW);
    m_rows = std::max(1, (m_ctx.height + m_cellH - 1) / m_cellH);
    m_columns.assign(m_cols, Column{});
    m_cells.resize(static_cast<size_t>(m_cols) * m_rows);
    m_mirror.resize(m_cells.size());
    for (size_t i = 0; i < m_cells.size(); ++i) {
        m_cells[i] = static_cast<uint16_t>(rng.Int(0, static_cast<int>(m_chars.size()) - 1));
        m_mirror[i] = rng.Chance(0.5f) ? 1 : 0;
    }
    // Start with a mix of columns already falling so the screen is not empty for the first seconds.
    float fill = 0.15f + 0.06f * m_settings.density;
    for (auto& c : m_columns) {
        if (rng.Chance(fill)) Spawn(c, true);
        else c.wait = rng.Range(0.0f, 4.0f);
    }
    if (m_settings.afterglow && m_device) {
        m_trail.Resize(*m_device, m_ctx.width, m_ctx.height);
        m_trail.Clear(m_device->Ctx());
    }
}

void MatrixSaver::Spawn(Column& c, bool anywhere) {
    Rng& rng = *m_ctx.rng;
    float k = m_settings.speed / 5.0f;
    c.active = true;
    c.speed = rng.Range(6.0f, 14.0f) * k;
    c.length = rng.Int(std::max(4, m_rows / 6), std::max(8, m_rows * 2 / 3));
    c.head = anywhere ? rng.Range(0.0f, static_cast<float>(m_rows + c.length)) : -rng.Range(0.0f, 6.0f);
    c.lastRow = static_cast<int>(std::floor(c.head));
}

const Glyph* MatrixSaver::GlyphFor(const GlyphAtlas& atlas, int cell) const {
    return atlas.Find(m_chars[m_cells[cell]]);
}

void MatrixSaver::Update(float dt, double) {
    Rng& rng = *m_ctx.rng;
    const int n = static_cast<int>(m_chars.size());
    // Idle gap between streams shrinks with density.
    const float maxWait = 0.4f + 1.1f * (11 - m_settings.density);
    for (int x = 0; x < m_cols; ++x) {
        Column& c = m_columns[x];
        if (!c.active) {
            c.wait -= dt;
            if (c.wait <= 0.0f) Spawn(c, false);
            continue;
        }
        c.head += c.speed * dt;
        int row = static_cast<int>(std::floor(c.head));
        // Re-randomise every cell the head entered since last frame (it "types" the glyph).
        for (int r = c.lastRow + 1; r <= row; ++r) {
            if (r >= 0 && r < m_rows) {
                size_t i = static_cast<size_t>(x) * m_rows + r;
                m_cells[i] = static_cast<uint16_t>(rng.Int(0, n - 1));
                m_mirror[i] = rng.Chance(0.5f) ? 1 : 0;
            }
        }
        c.lastRow = row;
        if (row - c.length >= m_rows) {
            c.active = false;
            c.wait = rng.Range(0.0f, maxWait);
        }
    }
    // Flicker: a handful of random visible cells change glyph each frame.
    int flickers = std::max(1, static_cast<int>(m_cells.size() * 0.4f * dt));
    for (int i = 0; i < flickers; ++i) {
        size_t idx = static_cast<size_t>(rng.Int(0, static_cast<int>(m_cells.size()) - 1));
        m_cells[idx] = static_cast<uint16_t>(rng.Int(0, n - 1));
    }
}

std::optional<XMFLOAT4> MatrixSaver::ClearColor() const {
    if (m_settings.afterglow) return std::nullopt;
    return XMFLOAT4{ 0, 0, 0, 1 };
}

void MatrixSaver::Render(Device& device, SwapChain& swap) {
    const float hw = m_cellW * 0.5f, hh = m_cellH * 0.5f;
    auto drawColumns = [&](const GlyphAtlas& atlas, bool headsOnly) {
        m_sprites.Begin(m_ctx.width, m_ctx.height);
        for (int x = 0; x < m_cols; ++x) {
            const Column& c = m_columns[x];
            if (!c.active) continue;
            int headRow = static_cast<int>(std::floor(c.head));
            float px = x * m_cellW + hw;
            int first = headsOnly ? 0 : 1, last = headsOnly ? 0 : c.length - 1;
            for (int i = first; i <= last; ++i) {
                int row = headRow - i;
                if (row < 0 || row >= m_rows) continue;
                size_t cell = static_cast<size_t>(x) * m_rows + row;
                const Glyph* g = GlyphFor(atlas, static_cast<int>(cell));
                if (!g) continue;
                XMFLOAT4 color;
                if (i == 0) {
                    color = m_headColor;
                } else {
                    float t = 1.0f - static_cast<float>(i) / c.length;
                    float b = std::pow(t, 1.5f);
                    color = { m_color.x * b, m_color.y * b, m_color.z * b, 1.0f };
                }
                XMFLOAT4 uv = g->uvRect;
                if (m_mirror[cell]) std::swap(uv.x, uv.z);
                m_sprites.Push(px, row * m_cellH + hh, static_cast<float>(m_atlas.CellWidth()), static_cast<float>(m_atlas.CellHeight()), color, uv);
            }
        }
        m_sprites.End(device, &atlas.GetTexture(), m_sprites.GetStates().AlphaBlend());
    };

    if (m_settings.afterglow) {
        if (!m_trail.Valid()) { m_trail.Resize(device, m_ctx.width, m_ctx.height); m_trail.Clear(device.Ctx()); }
        m_trail.Begin(device, 0.72f);
    }
    // Trails in the regular face, heads in bold on top so they read as the brightest cell.
    drawColumns(m_atlas, false);
    drawColumns(m_settings.boldHeads ? m_boldAtlas : m_atlas, true);
    if (m_settings.afterglow) {
        swap.Bind();
        swap.SetViewport(m_ctx.viewport);
        m_trail.Present(device);
    }
}

void MatrixSaver::Resize(int width, int height) {
    m_ctx.width = width;
    m_ctx.height = height;
    m_ctx.viewport = { 0, 0, width, height };
    Layout();
}
