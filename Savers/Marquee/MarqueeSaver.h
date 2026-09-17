#pragma once
#include "Saver.h"
#include "Gfx/SpriteBatch2D.h"
#include "Gfx/Texture.h"
#include <string>
#include <vector>

// Marquee (Windows 3.1 - XP): a line of text scrolls right to left, re-entering at a random
// (or centred) height each pass.
struct MarqueeSettings {
    enum Position { RandomPos = 0, Centred = 1 };

    std::wstring text = L"RetroSavers";
    LOGFONTW font{};              // face / weight / italic; height comes from `size`
    int size = 72;                // 8..200 px at 96 dpi
    int speed = 5;                // 1..10
    int position = RandomPos;
    COLORREF textColor = RGB(255, 255, 255);
    COLORREF background = RGB(0, 0, 0);
    bool mirror = false;

    MarqueeSettings();
    static MarqueeSettings Load(const rs::Settings& s);
    void Save(rs::Settings& s) const;
};

class MarqueeSaver : public rs::Saver {
public:
    void Initialize(rs::Device& device, const rs::SaverContext& ctx) override;
    void Update(float dt, double time) override;
    void Render(rs::Device& device, rs::SwapChain& swap) override;
    void Resize(int width, int height) override;
    std::optional<DirectX::XMFLOAT4> ClearColor() const override;

private:
    struct Chunk { rs::Texture texture; float offset, width; };
    static constexpr int kMaxChunk = 8192;   // texture width cap well under the FL11 limit

    void BuildText(rs::Device& device);
    void NewPass();

    MarqueeSettings m_settings;
    rs::SaverContext m_ctx;
    rs::SpriteBatch2D m_sprites;
    std::vector<Chunk> m_chunks;
    float m_totalW = 1, m_textH = 1;
    float m_x = 0, m_y = 0;
};
