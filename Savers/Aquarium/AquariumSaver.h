#pragma once
#include "Saver.h"
#include "Gfx/Billboard.h"
#include "Gfx/Camera.h"
#include "Gfx/ConstantBuffer.h"
#include "Gfx/Forward.h"
#include "Gfx/Mesh.h"
#include "Gfx/PostProcess.h"
#include "Gfx/RenderTexture.h"
#include "Gfx/Texture.h"
#include "FishMesh.h"
#include <vector>

// Aquarium: a SereneScreen / After Dark "Fish!" style tank. Procedural fish (see FishMesh)
// school and wander in front of sand dunes, rocks and swaying kelp, under animated caustics,
// light shafts and bubbles. Everything is generated - no models or textures on disk.
struct AquariumSettings {
    int fishCount = 14;          // 5..30
    int species = 0xFF;          // bitmask over kSpeciesCount species
    int plants = 6;              // 0..10
    int rocks = 5;               // 0..8
    bool bubbles = true;
    bool shafts = true;
    bool ornament = true;
    int caustics = 6;            // 0..10
    COLORREF waterTint = RGB(18, 84, 118);
    int speed = 5;               // 1..10
    int quality = 1;             // 0 low, 1 medium, 2 high

    static AquariumSettings Load(const rs::Settings& s);
    void Save(rs::Settings& s) const;
};

class AquariumSaver : public rs::Saver {
public:
    void Initialize(rs::Device& device, const rs::SaverContext& ctx) override;
    void Update(float dt, double time) override;
    void Render(rs::Device& device, rs::SwapChain& swap) override;
    void Resize(int width, int height) override;
    std::optional<DirectX::XMFLOAT4> ClearColor() const override { return std::nullopt; }

private:
    struct Fish {
        int species = 0;
        int school = -1;                 // shared wander seed for schooling groups
        float size = 1.0f;
        float speedScale = 1.0f;
        DirectX::XMFLOAT3 pos{}, vel{};  // vel is the steering state; the fish moves along its heading
        float yaw = 0.0f, pitch = 0.0f, roll = 0.0f, yawRate = 0.0f, bend = 0.0f;
        float phase = 0.0f;
        float wanderSeed = 0.0f;
        float dartTimer = 10.0f, dartLeft = 0.0f;
        float nibbleTimer = 30.0f, nibbleHold = 0.0f;
        int nibbleTarget = -1;           // plant index while approaching / nibbling
        float bubbleTimer = 20.0f;
    };
    struct Rock { DirectX::XMFLOAT3 pos; float radius; DirectX::XMFLOAT4 color; int mesh; };
    struct Kelp { DirectX::XMFLOAT3 base; float height, width, phase, freq, sway, lean; int segments; };
    struct Bubble { DirectX::XMFLOAT3 pos; float radius, rise, phase, age; };
    struct Vent { DirectX::XMFLOAT3 pos; float timer; };
    struct Shaft { float x, z, width, tilt, phase; };

    struct WaterCB { DirectX::XMFLOAT4 top, bottom, params, eye, forward, right, up, heights; };
    struct CausticsCB { DirectX::XMFLOAT4 params; };
    struct GlassCB { DirectX::XMFLOAT4 params; };
    struct FishCB {
        DirectX::XMFLOAT4 anim, anim2, base, accent, fin, tail, pattern, pattern2;
    };

    void SetupCamera();
    void BuildEnvironment(rs::Device& device);
    void SpawnFish(rs::Device& device);
    void UpdateFish(float dt);
    void UpdateKelp(rs::Device& device);
    void UpdateBubbles(float dt);
    void EmitBubble(const DirectX::XMFLOAT3& at, float radius);
    DirectX::XMFLOAT3 PlantTip(int plant) const;
    float FloorHeight(float x, float z) const;

    AquariumSettings m_settings;
    rs::SaverContext m_ctx;
    double m_time = 0.0;
    float m_simTime = 0.0f;
    float m_speedFactor = 1.0f;
    int m_quality = 1;

    // Tank volume the fish may use.
    float m_halfX = 7.0f;
    float m_yMin = 0.7f, m_yMax = 7.3f;
    float m_zMin = 1.5f, m_zMax = 8.0f;

    rs::Forward m_forward;
    rs::PostProcess m_post;
    rs::Billboard m_billboard;
    rs::Camera m_camera;

    rs::ComPtr<ID3D11PixelShader> m_waterPs, m_causticsPs, m_glassPs, m_fishPs;
    rs::ComPtr<ID3D11VertexShader> m_fishVs;
    rs::ComPtr<ID3D11InputLayout> m_fishLayout;
    rs::ConstantBuffer<WaterCB> m_waterCb;
    rs::ConstantBuffer<CausticsCB> m_causticsCb;
    rs::ConstantBuffer<GlassCB> m_glassCb;
    rs::ConstantBuffer<FishCB> m_fishCb;
    rs::RenderTexture m_caustics;

    rs::Texture m_sand, m_rockTex, m_kelpTex, m_shaftTex, m_bubbleTex;
    rs::Mesh m_floor, m_tufts, m_box, m_column;
    std::vector<rs::Mesh> m_rockMeshes;
    std::vector<Rock> m_rocks;
    std::vector<Kelp> m_kelp;
    rs::MeshData m_kelpData;
    rs::Mesh m_kelpMesh;
    bool m_kelpReady = false;
    DirectX::XMFLOAT3 m_ornamentPos{ 0, 0, 5 };
    float m_ornamentYaw = 0.0f;

    rs::Mesh m_fishMesh[kSpeciesCount];
    bool m_fishMeshReady[kSpeciesCount] = {};
    std::vector<Fish> m_fish;

    std::vector<Vent> m_vents;
    std::vector<Bubble> m_bubbles;
    std::vector<Shaft> m_shafts;

    DirectX::XMFLOAT3 m_tint{ 0.07f, 0.33f, 0.46f };
    DirectX::XMFLOAT3 m_waterTop{}, m_waterBottom{};
};
