#pragma once
#include "Saver.h"
#include "Gfx/Camera.h"
#include "Gfx/Forward.h"
#include "Gfx/Mesh.h"
#include "Gfx/SpriteBatch2D.h"
#include "Gfx/Texture.h"
#include <string>

// glxgears: the three meshing gears from the classic OpenGL demo, with its view slowly
// drifting and an optional frame counter.
struct GearsSettings {
    int speed = 5;            // 1..10
    COLORREF color1 = RGB(204, 25, 0);
    COLORREF color2 = RGB(0, 204, 50);
    COLORREF color3 = RGB(50, 50, 255);
    bool wireframe = false;
    bool autoRotate = true;
    bool showFps = false;

    static GearsSettings Load(const rs::Settings& s);
    void Save(rs::Settings& s) const;
};

class GearsSaver : public rs::Saver {
public:
    void Initialize(rs::Device& device, const rs::SaverContext& ctx) override;
    void Update(float dt, double time) override;
    void Render(rs::Device& device, rs::SwapChain& swap) override;
    void Resize(int width, int height) override;

private:
    void SetupCamera();

    GearsSettings m_settings;
    rs::SaverContext m_ctx;
    float m_angle = 0.0f;                 // gear 1 rotation, degrees (as in gears.c)
    float m_viewX = 20.0f, m_viewY = 30.0f;
    double m_time = 0.0;
    int m_frames = 0;
    float m_fpsTimer = 0.0f;
    float m_fps = 0.0f;

    rs::Forward m_forward;
    rs::Camera m_camera;
    rs::Mesh m_gears[3];
    rs::SpriteBatch2D m_sprites;
    rs::Texture m_fpsText;
    std::wstring m_fpsLast;
};
