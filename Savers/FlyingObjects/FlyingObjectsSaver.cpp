#include "FlyingObjectsSaver.h"
#include "Gfx/Device.h"
#include "Gfx/ImageLoader.h"
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

FlyingObjectsSettings FlyingObjectsSettings::Load(const Settings& s) {
    FlyingObjectsSettings v;
    v.style = Clamp(s.GetInt(L"Style", v.style), 0, 7);
    v.colorMode = Clamp(s.GetInt(L"ColorMode", v.colorMode), 0, 2);
    v.texturePath = s.GetString(L"TexturePath", L"");
    v.resolution = Clamp(s.GetInt(L"Resolution", v.resolution), 1, 10);
    v.speed = Clamp(s.GetInt(L"Speed", v.speed), 1, 10);
    v.size = Clamp(s.GetInt(L"Size", v.size), 1, 10);
    v.wireframe = s.GetBool(L"Wireframe", v.wireframe);
    return v;
}

void FlyingObjectsSettings::Save(Settings& s) const {
    s.SetInt(L"Style", style);
    s.SetInt(L"ColorMode", colorMode);
    s.SetString(L"TexturePath", texturePath);
    s.SetInt(L"Resolution", resolution);
    s.SetInt(L"Speed", speed);
    s.SetInt(L"Size", size);
    s.SetBool(L"Wireframe", wireframe);
}

// ---------------------------------------------------------------- lifecycle

void FlyingObjectsSaver::Initialize(Device& device, const SaverContext& ctx) {
    m_ctx = ctx;
    m_settings = FlyingObjectsSettings::Load(*ctx.settings);
    m_forward.Create(device);
    m_res = 6 + 2 * m_settings.resolution;
    m_size = 0.55f + 0.12f * m_settings.size;

    // Rainbow palette strip, checker, and the flag / logo image.
    Image pal(256, 1);
    for (int x = 0; x < 256; ++x) { XMFLOAT4 c = HsvToRgb(x / 256.0f, 0.9f, 1.0f); pal.At(x, 0) = PackRgbaF(c.x, c.y, c.z); }
    m_palette.FromImage(device, pal, false);
    m_checker.FromImage(device, TextureFactory::Checker(256, 256, 8, PackRgba(240, 240, 240), PackRgba(40, 40, 60)), true);
    std::optional<Image> img;
    if (!m_settings.texturePath.empty()) img = ImageLoader::Load(m_settings.texturePath, 1024);
    if (!img) img = TextureFactory::Emblem(256);
    m_image.FromImage(device, *img, true);
    m_box.Create(device, Primitives::Box(0.9f, 0.9f, 0.12f));
    m_material.specPower = 32.0f;
    m_material.specIntensity = 0.45f;

    Rng& rng = *ctx.rng;
    m_axisTarget = { rng.Range(-1.0f, 1.0f), rng.Range(-1.0f, 1.0f), rng.Range(-1.0f, 1.0f) };
    m_axis = m_axisTarget;
    m_time = rng.Range(0.0f, 100.0f);
    SelectStyle(m_settings.style == FlyingObjectsSettings::CycleStyles ? rng.Int(0, 6) : m_settings.style);
    SetupCamera();
}

void FlyingObjectsSaver::SelectStyle(int style) {
    m_style = style;
    m_styleTimer = 0.0f;
    m_solid = HsvToRgb(m_ctx.rng->Float(), 0.75f, 1.0f);
    for (auto& r : m_ribbon) r.clear();
    m_ripples.clear();
    m_meshReady[0] = m_meshReady[1] = false;
    if (style == FlyingObjectsSettings::Explode) {
        MeshData sphere = Primitives::Sphere(1.3f * m_size, std::max(8, m_res), std::max(6, m_res / 2 + 2));
        // De-index so every triangle owns its vertices and can fly independently.
        m_sphereBase = MeshData{};
        m_tris.clear();
        Rng& rng = *m_ctx.rng;
        for (size_t i = 0; i + 2 < sphere.indices.size(); i += 3) {
            Tri t{};
            XMVECTOR c = XMVectorZero();
            for (int k = 0; k < 3; ++k) {
                const VertexPNT& v = sphere.vertices[sphere.indices[i + k]];
                m_sphereBase.vertices.push_back(v);
                m_sphereBase.indices.push_back(static_cast<uint32_t>(m_sphereBase.vertices.size() - 1));
                c = XMVectorAdd(c, XMLoadFloat3(&v.position));
            }
            c = XMVectorScale(c, 1.0f / 3.0f);
            XMStoreFloat3(&t.centroid, c);
            XMStoreFloat3(&t.normal, XMVector3Normalize(c));
            t.axis = { rng.Range(-1.0f, 1.0f), rng.Range(-1.0f, 1.0f), rng.Range(-1.0f, 1.0f) };
            t.spin = rng.Range(-4.0f, 4.0f);
            m_tris.push_back(t);
        }
    }
}

void FlyingObjectsSaver::SetupCamera() {
    m_camera.eye = { 0, 0, -10.0f };
    m_camera.target = { 0, 0, 0 };
    m_camera.up = { 0, 1, 0 };
    m_camera.fovY = ToRadians(45.0f);
    m_camera.aspect = static_cast<float>(m_ctx.width) / static_cast<float>(std::max(m_ctx.height, 1));
    m_camera.nearZ = 0.1f;
    m_camera.farZ = 100.0f;
}

// ---------------------------------------------------------------- geometry builders

XMFLOAT3 FlyingObjectsSaver::Emitter(int which, float t) const {
    float p = which * 2.1f;
    return { 2.4f * std::sin(t * 0.9f + p) * std::cos(t * 0.31f), 1.6f * std::sin(t * 1.3f + 1.0f + p), 1.2f * std::cos(t * 0.7f + p) };
}

void FlyingObjectsSaver::BuildRibbon(int which, float t) {
    std::vector<Section>& sections = m_ribbon[which];
    const size_t maxSections = static_cast<size_t>(60 + 10 * m_res);
    XMFLOAT3 c = Emitter(which, t), ahead = Emitter(which, t + 0.02f);
    XMVECTOR v = XMVector3Normalize(XMVectorSubtract(XMLoadFloat3(&ahead), XMLoadFloat3(&c)));
    XMVECTOR side = XMVector3Normalize(XMVector3Cross(v, XMVectorSet(0, 1, 0, 0)));
    XMVECTOR up = XMVector3Cross(side, v);
    float theta = t * 1.4f + which;
    XMVECTOR half = XMVectorScale(XMVectorAdd(XMVectorScale(side, std::cos(theta)), XMVectorScale(up, std::sin(theta))), 0.32f * m_size);
    Section s;
    s.centre = c;
    XMStoreFloat3(&s.half, half);
    sections.push_back(s);
    if (sections.size() > maxSections) sections.erase(sections.begin());

    MeshData& d = m_data[which];
    d.vertices.clear();
    d.indices.clear();
    const size_t n = sections.size();
    for (size_t i = 0; i < n; ++i) {
        const Section& sec = sections[i];
        float u = static_cast<float>(i) / std::max<size_t>(n - 1, 1);
        d.vertices.push_back({ { sec.centre.x + sec.half.x, sec.centre.y + sec.half.y, sec.centre.z + sec.half.z }, { 0, 1, 0 }, { u, 0 } });
        d.vertices.push_back({ { sec.centre.x - sec.half.x, sec.centre.y - sec.half.y, sec.centre.z - sec.half.z }, { 0, 1, 0 }, { u, 1 } });
    }
    for (uint32_t i = 0; i + 1 < n; ++i) {
        uint32_t a = i * 2, b = a + 1, c2 = a + 2, dd = a + 3;
        d.indices.insert(d.indices.end(), { a, c2, b, b, c2, dd });
    }
    if (n >= 2) d.ComputeNormals();
}

void FlyingObjectsSaver::BuildTwist(float t) {
    MeshData& d = m_data[0];
    d.vertices.clear();
    d.indices.clear();
    const int m = 3 * m_res;
    const float L = 2.4f * m_size, w = 0.55f * m_size;
    for (int i = 0; i <= m; ++i) {
        float z = -L + 2.0f * L * i / m;
        float a = z * 1.6f / m_size + t * 1.5f;
        float u = static_cast<float>(i) / m;
        d.vertices.push_back({ { w * std::cos(a), w * std::sin(a), z }, { 0, 0, 1 }, { u, 0 } });
        d.vertices.push_back({ { -w * std::cos(a), -w * std::sin(a), z }, { 0, 0, 1 }, { u, 1 } });
    }
    for (uint32_t i = 0; i < static_cast<uint32_t>(m); ++i) {
        uint32_t a = i * 2, b = a + 1, c = a + 2, dd = a + 3;
        d.indices.insert(d.indices.end(), { a, c, b, b, c, dd });
    }
    d.ComputeNormals();
}

void FlyingObjectsSaver::BuildSplash(float t) {
    MeshData& d = m_data[0];
    d.vertices.clear();
    d.indices.clear();
    const int n = 2 * m_res;
    const float ext = 2.6f * m_size;
    for (int j = 0; j <= n; ++j) {
        for (int i = 0; i <= n; ++i) {
            float x = -ext + 2.0f * ext * i / n, z = -ext + 2.0f * ext * j / n;
            float y = 0.0f;
            for (const Ripple& r : m_ripples) {
                float dx = x - r.at.x, dz = z - r.at.y;
                float dist = std::sqrt(dx * dx + dz * dz);
                float front = r.age * 2.2f * m_size;
                if (dist > front) continue;
                y += 0.28f * m_size * std::exp(-r.age * 0.7f) * std::sin(dist * 5.0f / m_size - r.age * 9.0f) * (1.0f - dist / std::max(front, 0.01f));
            }
            float u = m_settings.colorMode == FlyingObjectsSettings::Checker ? static_cast<float>(i) / n : Saturate(0.5f + y / (0.6f * m_size));
            float v = m_settings.colorMode == FlyingObjectsSettings::Checker ? static_cast<float>(j) / n : 0.5f;
            d.vertices.push_back({ { x, y, z }, { 0, 1, 0 }, { u, v } });
        }
    }
    for (uint32_t j = 0; j < static_cast<uint32_t>(n); ++j)
        for (uint32_t i = 0; i < static_cast<uint32_t>(n); ++i) {
            uint32_t a = j * (n + 1) + i, b = a + 1, c = a + n + 1, dd = c + 1;
            d.indices.insert(d.indices.end(), { a, b, c, b, dd, c });
        }
    d.ComputeNormals();
    (void)t;
}

void FlyingObjectsSaver::BuildExplode(float t) {
    MeshData& d = m_data[0];
    d = m_sphereBase;
    // Envelope: whole, blown apart, reassembled, over ~9 s.
    float e = 0.5f - 0.5f * std::cos(t * 0.7f);
    e = e * e * (3.0f - 2.0f * e);
    for (size_t k = 0; k < m_tris.size(); ++k) {
        const Tri& tri = m_tris[k];
        XMVECTOR c = XMLoadFloat3(&tri.centroid);
        XMVECTOR axis = XMLoadFloat3(&tri.axis);
        if (XMVectorGetX(XMVector3LengthSq(axis)) < 1e-4f) axis = XMVectorSet(0, 1, 0, 0);
        XMMATRIX rot = XMMatrixRotationAxis(XMVector3Normalize(axis), tri.spin * e);
        XMVECTOR offset = XMVectorScale(XMLoadFloat3(&tri.normal), e * 3.0f * m_size);
        for (int i = 0; i < 3; ++i) {
            VertexPNT& v = d.vertices[k * 3 + i];
            XMVECTOR p = XMLoadFloat3(&v.position);
            p = XMVectorAdd(XMVectorAdd(XMVector3Transform(XMVectorSubtract(p, c), rot), c), offset);
            XMStoreFloat3(&v.position, p);
            v.uv = { static_cast<float>(k % 17) / 17.0f, 0.5f };
        }
    }
    d.ComputeNormals();
}

void FlyingObjectsSaver::BuildFlag(float t) {
    MeshData& d = m_data[0];
    d.vertices.clear();
    d.indices.clear();
    const int nx = 2 * m_res, ny = std::max(4, (4 * m_res) / 3);
    const float W = 3.2f * m_size, H = 2.1f * m_size;
    for (int j = 0; j <= ny; ++j)
        for (int i = 0; i <= nx; ++i) {
            float u = static_cast<float>(i) / nx, v = static_cast<float>(j) / ny;
            float x = -W * 0.5f + W * u, y = H * 0.5f - H * v;
            // The wave grows towards the free (right) edge; the left edge is on the pole.
            float z = 0.4f * m_size * u * std::sin(u * 7.0f - t * 4.0f + v * 1.5f);
            d.vertices.push_back({ { x, y, z }, { 0, 0, -1 }, { u, v } });
        }
    for (uint32_t j = 0; j < static_cast<uint32_t>(ny); ++j)
        for (uint32_t i = 0; i < static_cast<uint32_t>(nx); ++i) {
            uint32_t a = j * (nx + 1) + i, b = a + 1, c = a + nx + 1, dd = c + 1;
            d.indices.insert(d.indices.end(), { a, b, c, b, dd, c });
        }
    d.ComputeNormals();
}

void FlyingObjectsSaver::BuildLogo(float) {
    // Static geometry: the four panes are drawn as boxes in Render.
}

// ---------------------------------------------------------------- update / render

void FlyingObjectsSaver::Update(float dt, double) {
    Rng& rng = *m_ctx.rng;
    const float k = 0.4f + 0.12f * m_settings.speed;
    m_time += dt * k;
    m_angle += dt * k * 0.7f;
    m_styleTimer += dt;
    if (m_settings.style == FlyingObjectsSettings::CycleStyles && m_styleTimer > 30.0f) SelectStyle((m_style + 1) % 7);

    XMVECTOR a = XMLoadFloat3(&m_axis), tg = XMLoadFloat3(&m_axisTarget);
    if (XMVectorGetX(XMVector3Length(XMVectorSubtract(a, tg))) < 0.05f)
        m_axisTarget = { rng.Range(-1.0f, 1.0f), rng.Range(-1.0f, 1.0f), rng.Range(-1.0f, 1.0f) };
    XMStoreFloat3(&m_axis, XMVectorLerp(a, tg, std::min(1.0f, dt * 0.2f)));

    if (m_style == FlyingObjectsSettings::Splash) {
        for (auto& r : m_ripples) r.age += dt * k;
        m_ripples.erase(std::remove_if(m_ripples.begin(), m_ripples.end(), [](const Ripple& r) { return r.age > 6.0f; }), m_ripples.end());
        m_rippleTimer -= dt * k;
        if (m_rippleTimer <= 0.0f) {
            float ext = 2.0f * m_size;
            m_ripples.push_back({ { rng.Range(-ext, ext), rng.Range(-ext, ext) }, 0.0f });
            m_rippleTimer = rng.Range(0.5f, 1.4f);
        }
    }
}

void FlyingObjectsSaver::Render(Device& device, SwapChain&) {
    ID3D11DeviceContext* ctx = device.Ctx();
    const float t = static_cast<float>(m_time);
    const States& states = m_forward.GetStates();

    // Rebuild the object for this frame.
    int meshes = 1;
    switch (m_style) {
    case FlyingObjectsSettings::Ribbon: BuildRibbon(0, t); break;
    case FlyingObjectsSettings::TwoRibbons: BuildRibbon(0, t); BuildRibbon(1, t); meshes = 2; break;
    case FlyingObjectsSettings::Twist: BuildTwist(t); break;
    case FlyingObjectsSettings::Splash: BuildSplash(t); break;
    case FlyingObjectsSettings::Explode: BuildExplode(t); break;
    case FlyingObjectsSettings::Flag: BuildFlag(t); break;
    default: meshes = 0; break;
    }
    for (int i = 0; i < meshes; ++i) {
        if (m_data[i].vertices.size() < 3) { m_meshReady[i] = false; continue; }
        // The index layout changes while a ribbon grows; recreate then, otherwise just re-upload.
        if (!m_meshReady[i] || m_mesh[i].IndexCount() != m_data[i].indices.size()) { m_mesh[i].CreateDynamic(device, m_data[i]); m_meshReady[i] = true; }
        else m_mesh[i].Update(device, m_data[i]);
    }

    // Material by colour mode (flag always shows its image).
    Material m = m_material;
    m.color = { 1, 1, 1, 1 };
    m.sampler = states.LinearClamp();
    if (m_style == FlyingObjectsSettings::Flag) m.texture = &m_image;
    else if (m_settings.colorMode == FlyingObjectsSettings::Rainbow) m.texture = &m_palette;
    else if (m_settings.colorMode == FlyingObjectsSettings::Checker) { m.texture = &m_checker; m.sampler = states.LinearWrap(); m.uvScale = m_style == FlyingObjectsSettings::Splash ? 1.0f : 2.0f; }
    else { m.texture = nullptr; m.color = m_solid; }

    m_forward.BeginFrame(ctx, m_camera, { -0.4f, 0.6f, -0.7f }, { 0.75f, 0.75f, 0.75f }, { 0.35f, 0.35f, 0.38f });
    states.SetOpaque3D(ctx);
    ctx->RSSetState(m_settings.wireframe ? states.Wireframe() : states.CullNone());   // sheets are two-sided

    XMVECTOR axis = XMLoadFloat3(&m_axis);
    if (XMVectorGetX(XMVector3LengthSq(axis)) < 1e-4f) axis = XMVectorSet(0, 1, 0, 0);
    XMMATRIX tumble = XMMatrixRotationAxis(XMVector3Normalize(axis), m_angle);
    XMMATRIX world;
    switch (m_style) {
    case FlyingObjectsSettings::Splash:
        world = XMMatrixRotationY(t * 0.15f) * XMMatrixRotationX(ToRadians(-55.0f)) * XMMatrixTranslation(0, -0.3f, 0);
        break;
    case FlyingObjectsSettings::Flag:
        world = XMMatrixRotationY(0.5f * std::sin(t * 0.3f)) * XMMatrixRotationX(0.2f * std::sin(t * 0.21f));
        break;
    default:
        world = tumble;
        break;
    }

    if (m_style == FlyingObjectsSettings::Logo) {
        // Four panes in the emblem's colours, tumbling as a group.
        static const XMFLOAT4 pane[4] = { { 0.92f, 0.2f, 0.14f, 1 }, { 0.24f, 0.7f, 0.2f, 1 }, { 0.12f, 0.43f, 0.9f, 1 }, { 0.98f, 0.78f, 0.12f, 1 } };
        static const XMFLOAT2 at[4] = { { -0.5f, 0.5f }, { 0.5f, 0.5f }, { -0.5f, -0.5f }, { 0.5f, -0.5f } };
        for (int i = 0; i < 4; ++i) {
            Material pm = m_material;
            pm.color = m_settings.colorMode == FlyingObjectsSettings::Solid ? m_solid : pane[i];
            float wave = 0.25f * std::sin(t * 2.0f + i * 1.3f);
            XMMATRIX w = XMMatrixRotationY(wave) * XMMatrixTranslation(at[i].x * 1.05f, at[i].y * 1.05f, 0) * XMMatrixScaling(1.6f * m_size, 1.6f * m_size, 1.6f * m_size) * tumble;
            m_forward.Draw(ctx, m_box, w, pm);
        }
        return;
    }
    for (int i = 0; i < meshes; ++i)
        if (m_meshReady[i]) m_forward.Draw(ctx, m_mesh[i], world, m);
}

void FlyingObjectsSaver::Resize(int width, int height) {
    m_ctx.width = width;
    m_ctx.height = height;
    m_ctx.viewport = { 0, 0, width, height };
    SetupCamera();
}
