#include "GlyphAtlas.h"
#include "TextureFactory.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace rs {

namespace {

constexpr int kMaxTexture = 4096;

struct Measure {
    int cellW = 0, cellH = 0;
    std::vector<int> advances;
};

Measure MeasureGlyphs(const LOGFONTW& lf, const std::wstring& chars) {
    Measure m;
    HDC screen = GetDC(nullptr);
    HDC dc = CreateCompatibleDC(screen);
    HFONT font = CreateFontIndirectW(&lf);
    HGDIOBJ old = SelectObject(dc, font);
    TEXTMETRICW tm{};
    GetTextMetricsW(dc, &tm);
    int maxW = 1;
    m.advances.reserve(chars.size());
    for (wchar_t c : chars) {
        SIZE sz{};
        GetTextExtentPoint32W(dc, &c, 1, &sz);
        m.advances.push_back(sz.cx);
        maxW = std::max<int>(maxW, sz.cx + tm.tmOverhang);
    }
    m.cellW = maxW + 2;
    m.cellH = tm.tmHeight + 2;
    SelectObject(dc, old);
    DeleteObject(font);
    DeleteDC(dc);
    ReleaseDC(nullptr, screen);
    return m;
}

} // namespace

void GlyphAtlas::Build(Device& device, const LOGFONTW& fontIn, const std::wstring& charsIn) {
    m_glyphs.clear();
    // Distinct characters, first occurrence wins.
    std::wstring chars;
    for (wchar_t c : charsIn)
        if (chars.find(c) == std::wstring::npos) chars.push_back(c);
    if (chars.empty()) chars = L"?";

    LOGFONTW lf = fontIn;
    lf.lfQuality = ANTIALIASED_QUALITY;
    const int n = static_cast<int>(chars.size());
    int cols = std::max(1, static_cast<int>(std::ceil(std::sqrt(static_cast<double>(n)))));
    int rows = (n + cols - 1) / cols;

    Measure m = MeasureGlyphs(lf, chars);
    // Shrink the font until the grid fits the texture cap (long charsets at huge sizes).
    for (int attempt = 0; attempt < 16 && (cols * m.cellW > kMaxTexture || rows * m.cellH > kMaxTexture); ++attempt) {
        lf.lfHeight = static_cast<LONG>(lf.lfHeight * 0.8f);
        if (std::abs(lf.lfHeight) < 4) break;
        m = MeasureGlyphs(lf, chars);
    }
    m_cellW = m.cellW;
    m_cellH = m.cellH;
    const int w = cols * m_cellW, h = rows * m_cellH;

    HFONT font = CreateFontIndirectW(&lf);
    Image img = TextureFactory::GdiMask(w, h, [&](HDC dc) {
        HGDIOBJ old = SelectObject(dc, font);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(255, 255, 255));
        for (int i = 0; i < n; ++i) {
            int cx = (i % cols) * m_cellW + 1, cy = (i / cols) * m_cellH + 1;
            TextOutW(dc, cx, cy, &chars[i], 1);
        }
        SelectObject(dc, old);
    });
    DeleteObject(font);
    m_texture.FromImage(device, img, false);

    for (int i = 0; i < n; ++i) {
        int cx = (i % cols) * m_cellW, cy = (i / cols) * m_cellH;
        Glyph g;
        g.uvRect = { static_cast<float>(cx) / w, static_cast<float>(cy) / h,
                     static_cast<float>(cx + m_cellW) / w, static_cast<float>(cy + m_cellH) / h };
        g.advance = static_cast<float>(m.advances[i]);
        m_glyphs[chars[i]] = g;
    }
}

const Glyph* GlyphAtlas::Find(wchar_t c) const {
    auto it = m_glyphs.find(c);
    return it == m_glyphs.end() ? nullptr : &it->second;
}

} // namespace rs
