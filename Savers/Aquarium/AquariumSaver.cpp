#include "AquariumSaver.h"
#include "Gfx/Device.h"
#include "Gfx/Primitives.h"
#include "Gfx/SwapChain.h"
#include "Gfx/TextureFactory.h"
#include "Util/Color.h"
#include "Util/MathUtil.h"
#include "Shaders/Water_ps.h"
#include "Shaders/Caustics_ps.h"
#include "Shaders/Glass_ps.h"
#include "Shaders/Fish_vs.h"
#include "Shaders/Fish_ps.h"
#include <algorithm>
#include <cmath>

using namespace DirectX;
using namespace rs;

namespace {

constexpr float kFishScale = 1.2f;       // species sizes -> world units
constexpr float kSurfaceY = 9.2f;        // bubbles pop here
constexpr float kWallZ = 12.0f;          // back wall plane: the floor ends here and the gradient is measured on it
constexpr int kMaxFish = 60;
constexpr size_t kMaxBubbles = 400;

XMFLOAT3 Heading(float yaw, float pitch) {
    float cp = std::cos(pitch);
    return { std::cos(yaw) * cp, std::sin(pitch), std::sin(yaw) * cp };
}

float WrapAngle(float a) {
    while (a > kPi) a -= kTwoPi;
    while (a < -kPi) a += kTwoPi;
    return a;
}

float Approach(float from, float to, float rate, float dt) {
    return Lerp(from, to, 1.0f - std::exp(-rate * dt));
}

// Value-noise fbm that tiles at [0,1) in both axes: four offset samples blended by position.
float TileableFbm(float u, float v, float scale, uint32_t seed) {
    float x = u * scale, y = v * scale;
    float a = TextureFactory::Fbm(x, y, seed), b = TextureFactory::Fbm(x - scale, y, seed);
    float c = TextureFactory::Fbm(x, y - scale, seed), d = TextureFactory::Fbm(x - scale, y - scale, seed);
    return Lerp(Lerp(a, b, u), Lerp(c, d, u), v);
}

Image SandImage(int size, uint32_t seed) {
    Image img(size, size);
    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x) {
            float u = (x + 0.5f) / size, v = (y + 0.5f) / size;
            float n = TileableFbm(u, v, 6.0f, seed);
            float fine = TileableFbm(u, v, 40.0f, seed + 9);
            float k = 0.72f + 0.35f * n + 0.14f * (fine - 0.5f);
            img.At(x, y) = PackRgbaF(0.80f * k, 0.72f * k, 0.54f * k);
        }
    return img;
}

Image RockImage(int size, uint32_t seed) {
    Image img(size, size);
    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x) {
            float u = (x + 0.5f) / size, v = (y + 0.5f) / size;
            float n = TileableFbm(u, v, 5.0f, seed);
            float fine = TileableFbm(u, v, 24.0f, seed + 3);
            float k = (0.55f + 0.45f * n + 0.15f * (fine - 0.5f)) * (1.0f - 0.35f * v);   // darker towards the base
            img.At(x, y) = PackRgbaF(0.78f * k, 0.72f * k, 0.64f * k);
        }
    return img;
}

Image KelpImage(int w, int h) {
    Image img(w, h);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            float u = (x + 0.5f) / w, v = 1.0f - (y + 0.5f) / h;      // v = 0 at the base (bottom row)
            float vein = std::exp(-std::pow((u - 0.5f) / 0.14f, 2.0f));
            float edge = Smoothstep(0.0f, 0.12f, u) * Smoothstep(0.0f, 0.12f, 1.0f - u);
            XMFLOAT3 c{ Lerp(0.10f, 0.42f, v), Lerp(0.28f, 0.72f, v), Lerp(0.06f, 0.18f, v) };
            float k = (1.0f - 0.3f * vein) * (0.7f + 0.3f * edge);
            img.At(x, y) = PackRgbaF(c.x * k, c.y * k, c.z * k);
        }
    return img;
}

Image ShaftImage(int w, int h) {
    Image img(w, h, 0);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            float dx = (x + 0.5f) / w * 2.0f - 1.0f, v = (y + 0.5f) / h;
            float a = std::pow(std::max(1.0f - std::abs(dx), 0.0f), 2.2f) * std::pow(1.0f - v, 1.4f);
            img.At(x, y) = PackRgbaF(a, a, a, a);
        }
    return img;
}

} // namespace

// ---------------------------------------------------------------- settings

AquariumSettings AquariumSettings::Load(const Settings& s) {
    AquariumSettings v;
    v.fishCount = Clamp(s.GetInt(L"FishCount", v.fishCount), 5, 30);
    v.species = s.GetInt(L"Species", v.species) & ((1 << kSpeciesCount) - 1);
    v.plants = Clamp(s.GetInt(L"Plants", v.plants), 0, 10);
    v.rocks = Clamp(s.GetInt(L"Rocks", v.rocks), 0, 8);
    v.bubbles = s.GetBool(L"Bubbles", v.bubbles);
    v.shafts = s.GetBool(L"LightShafts", v.shafts);
    v.ornament = s.GetBool(L"Ornament", v.ornament);
    v.caustics = Clamp(s.GetInt(L"Caustics", v.caustics), 0, 10);
    v.waterTint = static_cast<COLORREF>(s.GetInt(L"WaterTint", static_cast<int>(v.waterTint))) & 0xFFFFFF;
    v.speed = Clamp(s.GetInt(L"Speed", v.speed), 1, 10);
    v.quality = Clamp(s.GetInt(L"Quality", v.quality), 0, 2);
    return v;
}

void AquariumSettings::Save(Settings& s) const {
    s.SetInt(L"FishCount", fishCount);
    s.SetInt(L"Species", species);
    s.SetInt(L"Plants", plants);
    s.SetInt(L"Rocks", rocks);
    s.SetBool(L"Bubbles", bubbles);
    s.SetBool(L"LightShafts", shafts);
    s.SetBool(L"Ornament", ornament);
    s.SetInt(L"Caustics", caustics);
    s.SetInt(L"WaterTint", static_cast<int>(waterTint));
    s.SetInt(L"Speed", speed);
    s.SetInt(L"Quality", quality);
}

// ---------------------------------------------------------------- lifecycle

void AquariumSaver::Initialize(Device& device, const SaverContext& ctx) {
    m_ctx = ctx;
    m_settings = AquariumSettings::Load(*ctx.settings);
    m_speedFactor = 0.4f + 0.12f * m_settings.speed;
    m_quality = ctx.previewMode ? 0 : m_settings.quality;
    Rng& rng = *ctx.rng;

    m_forward.Create(device);
    m_post.Create(device);
    m_billboard.Create(device);
    ID3D11Device* d = device.Get();
    ThrowIfFailed(d->CreatePixelShader(g_Water_ps, sizeof(g_Water_ps), nullptr, &m_waterPs), "CreatePixelShader(Water)");
    ThrowIfFailed(d->CreatePixelShader(g_Caustics_ps, sizeof(g_Caustics_ps), nullptr, &m_causticsPs), "CreatePixelShader(Caustics)");
    ThrowIfFailed(d->CreatePixelShader(g_Glass_ps, sizeof(g_Glass_ps), nullptr, &m_glassPs), "CreatePixelShader(Glass)");
    ThrowIfFailed(d->CreatePixelShader(g_Fish_ps, sizeof(g_Fish_ps), nullptr, &m_fishPs), "CreatePixelShader(Fish)");
    ThrowIfFailed(d->CreateVertexShader(g_Fish_vs, sizeof(g_Fish_vs), nullptr, &m_fishVs), "CreateVertexShader(Fish)");
    UINT n = 0;
    const auto* layout = LayoutPNT(n);
    ThrowIfFailed(d->CreateInputLayout(layout, n, g_Fish_vs, sizeof(g_Fish_vs), &m_fishLayout), "CreateInputLayout(Fish)");
    m_waterCb.Create(device);
    m_causticsCb.Create(device);
    m_glassCb.Create(device);
    m_fishCb.Create(device);
    const int causticsRes = m_quality == 0 ? 128 : m_quality == 1 ? 256 : 512;
    m_caustics.Create(device, causticsRes, causticsRes);

    // Water colours from the tint: a lighter top, a deep bottom that doubles as the fog colour.
    XMFLOAT4 tint = FromColorRef(m_settings.waterTint);
    m_tint = { tint.x, tint.y, tint.z };
    m_waterTop = { Saturate(tint.x * 1.5f + 0.06f), Saturate(tint.y * 1.5f + 0.06f), Saturate(tint.z * 1.5f + 0.06f) };
    m_waterBottom = { tint.x * 0.45f, tint.y * 0.45f, tint.z * 0.45f };

    const uint32_t seed = static_cast<uint32_t>(rng.Int(1, 1 << 30));
    m_sand.FromImage(device, SandImage(256, seed), true);
    m_rockTex.FromImage(device, RockImage(256, seed + 5), true);
    m_kelpTex.FromImage(device, KelpImage(16, 64), true);
    m_shaftTex.FromImage(device, ShaftImage(32, 256), true);
    m_bubbleTex.FromImage(device, TextureFactory::BubbleRing(64), true);
    m_box.Create(device, Primitives::Box(1, 1, 1));
    m_column.Create(device, Primitives::Cylinder(0.32f, 2.4f, 16));

    m_time = rng.Range(0.0f, 500.0f);
    m_simTime = static_cast<float>(m_time);
    SetupCamera();
    BuildEnvironment(device);
    SpawnFish(device);
}

void AquariumSaver::SetupCamera() {
    m_camera.fovY = ToRadians(38.0f);
    m_camera.aspect = static_cast<float>(m_ctx.width) / static_cast<float>(std::max(m_ctx.height, 1));
    m_camera.nearZ = 0.5f;
    m_camera.farZ = 80.0f;
    m_camera.target = { 0, 3.5f, 4.0f };
    m_camera.up = { 0, 1, 0 };
    // Very slow yaw drift around the tank centre.
    float yaw = ToRadians(3.0f) * std::sin(static_cast<float>(m_time) * 0.05f);
    m_camera.eye = { 12.0f * std::sin(yaw), 5.0f, 4.0f - 12.0f * std::cos(yaw) };
    // Fish may use the width visible at mid-depth (plus a little), clamped for extreme aspects.
    float halfW = 12.0f * std::tan(m_camera.fovY * 0.5f) * m_camera.aspect;
    m_halfX = Clamp(halfW * 1.05f, 5.0f, 14.0f);
}

float AquariumSaver::FloorHeight(float x, float z) const {
    float dunes = (TextureFactory::Fbm(x * 0.12f + 3.0f, z * 0.12f + 7.0f, 77, 3) - 0.5f) * 0.9f;
    float ripples = 0.05f * std::sin(x * 1.3f + z * 0.4f) + 0.03f * std::sin(x * 0.7f - z * 1.1f);
    return dunes + ripples;
}

void AquariumSaver::BuildEnvironment(Device& device) {
    Rng& rng = *m_ctx.rng;
    const int q = m_quality == 0 ? 2 : m_quality == 1 ? 4 : 6;

    // Floor: a wide grid pushed into dunes, running from just behind the camera to the back
    // wall plane, where fog has all but turned it into the wall colour.
    MeshData floor = Primitives::Grid(44.0f, kWallZ + 4.0f, 22 * q, 8 * q, 12.0f);
    for (auto& v : floor.vertices) {
        v.position.z += (kWallZ - 4.0f) * 0.5f;
        v.position.y = FloorHeight(v.position.x, v.position.z);
    }
    floor.ComputeNormals();
    m_floor.Create(device, floor);

    // Ornament and vents claim their spots first so rocks avoid them.
    m_ornamentPos = { rng.Range(-m_halfX * 0.6f, m_halfX * 0.6f), 0.0f, rng.Range(4.0f, 7.0f) };
    m_ornamentPos.y = FloorHeight(m_ornamentPos.x, m_ornamentPos.z) - 0.1f;
    m_ornamentYaw = rng.Range(-0.6f, 0.6f);
    int ventCount = rng.Int(2, 3);
    for (int i = 0; i < ventCount; ++i) {
        Vent v;
        v.pos = { rng.Range(-m_halfX * 0.85f, m_halfX * 0.85f), 0.0f, rng.Range(3.0f, 7.5f) };
        v.pos.y = FloorHeight(v.pos.x, v.pos.z);
        v.timer = rng.Range(0.0f, 1.0f);
        m_vents.push_back(v);
    }

    auto clearOf = [&](float x, float z, float keep) {
        if (m_settings.ornament && std::hypot(x - m_ornamentPos.x, z - m_ornamentPos.z) < keep + 1.3f) return false;
        for (const Vent& v : m_vents) if (std::hypot(x - v.pos.x, z - v.pos.z) < keep + 0.3f) return false;
        for (const Rock& r : m_rocks) if (std::hypot(x - r.pos.x, z - r.pos.z) < keep + r.radius + 0.3f) return false;
        return true;
    };

    // Rocks: unique lumpy meshes, placed by rejection sampling.
    for (int i = 0; i < m_settings.rocks; ++i) {
        float radius = rng.Range(0.5f, 1.4f);
        XMFLOAT3 pos{};
        bool placed = false;
        for (int attempt = 0; attempt < 40 && !placed; ++attempt) {
            pos = { rng.Range(-m_halfX * 0.95f, m_halfX * 0.95f), 0.0f, rng.Range(2.6f, 8.2f) };
            placed = clearOf(pos.x, pos.z, radius);
        }
        if (!placed) continue;
        pos.y = FloorHeight(pos.x, pos.z) - 0.08f;
        Rock r;
        r.pos = pos;
        r.radius = radius;
        float mix = rng.Float(), k = rng.Range(0.8f, 1.15f);
        r.color = { Lerp(0.85f, 0.80f, mix) * k, Lerp(0.82f, 0.64f, mix) * k, Lerp(0.78f, 0.48f, mix) * k, 1.0f };
        r.mesh = static_cast<int>(m_rockMeshes.size());
        m_rockMeshes.emplace_back();
        m_rockMeshes.back().Create(device, Primitives::Rock(radius, static_cast<uint32_t>(rng.Int(1, 100000)), rng.Range(0.25f, 0.45f), rng.Range(0.6f, 0.85f), 12 + 6 * q / 2, 8 + 4 * q / 2));
        m_rocks.push_back(r);
    }

    // Kelp: one dynamic mesh for every stalk, rebuilt each frame in UpdateKelp.
    const int segments = m_quality == 0 ? 8 : m_quality == 1 ? 11 : 14;
    for (int i = 0; i < m_settings.plants; ++i) {
        Kelp k;
        float x = 0, z = 0;
        for (int attempt = 0; attempt < 30; ++attempt) {
            x = rng.Range(-m_halfX * 0.95f, m_halfX * 0.95f);
            z = rng.Range(2.8f, 8.6f);
            if (clearOf(x, z, 0.2f)) break;
        }
        k.base = { x, FloorHeight(x, z) - 0.05f, z };
        k.height = rng.Range(2.2f, 5.2f);
        k.width = rng.Range(0.28f, 0.46f);
        k.phase = rng.Range(0.0f, kTwoPi);
        k.freq = rng.Range(0.5f, 0.9f);
        k.sway = rng.Range(0.22f, 0.42f);
        k.lean = rng.Range(-0.18f, 0.18f);
        k.segments = segments;
        m_kelp.push_back(k);
    }
    if (!m_kelp.empty()) {
        m_kelpData.vertices.assign(m_kelp.size() * (segments + 1) * 2, VertexPNT{});
        for (size_t s = 0; s < m_kelp.size(); ++s) {
            uint32_t base = static_cast<uint32_t>(s * (segments + 1) * 2);
            for (int i = 0; i < segments; ++i) {
                uint32_t a = base + i * 2, b = a + 1, c = a + 2, d = a + 3;
                m_kelpData.indices.insert(m_kelpData.indices.end(), { a, c, b, b, c, d });
            }
        }
    }

    // Grass tufts: bunches of thin static blades scattered over the floor.
    MeshData tufts;
    const int tuftCount = 18 + 3 * m_settings.plants;
    for (int t = 0; t < tuftCount; ++t) {
        float cx = rng.Range(-m_halfX * 1.1f, m_halfX * 1.1f), cz = rng.Range(2.4f, 9.5f);
        if (!clearOf(cx, cz, 0.0f)) continue;
        int blades = rng.Int(5, 9);
        for (int b = 0; b < blades; ++b) {
            float x = cx + rng.Range(-0.25f, 0.25f), z = cz + rng.Range(-0.25f, 0.25f);
            float y = FloorHeight(x, z) - 0.03f;
            float h = rng.Range(0.3f, 0.85f), w = rng.Range(0.04f, 0.08f);
            float ang = rng.Range(0.0f, kTwoPi), lean = rng.Range(0.05f, 0.3f);
            float sx = std::cos(ang) * w, sz = std::sin(ang) * w;
            uint32_t i0 = static_cast<uint32_t>(tufts.vertices.size());
            tufts.vertices.push_back({ { x - sx, y, z - sz }, { 0, 0, -1 }, { 0.2f, 0.0f } });
            tufts.vertices.push_back({ { x + sx, y, z + sz }, { 0, 0, -1 }, { 0.8f, 0.0f } });
            tufts.vertices.push_back({ { x + std::sin(ang) * lean, y + h, z - std::cos(ang) * lean }, { 0, 0, -1 }, { 0.5f, 1.0f } });
            tufts.indices.insert(tufts.indices.end(), { i0, i0 + 1, i0 + 2 });
        }
    }
    if (!tufts.indices.empty()) {
        tufts.ComputeNormals();
        m_tufts.Create(device, tufts);
    }

    // Light shafts leaning through the water.
    int shaftCount = rng.Int(6, 10);
    for (int i = 0; i < shaftCount; ++i) {
        Shaft s;
        s.x = rng.Range(-m_halfX * 1.1f, m_halfX * 1.1f);
        s.z = rng.Range(2.5f, 7.5f);
        s.width = rng.Range(0.6f, 1.7f);
        s.tilt = ToRadians(15.0f + rng.Range(-6.0f, 6.0f));
        s.phase = rng.Range(0.0f, kTwoPi);
        m_shafts.push_back(s);
    }
}

void AquariumSaver::SpawnFish(Device& device) {
    Rng& rng = *m_ctx.rng;
    std::vector<int> enabled;
    for (int i = 0; i < kSpeciesCount; ++i)
        if (m_settings.species & (1 << i)) enabled.push_back(i);
    if (enabled.empty())
        for (int i = 0; i < kSpeciesCount; ++i) enabled.push_back(i);

    int placed = 0, plecos = 0, schools = 0;
    int guard = 0;
    while (placed < m_settings.fishCount && static_cast<int>(m_fish.size()) < kMaxFish && ++guard < 500) {
        int si = enabled[static_cast<size_t>(rng.Int(0, static_cast<int>(enabled.size()) - 1))];
        const FishSpecies& sp = GetSpecies(si);
        if (sp.bottomDweller && plecos >= 2 && enabled.size() > 1) continue;
        if (sp.bottomDweller) ++plecos;
        if (!m_fishMeshReady[si]) {
            m_fishMesh[si].Create(device, BuildFishMesh(sp, m_quality));
            m_fishMeshReady[si] = true;
        }
        int count = sp.schooling ? rng.Int(sp.minSchool, sp.maxSchool) : 1;
        XMFLOAT3 centre{ rng.Range(-m_halfX * 0.8f, m_halfX * 0.8f), rng.Range(m_yMin + 1.0f, m_yMax - 1.0f), rng.Range(m_zMin + 0.5f, m_zMax - 0.5f) };
        if (sp.bottomDweller) centre.y = 0.5f;
        float yaw = rng.Range(0.0f, kTwoPi);
        float seed = rng.Range(0.0f, 100.0f);
        int school = sp.schooling ? schools++ : -1;
        for (int k = 0; k < count; ++k) {
            Fish f;
            f.species = si;
            f.school = school;
            f.size = sp.size * kFishScale * rng.Range(0.85f, 1.15f);
            f.speedScale = rng.Range(0.85f, 1.15f);
            float j = sp.schooling ? 0.9f : 0.0f;
            f.pos = { centre.x + rng.Range(-j, j), centre.y + rng.Range(-j * 0.6f, j * 0.6f), centre.z + rng.Range(-j, j) };
            f.yaw = yaw + rng.Range(-0.3f, 0.3f);
            XMFLOAT3 h = Heading(f.yaw, 0.0f);
            float sp0 = sp.cruise * f.size;
            f.vel = { h.x * sp0, h.y * sp0, h.z * sp0 };
            f.phase = rng.Range(0.0f, kTwoPi);
            f.wanderSeed = sp.schooling ? seed : rng.Range(0.0f, 100.0f);
            f.dartTimer = rng.Range(5.0f, 30.0f);
            f.nibbleTimer = rng.Range(10.0f, 60.0f);
            f.bubbleTimer = rng.Range(10.0f, 60.0f);
            m_fish.push_back(f);
            ++placed;
        }
    }
}

// ---------------------------------------------------------------- simulation

XMFLOAT3 AquariumSaver::PlantTip(int plant) const {
    if (plant < 0 || plant >= static_cast<int>(m_kelp.size())) return { 0, 2, 5 };
    const Kelp& k = m_kelp[static_cast<size_t>(plant)];
    return { k.base.x, k.base.y + k.height * 0.8f, k.base.z };
}

void AquariumSaver::Update(float dt, double) {
    m_time += dt;
    float sdt = dt * m_speedFactor;
    m_simTime += sdt;
    SetupCamera();
    UpdateFish(sdt);
    UpdateBubbles(sdt);
}

void AquariumSaver::UpdateFish(float dt) {
    if (dt <= 0.0f) return;
    Rng& rng = *m_ctx.rng;
    const size_t n = m_fish.size();
    const float t = m_simTime;
    std::vector<XMFLOAT3> steer(n, XMFLOAT3{ 0, 0, 0 });

    // Pairwise: everybody keeps a little distance; schools also align and gather.
    for (size_t i = 0; i < n; ++i) {
        const Fish& a = m_fish[i];
        for (size_t j = i + 1; j < n; ++j) {
            const Fish& b = m_fish[j];
            float dx = a.pos.x - b.pos.x, dy = a.pos.y - b.pos.y, dz = a.pos.z - b.pos.z;
            float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
            float sep = 0.8f * (a.size + b.size) * 0.5f;
            if (dist < sep && dist > 1e-3f) {
                float k = (sep - dist) / sep * 3.0f / dist;
                steer[i].x += dx * k; steer[i].y += dy * k; steer[i].z += dz * k;
                steer[j].x -= dx * k; steer[j].y -= dy * k; steer[j].z -= dz * k;
            }
            if (a.school >= 0 && a.school == b.school) {
                float R = 3.0f * a.size;
                if (dist < R) {
                    float c = 0.35f;
                    steer[i].x -= dx * c; steer[i].y -= dy * c; steer[i].z -= dz * c;
                    steer[j].x += dx * c; steer[j].y += dy * c; steer[j].z += dz * c;
                    XMFLOAT3 ha = Heading(a.yaw, a.pitch), hb = Heading(b.yaw, b.pitch);
                    float al = 0.8f;
                    steer[i].x += (hb.x - ha.x) * al; steer[i].y += (hb.y - ha.y) * al; steer[i].z += (hb.z - ha.z) * al;
                    steer[j].x += (ha.x - hb.x) * al; steer[j].y += (ha.y - hb.y) * al; steer[j].z += (ha.z - hb.z) * al;
                }
            }
        }
    }

    auto wall = [](float d, float margin) { float k = d < margin ? (margin - d) / margin : 0.0f; return k * k * 4.0f; };

    for (size_t i = 0; i < n; ++i) {
        Fish& f = m_fish[i];
        const FishSpecies& sp = GetSpecies(f.species);
        XMFLOAT3 st = steer[i];
        XMFLOAT3 h = Heading(f.yaw, f.pitch);
        const XMFLOAT3& p = f.pos;

        // Wander: smooth pseudo-random pull shared by a school.
        float ws = f.wanderSeed, tt = t * 0.45f + ws;
        st.x += 0.9f * (0.6f * std::sin(tt * 0.9f + ws * 3.0f) + 0.4f * std::sin(tt * 1.7f + ws));
        st.y += 0.9f * (0.3f * std::sin(tt * 0.6f + ws * 5.0f) + 0.2f * std::sin(tt * 1.1f));
        st.z += 0.9f * (0.6f * std::cos(tt * 0.8f + ws * 2.0f) + 0.4f * std::sin(tt * 1.3f + ws * 7.0f));

        // Soft walls, strongest at the front glass.
        float yMin = sp.bottomDweller ? 0.2f : m_yMin;
        st.x += wall(p.x + m_halfX, 2.5f) - wall(m_halfX - p.x, 2.5f);
        st.y += wall(p.y - yMin, 1.2f) - wall(m_yMax - p.y, 1.2f);
        st.z += wall(p.z - m_zMin, 1.8f) - wall(m_zMax - p.z, 1.5f);

        // Rocks and the ornament are cylinders to swim around (or over).
        auto avoid = [&](float cx, float cz, float radius, float top) {
            if (p.y > top + 0.5f * f.size) return;
            float dx = p.x - cx, dz = p.z - cz;
            float dist = std::sqrt(dx * dx + dz * dz);
            float keep = radius + 0.9f * f.size;
            if (dist < keep && dist > 1e-3f) {
                float k = (keep - dist) / keep;
                st.x += dx / dist * k * 5.0f;
                st.z += dz / dist * k * 5.0f;
                st.y += k * 1.5f;
            }
        };
        for (const Rock& r : m_rocks) avoid(r.pos.x, r.pos.z, r.radius, r.pos.y + r.radius * 0.8f);
        if (m_settings.ornament) avoid(m_ornamentPos.x, m_ornamentPos.z, 1.1f, m_ornamentPos.y + 1.2f);

        // Stay above the dunes; bottom dwellers hug them.
        float floorY = FloorHeight(p.x, p.z);
        if (sp.bottomDweller) {
            st.y += (floorY + 0.22f * f.size - p.y) * 2.5f;
        } else {
            float minY = floorY + 0.6f * f.size;
            if (p.y < minY) st.y += (minY - p.y) * 4.0f;
        }

        // Darts: a short burst of speed every so often.
        if (f.dartLeft > 0.0f) {
            f.dartLeft -= dt;
            st.x += h.x * 2.0f; st.y += h.y * 2.0f; st.z += h.z * 2.0f;
        } else {
            f.dartTimer -= dt;
            if (f.dartTimer < 0.0f && f.nibbleTarget < 0 && f.nibbleHold <= 0.0f) {
                f.dartLeft = rng.Range(0.5f, 1.0f);
                f.dartTimer = rng.Range(10.0f, 30.0f);
            }
        }

        float targetSpeed = sp.cruise * f.size * f.speedScale * (f.dartLeft > 0.0f ? 2.6f : 1.0f);
        bool hovering = false;

        if (sp.bottomDweller) {
            // Plecos rest on the sand between slow patrols.
            if (f.nibbleHold > 0.0f) {
                f.nibbleHold -= dt;
                targetSpeed = 0.0f;
            } else {
                f.nibbleTimer -= dt;
                if (f.nibbleTimer < 0.0f) { f.nibbleHold = rng.Range(5.0f, 15.0f); f.nibbleTimer = rng.Range(15.0f, 40.0f); }
            }
        } else if (!sp.schooling && !m_kelp.empty()) {
            // Nibbling: swim to a plant tip, hover with little head bobs, move on.
            if (f.nibbleTarget >= 0) {
                XMFLOAT3 tip = PlantTip(f.nibbleTarget);
                float dx = tip.x - p.x, dy = tip.y - p.y, dz = tip.z - p.z;
                float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
                if (f.nibbleHold > 0.0f) {
                    f.nibbleHold -= dt;
                    hovering = true;
                    targetSpeed = 0.05f;
                    st.x += dx * 0.5f; st.y += dy * 0.5f; st.z += dz * 0.5f;
                    if (f.nibbleHold <= 0.0f) { f.nibbleTarget = -1; f.nibbleTimer = rng.Range(25.0f, 70.0f); }
                } else if (dist < 0.7f * f.size) {
                    f.nibbleHold = rng.Range(3.0f, 6.0f);
                } else {
                    float want = sp.cruise * f.size;
                    st.x += (dx / dist * want - f.vel.x) * 1.5f;
                    st.y += (dy / dist * want - f.vel.y) * 1.5f;
                    st.z += (dz / dist * want - f.vel.z) * 1.5f;
                    targetSpeed *= 0.7f;
                    f.nibbleTimer -= dt;
                    if (f.nibbleTimer < -25.0f) { f.nibbleTarget = -1; f.nibbleTimer = rng.Range(20.0f, 50.0f); }   // gave up
                }
            } else {
                f.nibbleTimer -= dt;
                if (f.nibbleTimer < 0.0f) { f.nibbleTarget = rng.Int(0, static_cast<int>(m_kelp.size()) - 1); f.nibbleTimer = 0.0f; }
            }
        }

        // Integrate the steering into the velocity state, then relax its magnitude to the target speed.
        XMVECTOR v = XMLoadFloat3(&f.vel);
        v = XMVectorAdd(v, XMVectorScale(XMLoadFloat3(&st), dt * 2.5f));
        float speed = XMVectorGetX(XMVector3Length(v));
        if (speed < 1e-3f) { v = XMVectorSet(h.x, h.y, h.z, 0) * 0.1f; speed = 0.1f; }
        XMVECTOR dir = XMVectorScale(v, 1.0f / speed);
        float newSpeed = Approach(speed, targetSpeed, 1.2f, dt);
        XMStoreFloat3(&f.vel, XMVectorScale(dir, newSpeed));
        XMFLOAT3 d;
        XMStoreFloat3(&d, dir);

        // The heading chases the velocity with a turn-rate limit; roll and body bend follow the turn.
        float desiredYaw = std::atan2(d.z, d.x);
        float diff = WrapAngle(desiredYaw - f.yaw);
        float maxTurn = (1.6f + (f.dartLeft > 0.0f ? 1.5f : 0.0f)) * dt;
        float step = Clamp(diff, -maxTurn, maxTurn);
        f.yaw = WrapAngle(f.yaw + step);
        f.yawRate = Approach(f.yawRate, step / dt, 4.0f, dt);
        float desiredPitch = Clamp(std::asin(Clamp(d.y, -1.0f, 1.0f)), -0.45f, 0.45f);
        if (sp.bottomDweller) desiredPitch *= 0.3f;
        f.pitch = Approach(f.pitch, desiredPitch, 2.5f, dt);
        f.roll = Approach(f.roll, Clamp(f.yawRate * 0.35f, -0.5f, 0.5f), 3.0f, dt);
        f.bend = Approach(f.bend, Clamp(-f.yawRate * 0.12f, -0.15f, 0.15f), 4.0f, dt);

        // Move along the heading, never sideways.
        h = Heading(f.yaw, f.pitch);
        f.pos.x = Clamp(p.x + h.x * newSpeed * dt, -m_halfX - 0.5f, m_halfX + 0.5f);
        f.pos.y = Clamp(p.y + h.y * newSpeed * dt, yMin - 0.2f, m_yMax + 0.3f);
        f.pos.z = Clamp(p.z + h.z * newSpeed * dt, m_zMin - 0.3f, m_zMax + 0.3f);
        f.phase += dt * (newSpeed / f.size * 4.5f + (hovering ? 2.5f : 1.2f));

        // The odd bubble trail from the mouth.
        f.bubbleTimer -= dt;
        if (f.bubbleTimer < 0.0f) {
            if (m_settings.bubbles) {
                int count = rng.Int(3, 5);
                for (int k = 0; k < count; ++k)
                    EmitBubble({ f.pos.x + h.x * 0.5f * f.size + rng.Range(-0.05f, 0.05f), f.pos.y + h.y * 0.5f * f.size + k * 0.06f, f.pos.z + h.z * 0.5f * f.size },
                               rng.Range(0.03f, 0.06f) * (0.5f + f.size));
            }
            f.bubbleTimer = rng.Range(20.0f, 60.0f);
        }
    }
}

void AquariumSaver::EmitBubble(const XMFLOAT3& at, float radius) {
    if (m_bubbles.size() >= kMaxBubbles) return;
    Rng& rng = *m_ctx.rng;
    Bubble b;
    b.pos = at;
    b.radius = radius;
    b.rise = 0.9f + radius * 8.0f;
    b.phase = rng.Range(0.0f, kTwoPi);
    b.age = 0.0f;
    m_bubbles.push_back(b);
}

void AquariumSaver::UpdateBubbles(float dt) {
    Rng& rng = *m_ctx.rng;
    if (m_settings.bubbles) {
        for (Vent& v : m_vents) {
            v.timer -= dt;
            if (v.timer < 0.0f) {
                EmitBubble({ v.pos.x + rng.Range(-0.08f, 0.08f), v.pos.y + 0.05f, v.pos.z + rng.Range(-0.08f, 0.08f) }, rng.Range(0.05f, 0.14f));
                v.timer = rng.Range(0.25f, 0.9f);
            }
        }
    }
    for (Bubble& b : m_bubbles) {
        b.age += dt;
        b.pos.y += b.rise * dt;
        b.pos.x += std::sin(b.age * 4.0f + b.phase) * 0.3f * dt;
        b.pos.z += std::cos(b.age * 3.1f + b.phase) * 0.15f * dt;
        b.radius *= 1.0f + 0.04f * dt;
    }
    m_bubbles.erase(std::remove_if(m_bubbles.begin(), m_bubbles.end(), [](const Bubble& b) { return b.pos.y > kSurfaceY; }), m_bubbles.end());
}

void AquariumSaver::UpdateKelp(Device& device) {
    if (m_kelp.empty()) return;
    const float t = m_simTime;
    size_t vi = 0;
    for (const Kelp& k : m_kelp) {
        XMFLOAT3 pos = k.base;
        float segLen = k.height / k.segments;
        float halfW = k.width * 0.5f;
        // Sheets face the viewer, each turned a little so they don't all line up.
        float turn = k.lean * 2.0f;
        XMFLOAT3 side{ std::cos(turn), 0.0f, std::sin(turn) };
        for (int i = 0; i <= k.segments; ++i) {
            float s = static_cast<float>(i) / k.segments;
            float w = halfW * (1.0f - 0.6f * s) * (s < 0.1f ? 0.7f + 3.0f * s : 1.0f);
            VertexPNT& a = m_kelpData.vertices[vi++];
            VertexPNT& b = m_kelpData.vertices[vi++];
            a.position = { pos.x - side.x * w, pos.y, pos.z - side.z * w };
            b.position = { pos.x + side.x * w, pos.y, pos.z + side.z * w };
            a.uv = { 0.0f, s };
            b.uv = { 1.0f, s };
            // Sway accumulates up the stalk: a bend about Z (sideways) and a smaller one about X.
            float angle = k.sway * std::sin(t * k.freq + k.phase + s * 2.2f) * (0.15f + s) + k.lean * s;
            float angle2 = 0.4f * k.sway * std::sin(t * k.freq * 0.7f + k.phase * 1.3f + s * 1.7f) * s;
            XMFLOAT3 dir{ std::sin(angle), std::cos(angle) * std::cos(angle2), std::sin(angle2) };
            pos = { pos.x + dir.x * segLen, pos.y + dir.y * segLen, pos.z + dir.z * segLen };
        }
    }
    m_kelpData.ComputeNormals();
    if (!m_kelpReady) { m_kelpMesh.CreateDynamic(device, m_kelpData); m_kelpReady = true; }
    else m_kelpMesh.Update(device, m_kelpData);
}

// ---------------------------------------------------------------- render

void AquariumSaver::Render(Device& device, SwapChain& swap) {
    ID3D11DeviceContext* ctx = device.Ctx();
    const States& states = m_forward.GetStates();
    const float t = static_cast<float>(m_time);
    const float causticStrength = 0.28f * m_settings.caustics;

    // 1. Caustics into their tile.
    if (causticStrength > 0.0f) {
        CausticsCB c{ { m_simTime, 1.0f, 0, 0 } };
        m_causticsCb.Update(ctx, c);
        m_causticsCb.BindPS(ctx, 0);
        states.Set2D(ctx, states.Opaque());
        m_caustics.Bind(ctx);
        m_post.Draw(ctx, m_causticsPs.Get());
    }

    // 2. Water background straight into this viewport.
    swap.Bind();
    swap.SetViewport(m_ctx.viewport);
    WaterCB w{};
    w.top = { m_waterTop.x, m_waterTop.y, m_waterTop.z, 1 };
    w.bottom = { m_waterBottom.x, m_waterBottom.y, m_waterBottom.z, 1 };
    w.params = { m_simTime, m_camera.aspect, 0.35f, 1.0f };
    w.eye = { m_camera.eye.x, m_camera.eye.y, m_camera.eye.z, kWallZ };
    float tanHalf = std::tan(m_camera.fovY * 0.5f);
    XMStoreFloat4(&w.forward, m_camera.Forward());
    XMStoreFloat4(&w.right, XMVectorScale(m_camera.Right(), tanHalf * m_camera.aspect));
    XMStoreFloat4(&w.up, XMVectorScale(m_camera.TrueUp(), tanHalf));
    w.heights = { 0.3f, 9.5f, 0, 0 };
    m_waterCb.Update(ctx, w);
    m_waterCb.BindPS(ctx, 0);
    states.Set2D(ctx, states.Opaque());
    m_post.Draw(ctx, m_waterPs.Get());

    // 3. Opaque scene with depth fog into the water colour.
    FrameConstants fc{};
    XMStoreFloat4x4(&fc.viewProj, XMMatrixTranspose(m_camera.ViewProj()));
    fc.eyePos = { m_camera.eye.x, m_camera.eye.y, m_camera.eye.z, 1 };
    XMStoreFloat4(&fc.lightDir, XMVector3Normalize(XMVectorSet(0.15f, 1.0f, -0.35f, 0)));
    fc.lightColor = { 0.95f, 1.0f, 0.95f, 1 };
    fc.ambient = { Lerp(m_tint.x, 0.4f, 0.5f) * 0.9f, Lerp(m_tint.y, 0.4f, 0.5f) * 0.9f, Lerp(m_tint.z, 0.4f, 0.5f) * 0.9f, 1 };
    fc.fogColor = { m_waterBottom.x, m_waterBottom.y, m_waterBottom.z, 1 };
    fc.fogParams = { 9.0f, 21.0f, 0, 0 };
    m_forward.BeginFrame(ctx, fc);
    states.SetOpaque3D(ctx);

    ID3D11ShaderResourceView* lightMap = causticStrength > 0.0f ? m_caustics.SRV() : nullptr;
    Material sand;
    sand.texture = &m_sand;
    sand.specPower = 8.0f;
    sand.specIntensity = 0.05f;
    sand.lightMap = lightMap;
    sand.lightMapScale = 1.0f / 7.0f;
    sand.lightMapStrength = causticStrength;
    m_forward.Draw(ctx, m_floor, XMMatrixIdentity(), sand);

    Material rock = sand;
    rock.texture = &m_rockTex;
    rock.specPower = 16.0f;
    rock.specIntensity = 0.15f;
    rock.lightMapStrength = causticStrength * 0.8f;
    for (const Rock& r : m_rocks) {
        rock.color = r.color;
        m_forward.Draw(ctx, m_rockMeshes[static_cast<size_t>(r.mesh)], XMMatrixTranslation(r.pos.x, r.pos.y, r.pos.z), rock);
    }

    if (m_settings.ornament) {
        // A treasure chest (base, open lid, gold) and a fallen column beside it.
        XMMATRIX place = XMMatrixRotationY(m_ornamentYaw) * XMMatrixTranslation(m_ornamentPos.x, m_ornamentPos.y, m_ornamentPos.z);
        Material wood = sand;
        wood.texture = nullptr;
        wood.color = { 0.42f, 0.26f, 0.12f, 1 };
        wood.specPower = 12.0f;
        wood.specIntensity = 0.1f;
        wood.lightMapStrength = causticStrength * 0.8f;
        m_forward.Draw(ctx, m_box, XMMatrixScaling(1.3f, 0.7f, 0.8f) * XMMatrixTranslation(0, 0.35f, 0) * place, wood);
        m_forward.Draw(ctx, m_box, XMMatrixScaling(1.3f, 0.22f, 0.8f) * XMMatrixTranslation(0, 0.11f, -0.4f) * XMMatrixRotationX(ToRadians(60.0f)) * XMMatrixTranslation(0, 0.7f, 0.4f) * place, wood);
        Material gold = wood;
        gold.color = { 1.0f, 0.78f, 0.2f, 1 };
        gold.specPower = 48.0f;
        gold.specIntensity = 0.9f;
        m_forward.Draw(ctx, m_box, XMMatrixScaling(1.1f, 0.35f, 0.6f) * XMMatrixTranslation(0, 0.75f, 0) * place, gold);
        Material stone = rock;
        stone.color = { 0.72f, 0.70f, 0.62f, 1 };
        m_forward.Draw(ctx, m_column, XMMatrixTranslation(0, 0, -1.2f) * XMMatrixRotationY(XM_PIDIV2) * XMMatrixRotationZ(0.14f) * XMMatrixTranslation(2.2f, 0.2f, 0.3f) * place, stone);
    }

    // Plants are single sheets: two-sided.
    ctx->RSSetState(states.CullNone());
    Material plant = sand;
    plant.texture = &m_kelpTex;
    plant.sampler = states.LinearClamp();
    plant.specPower = 24.0f;
    plant.specIntensity = 0.25f;
    plant.lightMapStrength = causticStrength * 0.6f;
    if (m_tufts.Valid()) {
        plant.color = { 0.85f, 1.0f, 0.75f, 1 };
        m_forward.Draw(ctx, m_tufts, XMMatrixIdentity(), plant);
    }
    UpdateKelp(device);
    if (m_kelpReady) {
        plant.color = { 1, 1, 1, 1 };
        m_forward.Draw(ctx, m_kelpMesh, XMMatrixIdentity(), plant);
    }

    // 4. Fish: custom VS (swim) and PS (pattern) sharing the Phong constants and the caustics.
    Material fishMat;
    fishMat.lightMap = lightMap;
    fishMat.lightMapScale = 1.0f / 7.0f;
    fishMat.lightMapStrength = causticStrength * 0.7f;
    for (const Fish& f : m_fish) {
        const FishSpecies& sp = GetSpecies(f.species);
        bool bob = f.nibbleHold > 0.0f && !sp.bottomDweller;
        float pitch = f.pitch + (bob ? 0.1f * std::sin(m_simTime * 6.0f + f.wanderSeed) : 0.0f);
        XMMATRIX world = XMMatrixScaling(f.size, f.size, f.size) * XMMatrixRotationX(f.roll) * XMMatrixRotationZ(pitch) *
                         XMMatrixRotationY(-f.yaw) * XMMatrixTranslation(f.pos.x, f.pos.y, f.pos.z);
        float speed = std::sqrt(f.vel.x * f.vel.x + f.vel.y * f.vel.y + f.vel.z * f.vel.z) / f.size;
        FishCB cb{};
        cb.anim = { f.phase, sp.swimAmp * (0.6f + 0.5f * std::min(speed / sp.cruise, 2.0f)), sp.swimFreq, 0 };
        cb.anim2 = { f.bend, sp.swimAmp * 0.8f, 0.12f, 0 };
        cb.base = { sp.base.x, sp.base.y, sp.base.z, 1 };
        cb.accent = { sp.accent.x, sp.accent.y, sp.accent.z, 1 };
        cb.fin = { sp.fin.x, sp.fin.y, sp.fin.z, 1 };
        cb.tail = { sp.tail.x, sp.tail.y, sp.tail.z, 1 };
        cb.pattern = { static_cast<float>(sp.patternMode), sp.p1, sp.p2, sp.belly };
        cb.pattern2 = { sp.sheen, sp.tailGradient ? 1.0f : 0.0f, sp.specPower, sp.specIntensity };
        m_fishCb.Update(ctx, cb);
        m_fishCb.BindVS(ctx, 2);
        m_fishCb.BindPS(ctx, 2);
        m_forward.Draw(ctx, m_fishMesh[f.species], world, fishMat, m_fishVs.Get(), m_fishLayout.Get(), m_fishPs.Get());
    }

    // 5. Translucent sprites: light shafts (additive) and bubbles, depth-tested against the scene.
    m_billboard.BeginFrame(ctx, m_camera);
    if (m_settings.shafts) {
        for (const Shaft& s : m_shafts) {
            float k = 0.16f + 0.10f * std::sin(m_simTime * 0.3f + s.phase);
            XMFLOAT4 color{ (m_waterTop.x * 0.7f + 0.3f) * k, (m_waterTop.y * 0.7f + 0.3f) * k, (m_waterTop.z * 0.7f + 0.3f) * k, 1 };
            XMFLOAT3 right{ std::cos(s.tilt) * s.width * 0.5f, -std::sin(s.tilt) * s.width * 0.5f, 0 };
            XMFLOAT3 up{ std::sin(s.tilt) * 5.5f, std::cos(s.tilt) * 5.5f, 0 };
            m_billboard.DrawOriented(ctx, m_shaftTex, { s.x, 5.0f, s.z }, right, up, color, false, states.Additive());
        }
    }
    for (const Bubble& b : m_bubbles)
        m_billboard.Draw(ctx, m_bubbleTex, b.pos, b.radius * 2.0f, b.radius * 2.0f, { 0.85f, 0.95f, 1.0f, 0.85f }, false);

    // 6. Through the glass: vignette and a faint reflection.
    GlassCB g{ { t, m_camera.aspect, 0.45f, 0.10f } };
    m_glassCb.Update(ctx, g);
    m_glassCb.BindPS(ctx, 0);
    states.Set2D(ctx, states.AlphaBlend());
    m_post.Draw(ctx, m_glassPs.Get());

    // The caustics texture becomes a render target again next frame.
    ID3D11ShaderResourceView* null = nullptr;
    ctx->PSSetShaderResources(1, 1, &null);
}

void AquariumSaver::Resize(int width, int height) {
    m_ctx.width = width;
    m_ctx.height = height;
    m_ctx.viewport = { 0, 0, width, height };
    SetupCamera();
}
