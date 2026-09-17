#include "BoingBallSaver.h"
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

// ---------------------------------------------------------------- settings

BoingBallSettings BoingBallSettings::Load(const Settings& s) {
    BoingBallSettings v;
    v.speed = Clamp(s.GetInt(L"Speed", v.speed), 1, 10);
    v.ballSize = Clamp(s.GetInt(L"BallSize", v.ballSize), 1, 10);
    v.color1 = static_cast<COLORREF>(s.GetInt(L"Color1", static_cast<int>(v.color1)));
    v.color2 = static_cast<COLORREF>(s.GetInt(L"Color2", static_cast<int>(v.color2)));
    v.gridColor = static_cast<COLORREF>(s.GetInt(L"GridColor", static_cast<int>(v.gridColor)));
    v.showShadow = s.GetBool(L"ShowShadow", v.showShadow);
    return v;
}

void BoingBallSettings::Save(Settings& s) const {
    s.SetInt(L"Speed", speed);
    s.SetInt(L"BallSize", ballSize);
    s.SetInt(L"Color1", static_cast<int>(color1));
    s.SetInt(L"Color2", static_cast<int>(color2));
    s.SetInt(L"GridColor", static_cast<int>(gridColor));
    s.SetBool(L"ShowShadow", showShadow);
}

// ---------------------------------------------------------------- lifecycle

void BoingBallSaver::Initialize(Device& device, const SaverContext& ctx) {
    m_ctx = ctx;
    m_settings = BoingBallSettings::Load(*ctx.settings);
    m_forward.Create(device);
    m_lines.Create(device);

    m_sphere.Create(device, Primitives::Sphere(1.0f, 48, 24));
    auto pack = [](COLORREF c) { return PackRgba(GetRValue(c), GetGValue(c), GetBValue(c)); };
    // 16 checks around, 8 down: the Amiga pattern.
    m_checker.FromImage(device, TextureFactory::Checker(512, 256, 16, 8, pack(m_settings.color1), pack(m_settings.color2)), true);
    m_ballMaterial.texture = &m_checker;
    m_ballMaterial.specPower = 24.0f;
    m_ballMaterial.specIntensity = 0.18f;
    m_shadowMaterial.color = { 0, 0, 0, 0.38f };
    m_shadowMaterial.specIntensity = 0.0f;

    m_radius = 0.75f + 0.11f * m_settings.ballSize;
    m_pos = { 0.0f, kFloorY + m_radius + 2.0f };
    m_vel = { 2.2f, 0.0f };
    // Bounce so the top of the arc grazes the ceiling.
    m_bounceSpeed = std::sqrt(2.0f * m_gravity * std::max(0.5f, kCeilY - kFloorY - 2.0f * m_radius - 0.2f));
    SetupCamera();
}

void BoingBallSaver::SetupCamera() {
    m_camera.eye = { 0.0f, 0.6f, -9.5f };
    m_camera.target = { 0.0f, 0.2f, kWallZ };
    m_camera.up = { 0, 1, 0 };
    m_camera.fovY = ToRadians(46.0f);
    m_camera.aspect = static_cast<float>(m_ctx.width) / static_cast<float>(std::max(m_ctx.height, 1));
    m_camera.nearZ = 0.1f;
    m_camera.farZ = 60.0f;
    XMStoreFloat4x4(&m_viewProj, m_camera.ViewProj());
}

void BoingBallSaver::Update(float dt, double) {
    dt *= 0.5f + 0.1f * m_settings.speed;
    m_vel.y -= m_gravity * dt;
    m_pos.x += m_vel.x * dt;
    m_pos.y += m_vel.y * dt;
    float minX = -kHalfW + m_radius, maxX = kHalfW - m_radius;
    if (m_pos.x < minX) { m_pos.x = minX; m_vel.x = std::fabs(m_vel.x); }
    if (m_pos.x > maxX) { m_pos.x = maxX; m_vel.x = -std::fabs(m_vel.x); }
    if (m_pos.y < kFloorY + m_radius) { m_pos.y = kFloorY + m_radius; m_vel.y = m_bounceSpeed; }
    // Spin direction follows the travel direction, as on the Amiga.
    m_spin += (m_vel.x > 0 ? -1.0f : 1.0f) * 1.6f * dt;
}

bool BoingBallSaver::Project(const XMFLOAT3& p, XMFLOAT2& out) const {
    XMVECTOR v = XMVector3Transform(XMLoadFloat3(&p), XMLoadFloat4x4(&m_viewProj));
    float w = XMVectorGetW(v);
    if (w <= 0.001f) return false;
    out.x = (XMVectorGetX(v) / w * 0.5f + 0.5f) * m_ctx.width;
    out.y = (0.5f - XMVectorGetY(v) / w * 0.5f) * m_ctx.height;
    return true;
}

void BoingBallSaver::DrawRoom(Device& device) {
    XMFLOAT4 color = FromColorRef(m_settings.gridColor);
    auto line = [&](const XMFLOAT3& a, const XMFLOAT3& b) {
        XMFLOAT2 pa, pb;
        if (Project(a, pa) && Project(b, pb)) m_lines.Line(pa.x, pa.y, pb.x, pb.y, color);
    };
    m_lines.Begin(m_ctx.width, m_ctx.height);
    // Back wall: unit squares.
    for (int x = -5; x <= 5; ++x) line({ static_cast<float>(x), kFloorY, kWallZ }, { static_cast<float>(x), kCeilY, kWallZ });
    for (float y = kFloorY; y <= kCeilY + 0.01f; y += 1.0f) line({ -kHalfW, y, kWallZ }, { kHalfW, y, kWallZ });
    // Floor: the same columns running towards the viewer, plus depth rows.
    for (int x = -5; x <= 5; ++x) line({ static_cast<float>(x), kFloorY, kWallZ }, { static_cast<float>(x), kFloorY, kFrontZ });
    for (float z = kWallZ; z >= kFrontZ - 0.01f; z -= 1.0f) line({ -kHalfW, kFloorY, z }, { kHalfW, kFloorY, z });
    m_lines.End(device);
}

void BoingBallSaver::Render(Device& device, SwapChain&) {
    ID3D11DeviceContext* ctx = device.Ctx();
    DrawRoom(device);

    // Flat, bright look: strong ambient, weak directional light from the upper left front.
    m_forward.BeginFrame(ctx, m_camera, { -0.45f, 0.6f, -0.65f }, { 0.55f, 0.55f, 0.55f }, { 0.6f, 0.6f, 0.6f });
    const States& states = m_forward.GetStates();
    states.SetOpaque3D(ctx);
    XMMATRIX world = XMMatrixScaling(m_radius, m_radius, m_radius) * XMMatrixRotationY(m_spin) *
                     XMMatrixRotationZ(ToRadians(kTilt)) * XMMatrixTranslation(m_pos.x, m_pos.y, kBallZ);
    m_forward.Draw(ctx, m_sphere, world, m_ballMaterial);

    if (m_settings.showShadow) {
        // A flattened dark disc on the back wall, offset down-right of the ball. The wall is
        // further from the camera than the ball, so the offset has to be generous to show.
        ctx->OMSetDepthStencilState(states.DepthReadOnly(), 0);
        ctx->OMSetBlendState(states.AlphaBlend(), nullptr, 0xFFFFFFFF);
        XMMATRIX shadow = XMMatrixScaling(m_radius * 1.1f, m_radius * 1.1f, 0.02f) *
                          XMMatrixTranslation(m_pos.x + m_radius * 1.1f, m_pos.y - m_radius * 0.45f, kWallZ - 0.03f);
        m_forward.Draw(ctx, m_sphere, shadow, m_shadowMaterial);
        states.SetOpaque3D(ctx);
    }
}

void BoingBallSaver::Resize(int width, int height) {
    m_ctx.width = width;
    m_ctx.height = height;
    m_ctx.viewport = { 0, 0, width, height };
    SetupCamera();
}
