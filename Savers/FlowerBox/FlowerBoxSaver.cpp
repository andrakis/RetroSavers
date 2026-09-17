#include "FlowerBoxSaver.h"
#include "Gfx/Device.h"
#include "Gfx/SwapChain.h"
#include "Gfx/TextureFactory.h"
#include "Util/Color.h"
#include "Util/MathUtil.h"
#include <algorithm>
#include <cmath>

using namespace DirectX;
using namespace rs;

// ---------------------------------------------------------------- settings

FlowerBoxSettings FlowerBoxSettings::Load(const Settings& s) {
    FlowerBoxSettings v;
    v.complexity = Clamp(s.GetInt(L"Complexity", v.complexity), 1, 10);
    v.shape = Clamp(s.GetInt(L"Shape", v.shape), 0, 3);
    v.colorMode = Clamp(s.GetInt(L"ColorMode", v.colorMode), 0, 2);
    v.speed = Clamp(s.GetInt(L"Speed", v.speed), 1, 10);
    v.spin = s.GetBool(L"Spin", v.spin);
    v.bounce = s.GetBool(L"Bounce", v.bounce);
    v.size = Clamp(s.GetInt(L"Size", v.size), 1, 10);
    return v;
}

void FlowerBoxSettings::Save(Settings& s) const {
    s.SetInt(L"Complexity", complexity);
    s.SetInt(L"Shape", shape);
    s.SetInt(L"ColorMode", colorMode);
    s.SetInt(L"Speed", speed);
    s.SetBool(L"Spin", spin);
    s.SetBool(L"Bounce", bounce);
    s.SetInt(L"Size", size);
}

// ---------------------------------------------------------------- geometry

namespace {

// Face frames: normal, tangent along u, tangent along v.
const XMFLOAT3 kFaceN[6] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
const XMFLOAT3 kFaceU[6] = { { 0, 0, -1 }, { 0, 0, 1 }, { 1, 0, 0 }, { 1, 0, 0 }, { 1, 0, 0 }, { -1, 0, 0 } };
const XMFLOAT3 kFaceV[6] = { { 0, 1, 0 }, { 0, 1, 0 }, { 0, 0, -1 }, { 0, 0, 1 }, { 0, 1, 0 }, { 0, 1, 0 } };
const XMFLOAT4 kFaceColors[6] = { { 0.9f, 0.15f, 0.15f, 1 }, { 0.15f, 0.8f, 0.2f, 1 }, { 0.2f, 0.35f, 0.95f, 1 },
                                  { 0.95f, 0.85f, 0.15f, 1 }, { 0.85f, 0.2f, 0.85f, 1 }, { 0.15f, 0.85f, 0.85f, 1 } };

} // namespace

XMFLOAT3 FlowerBoxSaver::Surface(int face, float u, float v, float t) const {
    // Cube point and its projection onto the unit sphere.
    XMFLOAT3 c{ kFaceN[face].x + kFaceU[face].x * u + kFaceV[face].x * v,
                kFaceN[face].y + kFaceU[face].y * u + kFaceV[face].y * v,
                kFaceN[face].z + kFaceU[face].z * u + kFaceV[face].z * v };
    float len = std::sqrt(c.x * c.x + c.y * c.y + c.z * c.z);
    XMFLOAT3 s{ c.x / len, c.y / len, c.z / len };
    // Bulge is 1 at the face centre and 0 along its edges, so neighbours stay stitched.
    float bulge = std::cos(u * kPi * 0.5f) * std::cos(v * kPi * 0.5f);
    if (t < 0.0f) {                       // pinched cube: faces sink towards the centre
        float k = t * 0.8f * bulge;
        return { c.x + kFaceN[face].x * k, c.y + kFaceN[face].y * k, c.z + kFaceN[face].z * k };
    }
    if (t <= 1.0f) {                      // cube -> sphere
        return { Lerp(c.x, s.x, t), Lerp(c.y, s.y, t), Lerp(c.z, s.z, t) };
    }
    float k = (t - 1.0f) * 1.3f * bulge;  // sphere -> six-pointed star
    return { s.x + s.x * k, s.y + s.y * k, s.z + s.z * k };
}

void FlowerBoxSaver::BuildFace(int face, float t, MeshData& out) const {
    const int n = m_n;
    const float eps = 1.0f / n * 0.5f;
    out.vertices.resize(static_cast<size_t>(n + 1) * (n + 1));
    for (int j = 0; j <= n; ++j) {
        for (int i = 0; i <= n; ++i) {
            float u = -1.0f + 2.0f * i / n, v = -1.0f + 2.0f * j / n;
            VertexPNT& vert = out.vertices[static_cast<size_t>(j) * (n + 1) + i];
            vert.position = Surface(face, u, v, t);
            // Normal from parametric finite differences, one-sided at the face edges so the
            // samples never extrapolate past the face plane.
            float ua = std::max(u - eps, -1.0f), ub = std::min(u + eps, 1.0f);
            float va = std::max(v - eps, -1.0f), vb = std::min(v + eps, 1.0f);
            XMFLOAT3 pu0 = Surface(face, ua, v, t), pu1 = Surface(face, ub, v, t);
            XMFLOAT3 pv0 = Surface(face, u, va, t), pv1 = Surface(face, u, vb, t);
            XMVECTOR du = XMVectorSet(pu1.x - pu0.x, pu1.y - pu0.y, pu1.z - pu0.z, 0);
            XMVECTOR dv = XMVectorSet(pv1.x - pv0.x, pv1.y - pv0.y, pv1.z - pv0.z, 0);
            XMVECTOR nrm = XMVector3Normalize(XMVector3Cross(du, dv));
            XMVECTOR outward = XMVectorSet(kFaceN[face].x, kFaceN[face].y, kFaceN[face].z, 0);
            if (XMVectorGetX(XMVector3Dot(nrm, outward)) < 0.0f) nrm = XMVectorNegate(nrm);
            XMStoreFloat3(&vert.normal, nrm);
            vert.uv = { (u + 1.0f) * 0.5f, (v + 1.0f) * 0.5f };
        }
    }
    if (out.indices.empty()) {
        for (int j = 0; j < n; ++j)
            for (int i = 0; i < n; ++i) {
                uint32_t a = static_cast<uint32_t>(j * (n + 1) + i), b = a + 1, c = a + (n + 1), d = c + 1;
                out.indices.insert(out.indices.end(), { a, b, c, b, d, c });
            }
        out.FixWinding();
    }
}

// ---------------------------------------------------------------- lifecycle

void FlowerBoxSaver::Initialize(Device& device, const SaverContext& ctx) {
    m_ctx = ctx;
    m_settings = FlowerBoxSettings::Load(*ctx.settings);
    m_forward.Create(device);
    m_n = 4 + 3 * m_settings.complexity;
    m_radius = 0.55f + 0.11f * m_settings.size;

    if (m_settings.colorMode == FlowerBoxSettings::Checker) {
        m_checker.FromImage(device, TextureFactory::Checker(256, 256, 4, PackRgba(245, 245, 245), PackRgba(30, 30, 40)), true);
        m_material.texture = &m_checker;
    }
    m_material.specPower = 40.0f;
    m_material.specIntensity = 0.5f;

    Rng& rng = *ctx.rng;
    m_axisTarget = { rng.Range(-1.0f, 1.0f), rng.Range(-1.0f, 1.0f), rng.Range(-1.0f, 1.0f) };
    m_axis = m_axisTarget;
    float dir = rng.Range(0.0f, kTwoPi);
    m_vel = { std::cos(dir), std::sin(dir), 0 };
    m_time = rng.Range(0.0f, 100.0f);

    for (int f = 0; f < 6; ++f) {
        BuildFace(f, 0.0f, m_faceData[f]);
        m_faces[f].CreateDynamic(device, m_faceData[f]);
    }
    SetupCamera();
}

void FlowerBoxSaver::SetupCamera() {
    m_camera.eye = { 0, 0, -kCameraZ };
    m_camera.target = { 0, 0, 0 };
    m_camera.up = { 0, 1, 0 };
    m_camera.fovY = ToRadians(40.0f);
    m_camera.aspect = static_cast<float>(m_ctx.width) / static_cast<float>(std::max(m_ctx.height, 1));
    m_camera.nearZ = 0.1f;
    m_camera.farZ = 100.0f;
    m_halfH = kCameraZ * std::tan(m_camera.fovY * 0.5f);
    m_halfW = m_halfH * m_camera.aspect;
}

void FlowerBoxSaver::Update(float dt, double) {
    Rng& rng = *m_ctx.rng;
    const float k = 0.4f + 0.12f * m_settings.speed;
    m_time += dt * k;
    const float t = static_cast<float>(m_time);

    // Morph target range per shape; a slow sine breathes through it.
    float lo = -1.0f, hi = 2.0f;
    switch (m_settings.shape) {
    case FlowerBoxSettings::Cube: lo = -0.9f; hi = 0.25f; break;
    case FlowerBoxSettings::Sphere: lo = 0.4f; hi = 1.05f; break;
    case FlowerBoxSettings::Star: lo = 0.9f; hi = 2.0f; break;
    default: break;
    }
    m_morph = Lerp(lo, hi, 0.5f + 0.5f * std::sin(t * 0.45f));

    if (m_settings.spin) {
        m_angle += dt * k * 0.9f;
        XMVECTOR a = XMLoadFloat3(&m_axis), tg = XMLoadFloat3(&m_axisTarget);
        if (XMVectorGetX(XMVector3Length(XMVectorSubtract(a, tg))) < 0.05f)
            m_axisTarget = { rng.Range(-1.0f, 1.0f), rng.Range(-1.0f, 1.0f), rng.Range(-1.0f, 1.0f) };
        XMStoreFloat3(&m_axis, XMVectorLerp(a, tg, std::min(1.0f, dt * 0.25f)));
    }
    if (m_settings.bounce) {
        float extent = m_radius * 1.6f;   // star spikes reach past the unit cube
        float bx = std::max(0.0f, m_halfW - extent), by = std::max(0.0f, m_halfH - extent);
        float drift = 1.2f * k;
        m_pos.x += m_vel.x * drift * dt;
        m_pos.y += m_vel.y * drift * dt;
        if (m_pos.x < -bx) { m_pos.x = -bx; m_vel.x = std::fabs(m_vel.x); }
        if (m_pos.x > bx) { m_pos.x = bx; m_vel.x = -std::fabs(m_vel.x); }
        if (m_pos.y < -by) { m_pos.y = -by; m_vel.y = std::fabs(m_vel.y); }
        if (m_pos.y > by) { m_pos.y = by; m_vel.y = -std::fabs(m_vel.y); }
    }
}

void FlowerBoxSaver::Render(Device& device, SwapChain&) {
    ID3D11DeviceContext* ctx = device.Ctx();
    for (int f = 0; f < 6; ++f) {
        BuildFace(f, m_morph, m_faceData[f]);
        m_faces[f].Update(device, m_faceData[f]);
    }
    m_forward.BeginFrame(ctx, m_camera, { -0.4f, 0.6f, -0.7f }, { 0.85f, 0.85f, 0.85f }, { 0.25f, 0.25f, 0.28f });
    m_forward.GetStates().SetOpaque3D(ctx);
    XMVECTOR axis = XMLoadFloat3(&m_axis);
    if (XMVectorGetX(XMVector3LengthSq(axis)) < 1e-4f) axis = XMVectorSet(0, 1, 0, 0);
    XMMATRIX world = XMMatrixScaling(m_radius, m_radius, m_radius) *
                     (m_settings.spin ? XMMatrixRotationAxis(XMVector3Normalize(axis), m_angle) : XMMatrixRotationRollPitchYaw(0.4f, 0.6f, 0.0f)) *
                     XMMatrixTranslation(m_pos.x, m_pos.y, 0.0f);
    for (int f = 0; f < 6; ++f) {
        Material m = m_material;
        switch (m_settings.colorMode) {
        case FlowerBoxSettings::Checker: m.color = { 1, 1, 1, 1 }; break;
        case FlowerBoxSettings::CycleHues: m.color = HsvToRgb(Wrap01(static_cast<float>(m_time) * 0.05f + f / 6.0f), 0.85f, 1.0f); break;
        default: m.color = kFaceColors[f]; break;
        }
        m_forward.Draw(ctx, m_faces[f], world, m);
    }
}

void FlowerBoxSaver::Resize(int width, int height) {
    m_ctx.width = width;
    m_ctx.height = height;
    m_ctx.viewport = { 0, 0, width, height };
    SetupCamera();
}
