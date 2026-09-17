#pragma once
#include "Saver.h"
#include "Gfx/Camera.h"
#include "Gfx/Forward.h"
#include "Gfx/LineRenderer2D.h"
#include "Gfx/Mesh.h"
#include "Gfx/Texture.h"

// Amiga Boing Ball (1984): a red/white checkered sphere on a tilted axis spins and bounces
// around a purple wire-frame room, throwing a shadow on the back wall.
struct BoingBallSettings {
    int speed = 5;            // 1..10
    int ballSize = 5;         // 1..10
    COLORREF color1 = RGB(255, 40, 40);
    COLORREF color2 = RGB(255, 255, 255);
    COLORREF gridColor = RGB(160, 80, 220);
    bool showShadow = true;

    static BoingBallSettings Load(const rs::Settings& s);
    void Save(rs::Settings& s) const;
};

class BoingBallSaver : public rs::Saver {
public:
    void Initialize(rs::Device& device, const rs::SaverContext& ctx) override;
    void Update(float dt, double time) override;
    void Render(rs::Device& device, rs::SwapChain& swap) override;
    void Resize(int width, int height) override;
    std::optional<DirectX::XMFLOAT4> ClearColor() const override { return DirectX::XMFLOAT4{ 0.66f, 0.66f, 0.66f, 1 }; }

private:
    // Room extents (world units). The camera looks down +Z at the back wall.
    static constexpr float kHalfW = 5.0f, kFloorY = -3.2f, kCeilY = 3.8f, kWallZ = 4.0f, kFrontZ = -1.5f;
    static constexpr float kBallZ = 1.6f;
    static constexpr float kTilt = 17.0f;   // degrees the spin axis leans in the screen plane

    void SetupCamera();
    bool Project(const DirectX::XMFLOAT3& p, DirectX::XMFLOAT2& out) const;
    void DrawRoom(rs::Device& device);

    BoingBallSettings m_settings;
    rs::SaverContext m_ctx;

    float m_radius = 1.3f;
    DirectX::XMFLOAT2 m_pos{ 0, 0 };
    DirectX::XMFLOAT2 m_vel{ 2.2f, 0 };
    float m_spin = 0.0f;        // current rotation about the tilted axis (radians)
    float m_gravity = 9.0f;
    float m_bounceSpeed = 8.0f;

    rs::Forward m_forward;
    rs::Camera m_camera;
    rs::Mesh m_sphere;
    rs::Texture m_checker;
    rs::Material m_ballMaterial, m_shadowMaterial;
    rs::LineRenderer2D m_lines;
    DirectX::XMFLOAT4X4 m_viewProj{};
};
