#pragma once
#include "Saver.h"
#include "Gfx/Camera.h"
#include "Gfx/Forward.h"
#include "Gfx/Mesh.h"
#include "Gfx/Texture.h"

// 3D FlowerBox (Windows 95 Plus! / NT / XP): a subdivided cube that breathes between a pinched
// cube, a sphere and a six-pointed star while tumbling and bouncing about the screen.
struct FlowerBoxSettings {
    enum Shape { Cube = 0, Sphere = 1, Star = 2, Cycle = 3 };
    enum ColorMode { PerFace = 0, Checker = 1, CycleHues = 2 };

    int complexity = 5;       // 1..10 subdivisions per face edge
    int shape = Cycle;
    int colorMode = PerFace;
    int speed = 5;            // 1..10
    bool spin = true;
    bool bounce = true;
    int size = 5;             // 1..10

    static FlowerBoxSettings Load(const rs::Settings& s);
    void Save(rs::Settings& s) const;
};

class FlowerBoxSaver : public rs::Saver {
public:
    void Initialize(rs::Device& device, const rs::SaverContext& ctx) override;
    void Update(float dt, double time) override;
    void Render(rs::Device& device, rs::SwapChain& swap) override;
    void Resize(int width, int height) override;

private:
    static constexpr float kCameraZ = 9.0f;

    DirectX::XMFLOAT3 Surface(int face, float u, float v, float t) const;
    void BuildFace(int face, float t, rs::MeshData& out) const;
    void SetupCamera();

    FlowerBoxSettings m_settings;
    rs::SaverContext m_ctx;
    int m_n = 16;                       // grid cells per face edge
    float m_morph = 0.0f;               // -1 pinched cube .. 0 cube .. 1 sphere .. 2 star
    double m_time = 0.0;
    float m_angle = 0.0f;
    DirectX::XMFLOAT3 m_axis{ 0, 1, 0 }, m_axisTarget{ 0, 1, 0 };
    DirectX::XMFLOAT3 m_pos{ 0, 0, 0 };
    DirectX::XMFLOAT3 m_vel{ 1, 0.7f, 0 };
    float m_halfW = 4, m_halfH = 3;
    float m_radius = 1.0f;

    rs::Forward m_forward;
    rs::Camera m_camera;
    rs::Mesh m_faces[6];
    rs::MeshData m_faceData[6];
    rs::Texture m_checker;
    rs::Material m_material;
};
