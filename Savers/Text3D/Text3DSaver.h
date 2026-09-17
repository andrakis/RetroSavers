#pragma once
#include "Saver.h"
#include "Gfx/Camera.h"
#include "Gfx/Forward.h"
#include "Gfx/Mesh.h"
#include "Gfx/Texture.h"
#include <string>

// 3D Text (Windows 95 Plus! / NT / XP): extruded text or the current time tumbling and
// drifting about the screen. Geometry comes from FontMesh (DirectWrite + Direct2D).
struct Text3DSettings {
    enum Rotation { None = 0, Spin = 1, SeeSaw = 2, Wobble = 3, Tumble = 4, RandomStyle = 5 };
    enum Surface { Solid = 0, Marble = 1, Checker = 2, ImageFile = 3 };

    std::wstring text = L"RetroSavers";
    bool showTime = false;
    bool hours24 = false;
    LOGFONTW font{};
    int size = 5;               // 1..10
    int depth = 5;              // 1..10
    int resolution = 5;         // 1..10 (curve smoothness)
    int rotation = Spin;
    int speed = 5;              // 1..10
    int surface = Solid;
    std::wstring imagePath;
    COLORREF color = RGB(70, 130, 255);
    bool cycleColors = false;

    Text3DSettings();
    static Text3DSettings Load(const rs::Settings& s);
    void Save(rs::Settings& s) const;
};

class Text3DSaver : public rs::Saver {
public:
    void Initialize(rs::Device& device, const rs::SaverContext& ctx) override;
    void Update(float dt, double time) override;
    void Render(rs::Device& device, rs::SwapChain& swap) override;
    void Resize(int width, int height) override;

private:
    static constexpr float kCameraZ = 24.0f;

    std::wstring CurrentText() const;
    void BuildMesh(rs::Device& device, const std::wstring& text);
    void SetupCamera();
    void FitScale();
    DirectX::XMMATRIX RotationMatrix() const;

    Text3DSettings m_settings;
    rs::SaverContext m_ctx;
    rs::Device* m_device = nullptr;

    rs::Forward m_forward;
    rs::Camera m_camera;
    rs::Mesh m_mesh;
    rs::Texture m_texture;
    rs::Material m_material;
    std::wstring m_meshText;
    float m_textW = 1, m_textH = 1;     // mesh extents in em units
    float m_scale = 1.0f;               // em -> world
    float m_radius = 1.0f;              // bounding radius in world units

    DirectX::XMFLOAT2 m_pos{ 0, 0 };
    DirectX::XMFLOAT2 m_vel{ 0.5f, 0.3f };
    float m_halfW = 4, m_halfH = 3;     // visible half extents at the text's depth
    double m_time = 0.0;
    float m_angle = 0.0f;               // accumulated spin
    int m_style = Text3DSettings::Spin;
    float m_styleTimer = 0.0f;
    DirectX::XMFLOAT3 m_tumbleAxis{ 0, 1, 0 };
    DirectX::XMFLOAT3 m_tumbleTarget{ 0, 1, 0 };
    float m_hue = 0.0f;
    float m_clockCheck = 0.0f;
};
