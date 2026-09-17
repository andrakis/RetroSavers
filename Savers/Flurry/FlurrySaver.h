#pragma once
#include "Saver.h"
#include "Gfx/RenderTexture.h"
#include "Gfx/SpriteBatch2D.h"
#include "Gfx/Texture.h"
#include "Gfx/TrailBuffer.h"
#include <vector>

// Flurry (macOS): glowing streams wander the screen shedding short-lived particles whose
// ghosts are smeared outward by video feedback.
struct FlurrySettings {
    enum Preset { Classic = 0, RGB = 1, Fire = 2, Water = 3, Psychedelic = 4, Binary = 5 };

    int streams = 5;          // 1..12
    int preset = Classic;
    int speed = 5;            // 1..10
    int brightness = 5;       // 1..10
    int trail = 5;            // 1..10
    bool bloom = true;

    static FlurrySettings Load(const rs::Settings& s);
    void Save(rs::Settings& s) const;
};

class FlurrySaver : public rs::Saver {
public:
    void Initialize(rs::Device& device, const rs::SaverContext& ctx) override;
    void Update(float dt, double time) override;
    void Render(rs::Device& device, rs::SwapChain& swap) override;
    void Resize(int width, int height) override;
    std::optional<DirectX::XMFLOAT4> ClearColor() const override { return std::nullopt; }

private:
    struct Stream {
        float ax1, ax2, ay1, ay2;     // Lissajous amplitudes (fraction of the half extents)
        float wx1, wx2, wy1, wy2;     // angular rates
        float px1, px2, py1, py2;     // phases
        float hue, hueRate;
        DirectX::XMFLOAT2 pos, prev;
    };
    struct Particle { float x, y, vx, vy, life, maxLife, size; DirectX::XMFLOAT4 color; };
    static constexpr size_t kMaxParticles = 40000;

    void SpawnStreams();
    DirectX::XMFLOAT2 StreamPos(const Stream& s, double t) const;
    DirectX::XMFLOAT4 StreamColor(const Stream& s, int index) const;
    void CreateTargets(rs::Device& device);

    FlurrySettings m_settings;
    rs::SaverContext m_ctx;
    std::vector<Stream> m_streams;
    std::vector<Particle> m_particles;
    double m_time = 0.0;

    rs::SpriteBatch2D m_sprites;
    rs::Texture m_dot;
    rs::TrailBuffer m_trail;
    rs::RenderTexture m_bloomSrc, m_bloomTmp, m_bloom;
};
