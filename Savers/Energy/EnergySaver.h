#pragma once
#include "Saver.h"
#include "Gfx/LineRenderer2D.h"
#include "Gfx/RenderTexture.h"
#include "Gfx/SpriteBatch2D.h"
#include "Gfx/TrailBuffer.h"
#include <vector>

// Windows Energy (Vista): a bundle of glowing blue streamers waving across the screen,
// pinched at the edges, with bloom and motion streaks.
struct EnergySettings {
    int streamers = 5;        // 1..10 -> 30..120
    int amplitude = 5;        // 1..10
    int speed = 5;            // 1..10
    int tint = 210;           // hue in degrees, 0..359
    bool bloom = true;

    static EnergySettings Load(const rs::Settings& s);
    void Save(rs::Settings& s) const;
};

class EnergySaver : public rs::Saver {
public:
    void Initialize(rs::Device& device, const rs::SaverContext& ctx) override;
    void Update(float dt, double time) override;
    void Render(rs::Device& device, rs::SwapChain& swap) override;
    void Resize(int width, int height) override;
    std::optional<DirectX::XMFLOAT4> ClearColor() const override { return std::nullopt; }

private:
    struct Streamer { float phase, waves, amp, rate, offset, bright, noise; };

    void CreateTargets(rs::Device& device);
    void Spawn();

    EnergySettings m_settings;
    rs::SaverContext m_ctx;
    std::vector<Streamer> m_streamers;
    double m_time = 0.0;

    rs::LineRenderer2D m_lines;
    rs::SpriteBatch2D m_sprites;
    rs::TrailBuffer m_trail;
    rs::RenderTexture m_bloomSrc, m_bloomTmp, m_bloom;
};
