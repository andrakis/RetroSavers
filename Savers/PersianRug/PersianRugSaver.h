#pragma once
#include "Saver.h"
#include "Gfx/ConstantBuffer.h"
#include "Gfx/SpriteBatch2D.h"
#include "Gfx/Texture.h"
#include <vector>

// Persian Rug (Windows 3.x era): the "Persian recursion" pattern. A square grid's border is
// drawn in one colour, then each square's horizontal and vertical midlines take the colour
// (average of the four corners + shift) mod N and the four quadrants recurse. Drawn
// progressively like the original, then the palette cycles until the next rug is woven over it.
struct PersianRugSettings {
    enum Palette { Persian = 0, Rainbow = 1, Cool = 2, RandomKeys = 3, Grey = 4 };
    enum Layout { Square = 0, Stretch = 1, Tile = 2 };

    int detail = 8;           // 6..9 -> grid of 2^detail + 1 cells
    int colors = 16;          // 4..64 palette entries
    int speed = 5;            // 1..10 weaving speed
    int palette = Persian;
    int layout = Square;
    bool cycle = true;        // rotate the palette once the rug is complete
    bool dots = true;         // plot cells as spaced dots (the Windows 3.x look)
    int hold = 12;            // 2..60 seconds to show a finished rug

    static PersianRugSettings Load(const rs::Settings& s);
    void Save(rs::Settings& s) const;
};

class PersianRugSaver : public rs::Saver {
public:
    void Initialize(rs::Device& device, const rs::SaverContext& ctx) override;
    void Update(float dt, double time) override;
    void Render(rs::Device& device, rs::SwapChain& swap) override;
    void Resize(int width, int height) override;

private:
    struct Op { uint16_t x0, y0, x1, y1; uint8_t color; };   // one grid line in one colour
    static constexpr uint8_t kUnwoven = 255;                  // cell never drawn (first rug only)

    void NewRug();
    void BuildPalette();
    void Recurse(int x0, int y0, int x1, int y1);
    int CellAt(int x, int y) const { return m_cells[static_cast<size_t>(y) * m_n + x]; }
    void ApplyOp(const Op& op);
    void Encode();

    PersianRugSettings m_settings;
    rs::SaverContext m_ctx;

    int m_n = 257;                        // cells per side
    int m_shift = 1;                      // added to the corner average, mod colours
    std::vector<uint8_t> m_cells;         // palette index per cell (the rug being woven)
    std::vector<uint8_t> m_plan;          // scratch copy used while generating the op list
    std::vector<Op> m_ops;
    size_t m_nextOp = 0;
    float m_opAccumulator = 0.0f;
    float m_holdLeft = 0.0f;
    float m_cyclePhase = 0.0f;
    std::vector<DirectX::XMFLOAT4> m_palette;
    rs::Image m_image;
    bool m_dirty = true;

    struct DotsCB { DirectX::XMFLOAT4 params; };

    rs::SpriteBatch2D m_sprites;
    rs::Texture m_texture;
    rs::ComPtr<ID3D11PixelShader> m_dotsPs;
    rs::ConstantBuffer<DotsCB> m_dotsCb;
};
