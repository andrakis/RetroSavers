#pragma once
#include "Saver.h"
#include "Gfx/ConstantBuffer.h"
#include "Gfx/PostProcess.h"
#include "Gfx/RenderTexture.h"
#include <cstdint>

// Conway's Game of Life on the GPU: two RGBA8 targets ping-pong through a rule shader at a
// fixed tick; a palette shader draws the cells. Reseeds when the world goes still.
struct LifeSettings {
    enum Palette { Classic = 0, AgeHues = 1, White = 2, Amber = 3 };

    int cellSize = 6;         // 2..16 px
    int tickRate = 12;        // 1..30 generations per second
    int density = 30;         // 5..60 % alive at seed
    int palette = Classic;
    bool wrap = true;
    bool reseed = true;

    static LifeSettings Load(const rs::Settings& s);
    void Save(rs::Settings& s) const;
};

class LifeSaver : public rs::Saver {
public:
    void Initialize(rs::Device& device, const rs::SaverContext& ctx) override;
    void Update(float dt, double time) override;
    void Render(rs::Device& device, rs::SwapChain& swap) override;
    void Resize(int width, int height) override;
    std::optional<DirectX::XMFLOAT4> ClearColor() const override { return std::nullopt; }

private:
    struct StepCB { DirectX::XMFLOAT4 texel; };
    struct ShowCB { DirectX::XMFLOAT4 params; };

    void CreateWorld(rs::Device& device);
    void Seed(rs::Device& device);
    void Step(rs::Device& device);
    void CheckStagnation(rs::Device& device);

    LifeSettings m_settings;
    rs::SaverContext m_ctx;
    int m_cols = 1, m_rows = 1;
    float m_accumulator = 0.0f;
    int m_pendingSteps = 0;
    float m_checkTimer = 0.0f;
    uint64_t m_hashes[6]{};
    int m_hashCount = 0;
    float m_sinceSeed = 0.0f;

    rs::PostProcess m_post;
    rs::ComPtr<ID3D11PixelShader> m_stepPs, m_showPs;
    rs::ConstantBuffer<StepCB> m_stepCb;
    rs::ConstantBuffer<ShowCB> m_showCb;
    rs::RenderTexture m_state[2];
    int m_current = 0;
};
