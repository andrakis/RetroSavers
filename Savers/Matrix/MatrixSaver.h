#pragma once
#include "Saver.h"
#include "Gfx/GlyphAtlas.h"
#include "Gfx/SpriteBatch2D.h"
#include "Gfx/TrailBuffer.h"
#include <string>
#include <vector>

// Matrix digital rain: columns of falling mirrored half-width katakana with bright heads and
// fading trails, drawn from a GDI glyph atlas.
struct MatrixSettings {
    enum Charset { Katakana = 0, Latin = 1, Binary = 2 };

    int speed = 5;                        // 1..10
    int density = 5;                      // 1..10
    int glyphSize = 18;                   // 8..48 px (x dpiScale)
    COLORREF color = RGB(0, 255, 70);
    int charset = Katakana;
    bool boldHeads = true;
    bool afterglow = false;

    static MatrixSettings Load(const rs::Settings& s);
    void Save(rs::Settings& s) const;
};

class MatrixSaver : public rs::Saver {
public:
    void Initialize(rs::Device& device, const rs::SaverContext& ctx) override;
    void Update(float dt, double time) override;
    void Render(rs::Device& device, rs::SwapChain& swap) override;
    void Resize(int width, int height) override;
    std::optional<DirectX::XMFLOAT4> ClearColor() const override;

private:
    struct Column {
        bool active = false;
        float head = 0.0f;        // row of the head (fractional)
        float speed = 8.0f;       // rows per second
        int length = 12;
        float wait = 0.0f;        // seconds until respawn when inactive
        int lastRow = -1;         // row the head last occupied (re-randomise glyph on entry)
    };

    void BuildAtlases(rs::Device& device);
    void Layout();
    void Spawn(Column& c, bool anywhere);
    const rs::Glyph* GlyphFor(const rs::GlyphAtlas& atlas, int cell) const;

    MatrixSettings m_settings;
    rs::SaverContext m_ctx;
    rs::Device* m_device = nullptr;

    std::wstring m_chars;
    rs::GlyphAtlas m_atlas, m_boldAtlas;
    int m_cellW = 1, m_cellH = 1;
    int m_cols = 0, m_rows = 0;
    std::vector<Column> m_columns;
    std::vector<uint16_t> m_cells;     // glyph index per (col, row)
    std::vector<uint8_t> m_mirror;     // per cell: draw mirrored

    rs::SpriteBatch2D m_sprites;
    rs::TrailBuffer m_trail;
    DirectX::XMFLOAT4 m_color{ 0, 1, 0.27f, 1 };
    DirectX::XMFLOAT4 m_headColor{ 0.85f, 1, 0.9f, 1 };
};
