#pragma once
#include "Saver.h"
#include "Gfx/ConstantBuffer.h"
#include "Gfx/PostProcess.h"
#include "Gfx/RenderTexture.h"

// Plasma: the demoscene sum-of-sines with a cycling palette, one fullscreen pixel shader.
struct PlasmaSettings {
    enum Palette { Rainbow = 0, Fire = 1, Ocean = 2, Neon = 3, Greyscale = 4 };
    enum Resolution { Half = 0, Full = 1, Quarter = 2 };

    int speed = 5;        // 1..10
    int scale = 5;        // 1..10 (zoom)
    int palette = Rainbow;
    int resolution = Half;

    static PlasmaSettings Load(const rs::Settings& s);
    void Save(rs::Settings& s) const;
};

class PlasmaSaver : public rs::Saver {
public:
    void Initialize(rs::Device& device, const rs::SaverContext& ctx) override;
    void Update(float dt, double time) override;
    void Render(rs::Device& device, rs::SwapChain& swap) override;
    void Resize(int width, int height) override;
    std::optional<DirectX::XMFLOAT4> ClearColor() const override { return std::nullopt; }

private:
    struct PlasmaCB { DirectX::XMFLOAT4 timeRes, a, b, c, d; };

    PlasmaSettings m_settings;
    rs::SaverContext m_ctx;
    double m_time = 0.0;
    rs::PostProcess m_post;
    rs::ComPtr<ID3D11PixelShader> m_ps;
    rs::ConstantBuffer<PlasmaCB> m_cb;
    rs::RenderTexture m_target;
};
