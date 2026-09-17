#pragma once
#include "Saver.h"
#include "Gfx/Camera.h"
#include "Gfx/SpriteBatch2D.h"
#include "Gfx/Texture.h"
#include "Gfx/TrailBuffer.h"
#include <vector>

// Strange attractors: thousands of particles integrated with RK4 along Lorenz, Rossler,
// Aizawa, Thomas or Halvorsen flows, projected through an orbiting camera into a fading
// additive trail.
struct AttractorsSettings {
    enum Attractor { Lorenz = 0, Rossler = 1, Aizawa = 2, Thomas = 3, Halvorsen = 4, AutoCycle = 5 };
    enum ColorMode { BySpeed = 0, ByPosition = 1, CyclingHue = 2 };

    int attractor = AutoCycle;
    int particles = 15000;        // 2000..40000
    int trail = 6;                // 1..10
    int speed = 5;                // 1..10
    int colorMode = BySpeed;

    static AttractorsSettings Load(const rs::Settings& s);
    void Save(rs::Settings& s) const;
};

class AttractorsSaver : public rs::Saver {
public:
    void Initialize(rs::Device& device, const rs::SaverContext& ctx) override;
    void Update(float dt, double time) override;
    void Render(rs::Device& device, rs::SwapChain& swap) override;
    void Resize(int width, int height) override;
    std::optional<DirectX::XMFLOAT4> ClearColor() const override { return std::nullopt; }

private:
    struct Particle { DirectX::XMFLOAT3 p; float speed; };
    struct System { DirectX::XMFLOAT3 centre; float extent; float h; float typicalSpeed; float spawn; };

    DirectX::XMFLOAT3 Flow(const DirectX::XMFLOAT3& p) const;
    void Integrate(Particle& q, float h);
    void SelectAttractor(int which);
    void Respawn(Particle& q);

    AttractorsSettings m_settings;
    rs::SaverContext m_ctx;
    int m_current = 0;
    System m_sys{};
    std::vector<Particle> m_particles;
    double m_time = 0.0;
    float m_cycleTimer = 0.0f;
    float m_fade = 1.0f;          // fades to 0 when switching attractors
    float m_azimuth = 0.0f, m_elevation = 0.3f;
    float m_hue = 0.0f;

    rs::Camera m_camera;
    rs::SpriteBatch2D m_sprites;
    rs::Texture m_dot;
    rs::TrailBuffer m_trail;
};
