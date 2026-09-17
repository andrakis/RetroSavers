#pragma once
#include "Saver.h"
#include "Gfx/ConstantBuffer.h"
#include "Gfx/PostProcess.h"
#include "Gfx/RenderTexture.h"

// Aurora (Windows Vista): soft curtains of coloured light. Pixel-shader driven.
struct AuroraSettings {
    int speed = 5;       // 1..10
    int brightness = 5;  // 1..10
    int amplitude = 5;   // 1..10
    int layers = 6;      // 1..10

    static AuroraSettings Load(const rs::Settings& s);
    void Save(rs::Settings& s) const;
};

class AuroraSaver : public rs::Saver {
public:
    void Initialize(rs::Device& device, const rs::SaverContext& ctx) override;
    void Update(float dt, double time) override;
    void Render(rs::Device& device, rs::SwapChain& swap) override;
    void Resize(int width, int height) override;
    std::optional<DirectX::XMFLOAT4> ClearColor() const override { return std::nullopt; }

private:
    struct AuroraCB { DirectX::XMFLOAT4 timeRes; DirectX::XMFLOAT4 params; };
    struct CompositeCB { DirectX::XMFLOAT4 params; };

    void CreateTargets(rs::Device& device);

    AuroraSettings m_settings;
    rs::SaverContext m_ctx;
    double m_time = 0.0;
    float m_seed = 0.0f;

    rs::PostProcess m_post;
    rs::ComPtr<ID3D11PixelShader> m_auroraPs;
    rs::ComPtr<ID3D11PixelShader> m_compositePs;
    rs::ConstantBuffer<AuroraCB> m_auroraCb;
    rs::ConstantBuffer<CompositeCB> m_compositeCb;
    rs::RenderTexture m_base, m_blurTmp, m_bloom;
};
