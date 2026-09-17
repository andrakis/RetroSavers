#pragma once
#include "Saver.h"
#include "Gfx/SpriteBatch2D.h"
#include "Gfx/Texture.h"
#include <string>

// Bouncing disc logo: one sprite drifts diagonally, bounces off the edges, recolours on every
// bounce and celebrates the rare corner hit. The logo is a generic procedural stand-in; any
// image can be substituted in the settings.
struct DVDBounceSettings {
    enum ColorMode { Random = 0, Palette = 1, None = 2 };
    enum CornerRate { Rare = 0, Occasional = 1, Frequent = 2 };

    std::wstring imagePath;       // empty = procedural disc logo
    int size = 18;                // 5..50, % of the viewport height
    int speed = 5;                // 1..10
    int colorMode = Palette;
    int cornerRate = Occasional;
    bool showCounter = true;
    COLORREF background = RGB(0, 0, 0);

    static DVDBounceSettings Load(const rs::Settings& s);
    void Save(rs::Settings& s) const;
};

class DVDBounceSaver : public rs::Saver {
public:
    void Initialize(rs::Device& device, const rs::SaverContext& ctx) override;
    void Update(float dt, double time) override;
    void Render(rs::Device& device, rs::SwapChain& swap) override;
    void Resize(int width, int height) override;
    std::optional<DirectX::XMFLOAT4> ClearColor() const override;

private:
    void LoadLogo(rs::Device& device);
    void Layout();
    void NextColor();
    void RefreshCounter(rs::Device& device);

    DVDBounceSettings m_settings;
    rs::SaverContext m_ctx;
    rs::Device* m_device = nullptr;

    rs::SpriteBatch2D m_sprites;
    rs::Texture m_logo;
    rs::Texture m_counter;
    bool m_counterDirty = true;

    float m_spriteW = 1, m_spriteH = 1;   // pixels
    float m_travelW = 1, m_travelH = 1;   // distance the centre can move on each axis
    float m_periodX = 1, m_periodY = 1;   // round-trip time per axis (seconds)
    double m_time = 0.0;                  // motion clock, offset so the run starts mid-flight
    int m_segX = 0, m_segY = 0;           // half-period indices, change = wall hit
    double m_lastHitX = -10, m_lastHitY = -10;

    int m_corners = 0;
    float m_flash = 0.0f;                 // seconds of corner celebration left
    int m_paletteIndex = 0;
    DirectX::XMFLOAT4 m_color{ 1, 1, 1, 1 };
    DirectX::XMFLOAT2 m_pos{ 0, 0 };
};
