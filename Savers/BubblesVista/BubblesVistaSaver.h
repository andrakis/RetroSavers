#pragma once
#include "Saver.h"
#include "Gfx/ConstantBuffer.h"
#include "Gfx/SpriteBatch2D.h"
#include "Gfx/Texture.h"
#include <vector>

// Bubbles (Windows Vista+): iridescent soap bubbles drift over the desktop (or a dark
// gradient), bouncing off the edges and each other.
struct BubblesVistaSettings {
    enum ColorMode { Iridescent = 0, SolidColor = 1, PerBubble = 2 };

    int count = 12;               // 1..40
    int size = 5;                 // 1..10
    int speed = 5;                // 1..10
    bool showOnDesktop = true;    // run mode only; decided at launch in Main.cpp
    int colorMode = Iridescent;
    COLORREF color = RGB(120, 200, 255);
    bool wobble = true;

    static BubblesVistaSettings Load(const rs::Settings& s);
    void Save(rs::Settings& s) const;
};

class BubblesVistaSaver : public rs::Saver {
public:
    void Initialize(rs::Device& device, const rs::SaverContext& ctx) override;
    void Update(float dt, double time) override;
    void Render(rs::Device& device, rs::SwapChain& swap) override;
    void Resize(int width, int height) override;
    std::optional<DirectX::XMFLOAT4> ClearColor() const override { return std::nullopt; }

private:
    struct Bubble { float x, y, vx, vy, r, phase, wobblePhase, wobbleRate; };
    struct BubbleCB { DirectX::XMFLOAT4 rect, params, light, tint; };

    void CreateBackground(rs::Device& device);
    void Spawn();

    BubblesVistaSettings m_settings;
    rs::SaverContext m_ctx;
    std::vector<Bubble> m_bubbles;
    double m_time = 0.0;

    rs::SpriteBatch2D m_sprites;
    rs::Texture m_background;
    bool m_hasDesktop = false;
    rs::ComPtr<ID3D11PixelShader> m_bubblePs;
    rs::ConstantBuffer<BubbleCB> m_cb;
};
