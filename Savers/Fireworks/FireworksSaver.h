#pragma once
#include "Saver.h"
#include "Gfx/RenderTexture.h"
#include "Gfx/SpriteBatch2D.h"
#include "Gfx/Texture.h"
#include "Gfx/TrailBuffer.h"
#include <vector>

// Fireworks: rockets climb from the bottom edge and burst into peonies, chrysanthemums, rings,
// willows and crackles. Additive soft-dot particles accumulate in a decaying trail buffer with a
// half-resolution bloom on top.
struct FireworksSettings {
    int launchRate = 5;       // 1..10
    int gravity = 5;          // 1..10
    int trail = 5;            // 1..10 persistence
    int bloom = 5;            // 0..10
    int burstSize = 5;        // 1..10
    int finaleEvery = 45;     // seconds, 0 = never

    static FireworksSettings Load(const rs::Settings& s);
    void Save(rs::Settings& s) const;
};

class FireworksSaver : public rs::Saver {
public:
    void Initialize(rs::Device& device, const rs::SaverContext& ctx) override;
    void Update(float dt, double time) override;
    void Render(rs::Device& device, rs::SwapChain& swap) override;
    void Resize(int width, int height) override;
    std::optional<DirectX::XMFLOAT4> ClearColor() const override { return std::nullopt; }

private:
    enum class Kind : uint8_t { Rocket, Spark, Trail, Willow, Crackle };
    enum class Burst { Peony, Chrysanthemum, Ring, Willow, Crackle, Count };

    struct Particle {
        float x, y, vx, vy;
        float life, maxLife;     // seconds remaining / total
        float size;              // pixels
        float drag;
        DirectX::XMFLOAT4 color;
        Kind kind;
        bool emits;              // leaves a trail of tiny sparks
        float twinkle;           // crackle phase
    };

    static constexpr size_t kMaxParticles = 24000;

    void LaunchRocket();
    void Explode(const Particle& rocket);
    void Emit(const Particle& from, int count, float speed, float life, float size, const DirectX::XMFLOAT4& color);
    Particle* Alloc();
    void CreateTargets(rs::Device& device);

    FireworksSettings m_settings;
    rs::SaverContext m_ctx;
    std::vector<Particle> m_particles;
    float m_gravity = 500.0f;         // px/s^2
    float m_scale = 1.0f;             // viewport height / 1080
    float m_launchTimer = 0.0f;
    float m_finaleTimer = 0.0f;
    float m_finaleLeft = 0.0f;        // seconds of finale remaining

    rs::SpriteBatch2D m_sprites;
    rs::Texture m_dot;
    rs::TrailBuffer m_trail;
    rs::RenderTexture m_bloomSrc, m_bloomTmp, m_bloom;
};
