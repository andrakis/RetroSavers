#include "Text3DSaver.h"
#include "Gfx/Device.h"
#include "Gfx/FontMesh.h"
#include "Gfx/ImageLoader.h"
#include "Gfx/SwapChain.h"
#include "Gfx/TextureFactory.h"
#include "Host/FontSettings.h"
#include "Util/Color.h"
#include "Util/MathUtil.h"
#include <algorithm>
#include <cfloat>
#include <cmath>

using namespace DirectX;
using namespace rs;

// ---------------------------------------------------------------- settings

Text3DSettings::Text3DSettings() : font(TextureFactory::MakeLogFont(L"Arial", 72, FW_BOLD)) {}

Text3DSettings Text3DSettings::Load(const Settings& s) {
    Text3DSettings v;
    v.text = s.GetString(L"Text", v.text);
    v.showTime = s.GetBool(L"ShowTime", v.showTime);
    v.hours24 = s.GetBool(L"Hours24", v.hours24);
    v.font = LoadFont(s, L"Font", v.font);
    v.size = Clamp(s.GetInt(L"Size", v.size), 1, 10);
    v.depth = Clamp(s.GetInt(L"Depth", v.depth), 1, 10);
    v.resolution = Clamp(s.GetInt(L"Resolution", v.resolution), 1, 10);
    v.rotation = Clamp(s.GetInt(L"Rotation", v.rotation), 0, 5);
    v.speed = Clamp(s.GetInt(L"Speed", v.speed), 1, 10);
    v.surface = Clamp(s.GetInt(L"Surface", v.surface), 0, 3);
    v.imagePath = s.GetString(L"ImagePath", L"");
    v.color = static_cast<COLORREF>(s.GetInt(L"Color", static_cast<int>(v.color)));
    v.cycleColors = s.GetBool(L"CycleColors", v.cycleColors);
    return v;
}

void Text3DSettings::Save(Settings& s) const {
    s.SetString(L"Text", text);
    s.SetBool(L"ShowTime", showTime);
    s.SetBool(L"Hours24", hours24);
    SaveFont(s, L"Font", font);
    s.SetInt(L"Size", size);
    s.SetInt(L"Depth", depth);
    s.SetInt(L"Resolution", resolution);
    s.SetInt(L"Rotation", rotation);
    s.SetInt(L"Speed", speed);
    s.SetInt(L"Surface", surface);
    s.SetString(L"ImagePath", imagePath);
    s.SetInt(L"Color", static_cast<int>(color));
    s.SetBool(L"CycleColors", cycleColors);
}

// ---------------------------------------------------------------- lifecycle

void Text3DSaver::Initialize(Device& device, const SaverContext& ctx) {
    m_ctx = ctx;
    m_device = &device;
    m_settings = Text3DSettings::Load(*ctx.settings);
    m_forward.Create(device);

    // Surface.
    m_material.specPower = 48.0f;
    m_material.specIntensity = 0.55f;
    std::optional<Image> img;
    switch (m_settings.surface) {
    case Text3DSettings::Marble: img = TextureFactory::Marble(); break;
    case Text3DSettings::Checker: img = TextureFactory::Checker(256, 256, 8, PackRgba(240, 240, 240), PackRgba(40, 40, 60)); break;
    case Text3DSettings::ImageFile:
        img = ImageLoader::Load(m_settings.imagePath, 2048);
        if (!img) img = TextureFactory::Marble();
        break;
    default: break;
    }
    if (img) {
        m_texture.FromImage(device, *img, true);
        m_material.texture = &m_texture;
        m_material.uvScale = 0.6f;   // uv is in em units: a little under two repeats per em
    }
    m_hue = ctx.rng->Float();
    m_material.color = m_settings.cycleColors ? HsvToRgb(m_hue, 0.8f, 1.0f) : FromColorRef(m_settings.color);
    if (m_settings.surface != Text3DSettings::Solid && !m_settings.cycleColors) m_material.color = { 1, 1, 1, 1 };

    m_style = m_settings.rotation == Text3DSettings::RandomStyle ? ctx.rng->Int(Text3DSettings::Spin, Text3DSettings::Tumble) : m_settings.rotation;
    m_styleTimer = 15.0f;
    m_tumbleTarget = { ctx.rng->Range(-1.0f, 1.0f), ctx.rng->Range(-1.0f, 1.0f), ctx.rng->Range(-0.5f, 0.5f) };
    m_tumbleAxis = m_tumbleTarget;
    float dir = ctx.rng->Range(0.0f, kTwoPi);
    m_vel = { std::cos(dir), std::sin(dir) };

    SetupCamera();
    BuildMesh(device, CurrentText());
}

std::wstring Text3DSaver::CurrentText() const {
    if (!m_settings.showTime) return m_settings.text.empty() ? L"3D Text" : m_settings.text;
    SYSTEMTIME t{};
    GetLocalTime(&t);
    wchar_t buf[32];
    if (m_settings.hours24) swprintf_s(buf, L"%02d:%02d:%02d", t.wHour, t.wMinute, t.wSecond);
    else swprintf_s(buf, L"%d:%02d:%02d %s", (t.wHour % 12) ? (t.wHour % 12) : 12, t.wMinute, t.wSecond, t.wHour < 12 ? L"AM" : L"PM");
    return buf;
}

void Text3DSaver::BuildMesh(Device& device, const std::wstring& text) {
    float depth = 0.06f + 0.06f * m_settings.depth;              // em units
    float tolerance = 0.03f / (1.0f + 0.9f * m_settings.resolution);
    MeshData data = FontMesh::Build(text, m_settings.font, 1.0f, depth, tolerance);
    if (data.vertices.empty()) data = FontMesh::Build(L"?", m_settings.font, 1.0f, depth, tolerance);
    XMFLOAT3 lo{ FLT_MAX, FLT_MAX, FLT_MAX }, hi{ -FLT_MAX, -FLT_MAX, -FLT_MAX };
    for (const auto& v : data.vertices) {
        lo.x = std::min(lo.x, v.position.x); lo.y = std::min(lo.y, v.position.y);
        hi.x = std::max(hi.x, v.position.x); hi.y = std::max(hi.y, v.position.y);
    }
    m_textW = std::max(hi.x - lo.x, 0.1f);
    m_textH = std::max(hi.y - lo.y, 0.1f);
    m_mesh.Create(device, data);
    m_meshText = text;
    FitScale();
}

void Text3DSaver::SetupCamera() {
    // Far camera with a narrow field of view keeps the perspective mild while the text spins.
    m_camera.eye = { 0, 0, -kCameraZ };
    m_camera.target = { 0, 0, 0 };
    m_camera.up = { 0, 1, 0 };
    m_camera.fovY = ToRadians(28.0f);
    m_camera.aspect = static_cast<float>(m_ctx.width) / static_cast<float>(std::max(m_ctx.height, 1));
    m_camera.nearZ = 0.1f;
    m_camera.farZ = 100.0f;
    m_halfH = kCameraZ * std::tan(m_camera.fovY * 0.5f);
    m_halfW = m_halfH * m_camera.aspect;
    FitScale();
}

void Text3DSaver::FitScale() {
    // Size 1..10 spans roughly 12%..55% of the screen height, capped so the string fits the width.
    float frac = 0.08f + 0.047f * m_settings.size;
    m_scale = frac * 2.0f * m_halfH / m_textH;
    m_scale = std::min(m_scale, 1.3f * m_halfW / m_textW);
    m_radius = 0.5f * std::sqrt(m_textW * m_textW + m_textH * m_textH) * m_scale;
    // Keep the drift inside the view for the current size.
    float bx = std::max(0.0f, m_halfW - m_radius * 0.8f), by = std::max(0.0f, m_halfH - m_radius * 0.8f);
    m_pos.x = Clamp(m_pos.x, -bx, bx);
    m_pos.y = Clamp(m_pos.y, -by, by);
}

void Text3DSaver::Update(float dt, double) {
    Rng& rng = *m_ctx.rng;
    const float k = 0.4f + 0.12f * m_settings.speed;
    m_time += dt * k;
    m_angle += dt * k * 1.2f;

    // Random style: switch every 15 s.
    if (m_settings.rotation == Text3DSettings::RandomStyle) {
        m_styleTimer -= dt;
        if (m_styleTimer <= 0.0f) { m_style = rng.Int(Text3DSettings::Spin, Text3DSettings::Tumble); m_styleTimer = 15.0f; }
    }
    // Tumble axis wanders towards a new random target.
    if (m_style == Text3DSettings::Tumble) {
        XMVECTOR a = XMLoadFloat3(&m_tumbleAxis), t = XMLoadFloat3(&m_tumbleTarget);
        if (XMVectorGetX(XMVector3Length(XMVectorSubtract(a, t))) < 0.05f)
            m_tumbleTarget = { rng.Range(-1.0f, 1.0f), rng.Range(-1.0f, 1.0f), rng.Range(-0.5f, 0.5f) };
        XMStoreFloat3(&m_tumbleAxis, XMVectorLerp(a, t, std::min(1.0f, dt * 0.3f)));
    }

    // Drift and bounce off the visible edges.
    float bx = std::max(0.0f, m_halfW - m_radius * 0.8f), by = std::max(0.0f, m_halfH - m_radius * 0.8f);
    float drift = 0.9f * k;
    m_pos.x += m_vel.x * drift * dt;
    m_pos.y += m_vel.y * drift * dt;
    if (m_pos.x < -bx) { m_pos.x = -bx; m_vel.x = std::fabs(m_vel.x); }
    if (m_pos.x > bx) { m_pos.x = bx; m_vel.x = -std::fabs(m_vel.x); }
    if (m_pos.y < -by) { m_pos.y = -by; m_vel.y = std::fabs(m_vel.y); }
    if (m_pos.y > by) { m_pos.y = by; m_vel.y = -std::fabs(m_vel.y); }

    if (m_settings.cycleColors) {
        m_hue = Wrap01(m_hue + dt * 0.03f);
        m_material.color = HsvToRgb(m_hue, 0.75f, 1.0f);
    }
    // Clock: rebuild when the second changes.
    if (m_settings.showTime) {
        m_clockCheck += dt;
        if (m_clockCheck > 0.1f) {
            m_clockCheck = 0.0f;
            std::wstring now = CurrentText();
            if (now != m_meshText && m_device) BuildMesh(*m_device, now);
        }
    }
}

XMMATRIX Text3DSaver::RotationMatrix() const {
    switch (m_style) {
    case Text3DSettings::Spin:
        return XMMatrixRotationY(m_angle);
    case Text3DSettings::SeeSaw:
        return XMMatrixRotationY(std::sin(static_cast<float>(m_time) * 0.9f) * 1.1f);
    case Text3DSettings::Wobble:
        return XMMatrixRotationX(std::sin(static_cast<float>(m_time) * 1.3f) * 0.35f) *
               XMMatrixRotationY(std::sin(static_cast<float>(m_time) * 0.7f) * 0.5f) *
               XMMatrixRotationZ(std::sin(static_cast<float>(m_time) * 1.1f) * 0.15f);
    case Text3DSettings::Tumble: {
        XMVECTOR axis = XMLoadFloat3(&m_tumbleAxis);
        if (XMVectorGetX(XMVector3LengthSq(axis)) < 1e-4f) axis = XMVectorSet(0, 1, 0, 0);
        return XMMatrixRotationAxis(XMVector3Normalize(axis), m_angle);
    }
    default:
        return XMMatrixIdentity();
    }
}

void Text3DSaver::Render(Device& device, SwapChain&) {
    ID3D11DeviceContext* ctx = device.Ctx();
    m_forward.BeginFrame(ctx, m_camera, { -0.4f, 0.55f, -0.75f }, { 0.9f, 0.9f, 0.9f }, { 0.22f, 0.22f, 0.26f });
    m_forward.GetStates().SetOpaque3D(ctx);
    XMMATRIX world = XMMatrixScaling(m_scale, m_scale, m_scale) * RotationMatrix() * XMMatrixTranslation(m_pos.x, m_pos.y, 0.0f);
    m_forward.Draw(ctx, m_mesh, world, m_material);
}

void Text3DSaver::Resize(int width, int height) {
    m_ctx.width = width;
    m_ctx.height = height;
    m_ctx.viewport = { 0, 0, width, height };
    SetupCamera();
}
