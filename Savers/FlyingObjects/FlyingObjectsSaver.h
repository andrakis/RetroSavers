#pragma once
#include "Saver.h"
#include "Gfx/Camera.h"
#include "Gfx/Forward.h"
#include "Gfx/Mesh.h"
#include "Gfx/Texture.h"
#include <string>
#include <vector>

// 3D Flying Objects (Windows NT / 95 Plus!): one deforming object tumbling through space —
// ribbon, two ribbons, twist, splash, explode, textured flag or logo — rebuilt every frame on
// a dynamic mesh.
struct FlyingObjectsSettings {
    enum Style { Ribbon = 0, TwoRibbons = 1, Twist = 2, Splash = 3, Explode = 4, Flag = 5, Logo = 6, CycleStyles = 7 };
    enum ColorMode { Rainbow = 0, Solid = 1, Checker = 2 };

    int style = CycleStyles;
    int colorMode = Rainbow;
    std::wstring texturePath;     // flag / logo image; empty = procedural emblem
    int resolution = 5;           // 1..10
    int speed = 5;                // 1..10
    int size = 5;                 // 1..10
    bool wireframe = false;

    static FlyingObjectsSettings Load(const rs::Settings& s);
    void Save(rs::Settings& s) const;
};

class FlyingObjectsSaver : public rs::Saver {
public:
    void Initialize(rs::Device& device, const rs::SaverContext& ctx) override;
    void Update(float dt, double time) override;
    void Render(rs::Device& device, rs::SwapChain& swap) override;
    void Resize(int width, int height) override;

private:
    struct Section { DirectX::XMFLOAT3 centre, half; };   // ribbon cross-section: centre and half-width vector
    struct Ripple { DirectX::XMFLOAT2 at; float age; };
    struct Tri { DirectX::XMFLOAT3 centroid, normal, axis; float spin; };

    void SelectStyle(int style);
    void BuildRibbon(int which, float t);
    void BuildTwist(float t);
    void BuildSplash(float t);
    void BuildExplode(float t);
    void BuildFlag(float t);
    void BuildLogo(float t);
    DirectX::XMFLOAT3 Emitter(int which, float t) const;
    void SetupCamera();

    FlyingObjectsSettings m_settings;
    rs::SaverContext m_ctx;
    int m_style = 0;
    float m_styleTimer = 0.0f;
    double m_time = 0.0;
    float m_angle = 0.0f;
    DirectX::XMFLOAT3 m_axis{ 0, 1, 0 }, m_axisTarget{ 0, 1, 0 };
    int m_res = 20;
    float m_size = 1.0f;
    DirectX::XMFLOAT4 m_solid{ 0.3f, 0.5f, 1.0f, 1.0f };

    // Style state.
    std::vector<Section> m_ribbon[2];
    std::vector<Ripple> m_ripples;
    float m_rippleTimer = 0.0f;
    rs::MeshData m_sphereBase;        // de-indexed sphere for Explode
    std::vector<Tri> m_tris;

    rs::MeshData m_data[2];
    rs::Mesh m_mesh[2];
    bool m_meshReady[2] = { false, false };
    rs::Mesh m_box;
    rs::Forward m_forward;
    rs::Camera m_camera;
    rs::Texture m_palette, m_checker, m_image;
    rs::Material m_material;
};
