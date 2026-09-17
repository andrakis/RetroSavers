#pragma once
#include "Saver.h"
#include "Gfx/ConstantBuffer.h"
#include "Gfx/LineRenderer2D.h"
#include "Gfx/PostProcess.h"
#include "Gfx/SpriteBatch2D.h"
#include "Gfx/Texture.h"
#include "Gfx/TrailBuffer.h"
#include <vector>

// After Dark homage: four classic modes in one saver. Starry Night (twinkling sky over a city
// whose windows light up), Warp (streaking star field), Rain (drops and ripples, refracting the
// desktop when shown over it) and Toasters (winged appliances and toast flying by).
struct AfterDarkSettings {
    enum Mode { StarryNight = 0, Warp = 1, Rain = 2, Toasters = 3 };

    int mode = Toasters;
    int density = 5;          // 1..10
    int speed = 5;            // 1..10
    bool rainOnDesktop = true;

    static AfterDarkSettings Load(const rs::Settings& s);
    void Save(rs::Settings& s) const;
};

class AfterDarkSaver : public rs::Saver {
public:
    void Initialize(rs::Device& device, const rs::SaverContext& ctx) override;
    void Update(float dt, double time) override;
    void Render(rs::Device& device, rs::SwapChain& swap) override;
    void Resize(int width, int height) override;
    std::optional<DirectX::XMFLOAT4> ClearColor() const override;

private:
    struct Star { float x, y, size, phase, rate, bright; };
    struct Building { float x, w, h; int cols, rows; std::vector<uint8_t> lit; };
    struct Meteor { float x, y, vx, vy, life; };
    struct WarpStar { float x, y, z, hue; };
    struct Drop { float x, y, vy, targetY; };
    struct Ring { float x, y, age; };
    struct Flyer { float x, y, depth, speed, flap, tumble; bool toast; };
    struct RippleCB { DirectX::XMFLOAT4 info; DirectX::XMFLOAT4 rings[32]; };

    void SetupMode(rs::Device& device);
    void UpdateStarry(float dt);
    void UpdateWarp(float dt);
    void UpdateRain(float dt);
    void UpdateToasters(float dt);
    void RenderStarry(rs::Device& device);
    void RenderWarp(rs::Device& device, rs::SwapChain& swap);
    void RenderRain(rs::Device& device);
    void RenderToasters(rs::Device& device);
    void SpawnFlyer(Flyer& f, bool anywhere);

    AfterDarkSettings m_settings;
    rs::SaverContext m_ctx;
    double m_time = 0.0;
    bool m_pendingSetup = false;   // re-run SetupMode on the next Render (after a resize)

    // Starry Night
    std::vector<Star> m_stars;
    std::vector<Building> m_buildings;
    std::vector<Meteor> m_meteors;
    float m_meteorTimer = 3.0f;
    float m_windowTimer = 0.0f;
    // Warp
    std::vector<WarpStar> m_warp;
    // Rain
    std::vector<Drop> m_drops;
    std::vector<Ring> m_rings;
    float m_dropTimer = 0.0f;
    bool m_hasDesktop = false;
    // Toasters
    std::vector<Flyer> m_flyers;

    rs::SpriteBatch2D m_sprites;
    rs::LineRenderer2D m_lines;
    rs::TrailBuffer m_trail;
    rs::PostProcess m_post;
    rs::Texture m_dot, m_toaster, m_toast, m_desktop, m_moon;
    rs::ComPtr<ID3D11PixelShader> m_ripplePs;
    rs::ConstantBuffer<RippleCB> m_rippleCb;
};
