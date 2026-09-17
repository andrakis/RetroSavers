#pragma once
#include "Saver.h"
#include "Gfx/ConstantBuffer.h"
#include "Gfx/States.h"
#include <vector>

struct StarfieldSettings {
    int starCount = 50;   // 10..200
    int warpSpeed = 5;    // 1..10

    static StarfieldSettings Load(const rs::Settings& s);
    void Save(rs::Settings& s) const;
};

class StarfieldSaver : public rs::Saver {
public:
    void Initialize(rs::Device& device, const rs::SaverContext& ctx) override;
    void Update(float dt, double time) override;
    void Render(rs::Device& device, rs::SwapChain& swap) override;
    void Resize(int width, int height) override;

private:
    struct Star { float x, y, z; };
    struct Instance { DirectX::XMFLOAT2 pos; float size; float brightness; };
    struct ViewportCB { DirectX::XMFLOAT4 viewport; };

    void Respawn(Star& s, bool anywhere);

    StarfieldSettings m_settings;
    rs::SaverContext m_ctx;
    std::vector<Star> m_stars;
    std::vector<Instance> m_instances;

    rs::ComPtr<ID3D11VertexShader> m_vs;
    rs::ComPtr<ID3D11PixelShader> m_ps;
    rs::ComPtr<ID3D11InputLayout> m_layout;
    rs::ConstantBuffer<ViewportCB> m_cb;
    rs::DynamicVertexBuffer<Instance> m_vb;
    rs::States m_states;
};
