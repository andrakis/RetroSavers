#pragma once
#include "Texture.h"
#include <DirectXMath.h>
#include <windows.h>
#include <string>
#include <unordered_map>

namespace rs {

struct Glyph {
    DirectX::XMFLOAT4 uvRect;   // u0, v0, u1, v1 of the glyph's cell
    float advance;              // pen advance in pixels
};

// Grid of anti-aliased GDI glyphs (white, luma -> alpha) for one font. Every cell has the same
// size (the font's maximum extents) so callers can lay text out on a fixed grid, e.g. Matrix.
class GlyphAtlas {
public:
    // Renders each distinct character of `chars`. The cell size is reduced (font shrunk) as
    // needed so the whole set fits in a 4096 x 4096 texture.
    void Build(Device& device, const LOGFONTW& font, const std::wstring& chars);

    const Glyph* Find(wchar_t c) const;
    int CellWidth() const { return m_cellW; }
    int CellHeight() const { return m_cellH; }
    int Count() const { return static_cast<int>(m_glyphs.size()); }
    const Texture& GetTexture() const { return m_texture; }
    bool Valid() const { return m_texture.Valid(); }

private:
    Texture m_texture;
    std::unordered_map<wchar_t, Glyph> m_glyphs;
    int m_cellW = 0, m_cellH = 0;
};

} // namespace rs
