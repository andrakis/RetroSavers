#include "GearsSaver.h"
#include "Gfx/Device.h"
#include "Gfx/Primitives.h"
#include "Gfx/SwapChain.h"
#include "Gfx/TextureFactory.h"
#include "Util/Color.h"
#include "Util/MathUtil.h"
#include <algorithm>
#include <cmath>

using namespace DirectX;
using namespace rs;

GearsSettings GearsSettings::Load(const Settings& s) {
    GearsSettings v;
    v.speed = Clamp(s.GetInt(L"Speed", v.speed), 1, 10);
    v.color1 = static_cast<COLORREF>(s.GetInt(L"Color1", static_cast<int>(v.color1)));
    v.color2 = static_cast<COLORREF>(s.GetInt(L"Color2", static_cast<int>(v.color2)));
    v.color3 = static_cast<COLORREF>(s.GetInt(L"Color3", static_cast<int>(v.color3)));
    v.wireframe = s.GetBool(L"Wireframe", v.wireframe);
    v.autoRotate = s.GetBool(L"AutoRotate", v.autoRotate);
    v.showFps = s.GetBool(L"ShowFps", v.showFps);
    return v;
}

void GearsSettings::Save(Settings& s) const {
    s.SetInt(L"Speed", speed);
    s.SetInt(L"Color1", static_cast<int>(color1));
    s.SetInt(L"Color2", static_cast<int>(color2));
    s.SetInt(L"Color3", static_cast<int>(color3));
    s.SetBool(L"Wireframe", wireframe);
    s.SetBool(L"AutoRotate", autoRotate);
    s.SetBool(L"ShowFps", showFps);
}

void GearsSaver::Initialize(Device& device, const SaverContext& ctx) {
    m_ctx = ctx;
    m_settings = GearsSettings::Load(*ctx.settings);
    m_forward.Create(device);
    m_sprites.Create(device);
    // The gears.c trio: (inner, outer, width, teeth, tooth depth).
    m_gears[0].Create(device, Primitives::Gear(1.0f, 4.0f, 1.0f, 20, 0.7f));
    m_gears[1].Create(device, Primitives::Gear(0.5f, 2.0f, 2.0f, 10, 0.7f));
    m_gears[2].Create(device, Primitives::Gear(1.3f, 2.0f, 0.5f, 10, 0.7f));
    m_time = ctx.rng->Range(0.0f, 100.0f);
    SetupCamera();
}

void GearsSaver::SetupCamera() {
    m_camera.eye = { 0, 0, -40.0f };
    m_camera.target = { 0, 0, 0 };
    m_camera.up = { 0, 1, 0 };
    // gears.c: glFrustum(-1, 1, -h, h, 5, 60) -> vertical fov from h = 1 at near 5.
    m_camera.aspect = static_cast<float>(m_ctx.width) / static_cast<float>(std::max(m_ctx.height, 1));
    m_camera.fovY = 2.0f * std::atan(1.0f / 5.0f);   // 22.6 degrees, as the original's frustum
    m_camera.nearZ = 1.0f;
    m_camera.farZ = 100.0f;
}

void GearsSaver::Update(float dt, double) {
    m_time += dt;
    // 70 degrees/second at the default, like a well-behaved glxgears at 60 fps (2 deg/frame).
    m_angle += dt * (14.0f + 14.0f * m_settings.speed);
    if (m_angle > 3600.0f) m_angle -= 3600.0f;
    if (m_settings.autoRotate) {
        float t = static_cast<float>(m_time);
        m_viewX = 20.0f + 12.0f * std::sin(t * 0.21f);
        m_viewY = 30.0f + 40.0f * std::sin(t * 0.13f);
    }
    m_fpsTimer += dt;
    ++m_frames;
    if (m_fpsTimer >= 1.0f) { m_fps = m_frames / m_fpsTimer; m_frames = 0; m_fpsTimer = 0.0f; }
}

void GearsSaver::Render(Device& device, SwapChain&) {
    ID3D11DeviceContext* ctx = device.Ctx();
    // Single white light from (5, 5, 10) in the original's eye-facing frame.
    m_forward.BeginFrame(ctx, m_camera, { 5.0f, 5.0f, -10.0f }, { 0.85f, 0.85f, 0.85f }, { 0.2f, 0.2f, 0.2f });
    const States& states = m_forward.GetStates();
    states.SetOpaque3D(ctx);
    if (m_settings.wireframe) ctx->RSSetState(states.Wireframe());

    XMMATRIX view = XMMatrixRotationX(ToRadians(m_viewX)) * XMMatrixRotationY(ToRadians(m_viewY));
    Material m;
    m.specPower = 20.0f;
    m.specIntensity = 0.3f;
    const COLORREF colors[3] = { m_settings.color1, m_settings.color2, m_settings.color3 };
    // Positions and phase offsets from gears.c.
    const XMFLOAT3 pos[3] = { { -3.0f, -2.0f, 0 }, { 3.1f, -2.0f, 0 }, { -3.1f, 4.2f, 0 } };
    const float angles[3] = { m_angle, -2.0f * m_angle - 9.0f, -2.0f * m_angle - 25.0f };
    for (int i = 0; i < 3; ++i) {
        m.color = FromColorRef(colors[i]);
        XMMATRIX world = XMMatrixRotationZ(ToRadians(angles[i])) * XMMatrixTranslation(pos[i].x, pos[i].y, pos[i].z) * view;
        m_forward.Draw(ctx, m_gears[i], world, m);
    }

    if (m_settings.showFps) {
        wchar_t buf[64];
        swprintf_s(buf, L"%.0f frames/s", m_fps);
        if (m_fpsLast != buf || !m_fpsText.Valid()) {
            int px = static_cast<int>(16 * m_ctx.dpiScale);
            m_fpsText.FromImage(device, TextureFactory::TextImage(buf, TextureFactory::MakeLogFont(L"Consolas", px)), false);
            m_fpsLast = buf;
        }
        float w = static_cast<float>(m_fpsText.Width()), h = static_cast<float>(m_fpsText.Height());
        m_sprites.Begin(m_ctx.width, m_ctx.height);
        m_sprites.Push(10 * m_ctx.dpiScale + w * 0.5f, 10 * m_ctx.dpiScale + h * 0.5f, w, h, { 0.8f, 0.8f, 0.8f, 1 });
        m_sprites.End(device, &m_fpsText, states.AlphaBlend());
    }
}

void GearsSaver::Resize(int width, int height) {
    m_ctx.width = width;
    m_ctx.height = height;
    m_ctx.viewport = { 0, 0, width, height };
    SetupCamera();
}
