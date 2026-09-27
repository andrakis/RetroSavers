#pragma once
#include "Saver.h"
#include "Gfx/ConstantBuffer.h"
#include "Gfx/PostProcess.h"
#include "Gfx/RenderTexture.h"
#include "Gfx/SpriteBatch2D.h"
#include "Gfx/Texture.h"
#include <string>
#include <utility>
#include <vector>

// Celestials: the camera slowly orbits one strange (or ordinary) stellar object, then
// warp-jumps to another. Each object is a fullscreen pixel shader (lensing ones march photons
// through curved space) with CPU particles drawn on top, bloom and a filmic tone curve.
// New kinds are appended so the registry keys of existing ones keep their meaning.
enum class Kind {
    BlackHole, Boson, WhiteHole, Tzo, Strange, Ember, RedDwarf, SunLike, BlueGiant, RedGiant,
    GasGiant, RingedGiant, IceGiant, HotJupiter, EarthLike, LavaWorld,
    CrateredMoon, IcyMoon, VolcanicMoon, HazyMoon,
    Pulsar, PlanetaryNebula, MassTransfer, Comet,
    Count
};

struct CelestialsSettings {
    static constexpr int kKinds = static_cast<int>(Kind::Count);
    enum Quality { Low = 0, Medium = 1, High = 2 };

    bool enabled[kKinds] = { true, true, true, true, true, true, true, true, true, true, true, true,
                             true, true, true, true, true, true, true, true, true, true, true, true };
    int seconds = 20;          // 5..120 per object
    int orbit = 5;             // 1..10
    bool disk = true;          // black hole accretion disk
    bool warp = true;          // warp jump (off = fade through black)
    bool reduceGlare = false;
    bool info = true;          // description card
    int quality = Medium;

    static const wchar_t* KindKey(int kind);
    static CelestialsSettings Load(const rs::Settings& s);
    void Save(rs::Settings& s) const;
    bool AnyEnabled() const;
};

class CelestialsSaver : public rs::Saver {
public:
    void Initialize(rs::Device& device, const rs::SaverContext& ctx) override;
    void Update(float dt, double time) override;
    void Render(rs::Device& device, rs::SwapChain& swap) override;
    void Resize(int width, int height) override;
    std::optional<DirectX::XMFLOAT4> ClearColor() const override { return std::nullopt; }

    struct V3 { float x = 0, y = 0, z = 0; };

private:
    struct SceneCB {
        DirectX::XMFLOAT4 camPos, camRight, camUp, camFwd, axis;
        DirectX::XMFLOAT4 p[6];
        DirectX::XMFLOAT4 c[6];
        DirectX::XMFLOAT4 sky, misc, sun, sunColor;
    };
    struct BrightCB { DirectX::XMFLOAT4 params; };
    struct CompositeCB { DirectX::XMFLOAT4 a, b; };

    // Everything that describes the object currently on screen.
    struct Visit {
        Kind kind = Kind::SunLike;
        int shader = 0;
        DirectX::XMFLOAT4 p[6]{}, c[6]{};
        V3 sunDir{ 0, 0, 0 };                 // planets: towards their star
        float sunSize = 0;                    // its angular radius (0 = no star)
        DirectX::XMFLOAT4 sunColor{ 1, 1, 1, 0 };
        float orbitScale = 1;                 // camera orbit speed multiplier
        float magIncl = 0.6f, magRate = 2.0f; // pulsar
        float donorDist = 5, donorRate = 0.08f, donorPhase = 0;   // mass-transfer binary
        V3 wind{ 0, 0, 0 };                   // acceleration on particles with a wind factor
        DirectX::XMFLOAT4 sky{};
        V3 axis{ 0, 1, 0 }, precess{ 0, 1, 0 };
        float seed = 0, camDist = 4, elevation = 0.2f, roll = 0, azimuth = 0, spinRate = 0.02f;
        float exposure = 1, bloom = 0.4f, wide = 0.3f, ca = 0.0015f, occluder = 1;
        float precessRate = 0;
        V3 stream{ 1, 0, 0 }, streamU{ 0, 1, 0 }, streamW{ 0, 0, 1 };   // boson dust stream frame
    };

    struct Particle {
        V3 pos, vel, col;
        float size, grow, age, life, drag, pull, streak, wind;
    };
    struct Loop { V3 a, b, col; float height, age, life; };
    struct WarpStar { float angle, r, speed, bright; };

    enum class Phase { Hold, Out, In };

    void CreateTargets(rs::Device& device);
    void StartVisit(Kind kind);
    Kind PickNext() const;
    void UpdateEmitters(float dt);
    void UpdateParticles(float dt);
    void Emit(const Particle& p);
    void Flare(float scale);
    void Cme(float scale);
    void BuildCard(rs::Device& device);
    void RenderCard(rs::Device& device);
    float CardAlpha() const;
    V3 PulsarAxis() const;
    V3 DonorPos() const;
    void RenderParticles(rs::Device& device, const V3& eye, const V3& fwd, const V3& up, const V3& right, float fovY);
    float RenderScale() const;

    CelestialsSettings m_settings;
    rs::SaverContext m_ctx;
    rs::Rng* m_rng = nullptr;

    rs::PostProcess m_post;
    rs::SpriteBatch2D m_sprites;
    rs::Texture m_dot;
    static constexpr int kShaders = 15;
    rs::ComPtr<ID3D11PixelShader> m_objectPs[kShaders];
    rs::ComPtr<ID3D11PixelShader> m_brightPs, m_compositePs;
    rs::ConstantBuffer<SceneCB> m_sceneCb;
    rs::ConstantBuffer<BrightCB> m_brightCb;
    rs::ConstantBuffer<CompositeCB> m_compositeCb;
    rs::RenderTexture m_scene, m_half, m_halfTmp, m_wide, m_wideTmp;

    Visit m_visit;
    bool m_started = false;
    float m_objTime = 0;      // seconds since the object appeared (drives the shaders)
    float m_spin = 0;         // object's own rotation angle
    float m_clock = 0;        // saver wall clock (dither seed)
    Phase m_phase = Phase::Hold;
    float m_phaseTime = 0;
    float m_warp = 0, m_fade = 1, m_dolly = 1, m_fovBoost = 0;

    std::vector<Particle> m_particles;
    std::vector<Loop> m_loops;
    std::vector<WarpStar> m_warpStars;

    // Description card: text rolled per visit, rasterised on the render thread.
    std::wstring m_infoTitle, m_infoSubtitle, m_infoBody;
    std::vector<std::pair<std::wstring, std::wstring>> m_infoStats;
    DirectX::XMFLOAT4 m_accent{ 1, 1, 1, 1 };
    rs::Texture m_card;
    int m_cardW = 0, m_cardH = 0;
    bool m_cardDirty = true;
    float m_flareTimer = 0, m_cmeTimer = 0, m_burstTimer = 0, m_emitAcc = 0, m_emitAcc2 = 0;
};
