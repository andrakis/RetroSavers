#include "CelestialsSaver.h"
#include "Gfx/Device.h"
#include "Gfx/SwapChain.h"
#include "Gfx/TextureFactory.h"
#include "Util/MathUtil.h"
#include "Shaders/BlackHole_ps.h"
#include "Shaders/Boson_ps.h"
#include "Shaders/WhiteHole_ps.h"
#include "Shaders/Tzo_ps.h"
#include "Shaders/Strange_ps.h"
#include "Shaders/Ember_ps.h"
#include "Shaders/Star_ps.h"
#include "Shaders/BrightPass_ps.h"
#include "Shaders/Composite_ps.h"
#include <algorithm>
#include <cmath>

using namespace DirectX;
using namespace rs;

namespace {

using V3 = CelestialsSaver::V3;

V3 operator+(V3 a, V3 b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
V3 operator-(V3 a, V3 b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
V3 operator*(V3 a, float s) { return { a.x * s, a.y * s, a.z * s }; }
float Dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
V3 Cross(V3 a, V3 b) { return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x }; }
float Len(V3 a) { return std::sqrt(Dot(a, a)); }
V3 Norm(V3 a) { float l = Len(a); return l > 1e-6f ? a * (1.0f / l) : V3{ 0, 1, 0 }; }
V3 RotateAxis(V3 p, V3 k, float a) {
    float c = std::cos(a), s = std::sin(a);
    return p * c + Cross(k, p) * s + k * (Dot(k, p) * (1.0f - c));
}
XMFLOAT4 F4(V3 v, float w) { return { v.x, v.y, v.z, w }; }
V3 Rgb(float r, float g, float b) { return { r, g, b }; }
XMFLOAT4 C4(float r, float g, float b) { return { r, g, b, 1.0f }; }

V3 RandomUnit(Rng& rng) {
    float z = rng.Range(-1.0f, 1.0f), phi = rng.Range(0.0f, kTwoPi), r = std::sqrt(std::max(0.0f, 1.0f - z * z));
    return { r * std::cos(phi), z, r * std::sin(phi) };
}

// Any unit vector perpendicular to n.
V3 Perp(V3 n) { return Norm(std::fabs(n.y) < 0.9f ? Cross(n, { 0, 1, 0 }) : Cross(n, { 1, 0, 0 })); }

// Near hit distance of a straight ray against a sphere at the origin, or -1.
float SphereHit(V3 ro, V3 rd, float radius) {
    float b = Dot(ro, rd), c = Dot(ro, ro) - radius * radius, disc = b * b - c;
    return disc < 0.0f ? -1.0f : -b - std::sqrt(disc);
}

enum Shader { ShBlackHole, ShBoson, ShWhiteHole, ShTzo, ShStrange, ShEmber, ShStar };

const wchar_t* const kKindKeys[CelestialsSettings::kKinds] = {
    L"BlackHole", L"BosonStar", L"WhiteHole", L"ThorneZytkow", L"StrangeStar",
    L"ColdNeutronStar", L"RedDwarf", L"SunLike", L"BlueGiant", L"RedGiant",
};

constexpr float kBaseFov = 50.0f;
constexpr float kWarpTime = 1.7f;   // each half of a warp jump
constexpr float kFadeTime = 1.0f;   // each half of a fade through black
constexpr size_t kMaxParticles = 9000;

} // namespace

// ---- settings ----

const wchar_t* CelestialsSettings::KindKey(int kind) { return kKindKeys[kind]; }

CelestialsSettings CelestialsSettings::Load(const Settings& s) {
    CelestialsSettings v;
    for (int k = 0; k < kKinds; ++k) v.enabled[k] = s.GetBool(kKindKeys[k], v.enabled[k]);
    v.seconds = Clamp(s.GetInt(L"Seconds", v.seconds), 5, 120);
    v.orbit = Clamp(s.GetInt(L"Orbit", v.orbit), 1, 10);
    v.disk = s.GetBool(L"Disk", v.disk);
    v.warp = s.GetBool(L"Warp", v.warp);
    v.reduceGlare = s.GetBool(L"ReduceGlare", v.reduceGlare);
    v.quality = Clamp(s.GetInt(L"Quality", v.quality), 0, 2);
    if (!v.AnyEnabled()) for (bool& e : v.enabled) e = true;
    return v;
}

void CelestialsSettings::Save(Settings& s) const {
    for (int k = 0; k < kKinds; ++k) s.SetBool(kKindKeys[k], enabled[k]);
    s.SetInt(L"Seconds", seconds);
    s.SetInt(L"Orbit", orbit);
    s.SetBool(L"Disk", disk);
    s.SetBool(L"Warp", warp);
    s.SetBool(L"ReduceGlare", reduceGlare);
    s.SetInt(L"Quality", quality);
}

bool CelestialsSettings::AnyEnabled() const {
    return std::any_of(std::begin(enabled), std::end(enabled), [](bool e) { return e; });
}

// ---- setup ----

void CelestialsSaver::Initialize(Device& device, const SaverContext& ctx) {
    m_ctx = ctx;
    m_rng = ctx.rng;
    m_settings = CelestialsSettings::Load(*ctx.settings);
    m_post.Create(device);
    m_sprites.Create(device);
    m_dot.FromImage(device, TextureFactory::SoftDot(64, 0.1f), true);

    ID3D11Device* d = device.Get();
    struct { const BYTE* code; size_t size; } shaders[kShaders] = {
        { g_BlackHole_ps, sizeof(g_BlackHole_ps) }, { g_Boson_ps, sizeof(g_Boson_ps) },
        { g_WhiteHole_ps, sizeof(g_WhiteHole_ps) }, { g_Tzo_ps, sizeof(g_Tzo_ps) },
        { g_Strange_ps, sizeof(g_Strange_ps) }, { g_Ember_ps, sizeof(g_Ember_ps) },
        { g_Star_ps, sizeof(g_Star_ps) },
    };
    for (int i = 0; i < kShaders; ++i)
        ThrowIfFailed(d->CreatePixelShader(shaders[i].code, shaders[i].size, nullptr, &m_objectPs[i]), "CreatePixelShader(Celestials object)");
    ThrowIfFailed(d->CreatePixelShader(g_BrightPass_ps, sizeof(g_BrightPass_ps), nullptr, &m_brightPs), "CreatePixelShader(BrightPass)");
    ThrowIfFailed(d->CreatePixelShader(g_Composite_ps, sizeof(g_Composite_ps), nullptr, &m_compositePs), "CreatePixelShader(Composite)");
    m_sceneCb.Create(device);
    m_brightCb.Create(device);
    m_compositeCb.Create(device);

    m_warpStars.resize(420);
    for (WarpStar& w : m_warpStars) w = { m_rng->Range(0.0f, kTwoPi), m_rng->Range(0.05f, 1.1f), m_rng->Range(0.5f, 1.5f), m_rng->Range(0.4f, 1.0f) };

    CreateTargets(device);
    StartVisit(PickNext());
}

float CelestialsSaver::RenderScale() const {
    if (m_ctx.previewMode) return 1.0f;
    return m_settings.quality == CelestialsSettings::Low ? 0.5f : (m_settings.quality == CelestialsSettings::High ? 1.0f : 0.75f);
}

void CelestialsSaver::CreateTargets(Device& device) {
    float s = RenderScale();
    int w = std::max(static_cast<int>(m_ctx.width * s), 8), h = std::max(static_cast<int>(m_ctx.height * s), 8);
    m_scene.Resize(device, w, h);
    int hw = std::max(m_ctx.width / 2, 4), hh = std::max(m_ctx.height / 2, 4);
    m_half.Resize(device, hw, hh);
    m_halfTmp.Resize(device, hw, hh);
    int ww = std::max(hw / 4, 4), wh = std::max(hh / 4, 4);
    m_wide.Resize(device, ww, wh);
    m_wideTmp.Resize(device, ww, wh);
}

Kind CelestialsSaver::PickNext() const {
    std::vector<int> choices;
    for (int k = 0; k < CelestialsSettings::kKinds; ++k)
        if (m_settings.enabled[k] && (!m_started || k != static_cast<int>(m_visit.kind))) choices.push_back(k);
    if (choices.empty()) return m_visit.kind;   // only the current type is enabled: a fresh take on it
    return static_cast<Kind>(choices[m_rng->Int(0, static_cast<int>(choices.size()) - 1)]);
}

// Rolls a new object of the given kind: parameters, colours, camera and particle emitters.
void CelestialsSaver::StartVisit(Kind kind) {
    Rng& rng = *m_rng;
    Visit v;
    v.kind = kind;
    v.seed = std::floor(rng.Range(0.0f, 500.0f));
    v.axis = Norm(V3{ rng.Range(-0.35f, 0.35f), 1.0f, rng.Range(-0.35f, 0.35f) });
    v.precess = v.axis;
    v.azimuth = rng.Range(0.0f, kTwoPi);
    v.elevation = rng.Range(-0.45f, 0.45f);
    v.roll = rng.Range(-0.18f, 0.18f);
    float hue = rng.Range(0.0f, 1.0f);
    XMFLOAT4 tint{ 0.5f + 0.5f * std::cos(kTwoPi * hue), 0.5f + 0.5f * std::cos(kTwoPi * (hue + 0.33f)), 0.5f + 0.5f * std::cos(kTwoPi * (hue + 0.67f)), 1.0f };
    v.sky = { tint.x * 0.8f + 0.1f, tint.y * 0.6f + 0.1f, tint.z * 0.9f + 0.15f, 1.0f };
    const bool glare = m_settings.reduceGlare;

    switch (kind) {
    case Kind::BlackHole:
        v.shader = ShBlackHole;
        v.camDist = rng.Range(21.0f, 26.0f);
        v.elevation = rng.Range(0.09f, 0.2f) * (rng.Chance(0.8f) ? 1.0f : -1.0f);
        v.p[0] = { 3.0f, rng.Range(11.0f, 14.0f), 0.8f, 0.55f };
        v.c[0] = C4(1.0f, 0.86f, 0.66f);
        v.c[1] = C4(1.0f, 0.46f, 0.1f);
        v.c[2] = C4(0.55f, 0.1f, 0.02f);
        v.exposure = 1.0f; v.bloom = 0.35f; v.wide = 0.3f; v.occluder = 0;
        break;
    case Kind::Boson:
        v.shader = ShBoson;
        v.camDist = rng.Range(24.0f, 28.0f);
        v.p[0] = { rng.Range(2.2f, 2.7f), rng.Range(0.9f, 1.2f), 1.0f, 0.12f };
        v.c[0] = C4(0.9f, 0.9f, 1.0f);
        v.exposure = 1.1f; v.bloom = 0.25f; v.wide = 0.15f; v.occluder = 0;
        v.stream = Norm(V3{ rng.Range(-1.0f, 1.0f), rng.Range(-0.3f, 0.3f), rng.Range(-1.0f, 1.0f) });
        v.streamU = Perp(v.stream);
        v.streamW = Cross(v.stream, v.streamU);
        break;
    case Kind::WhiteHole: {
        v.shader = ShWhiteHole;
        v.camDist = rng.Range(15.0f, 18.0f);
        v.elevation = rng.Range(0.18f, 0.38f) * (rng.Chance(0.5f) ? 1.0f : -1.0f);
        v.p[0] = { glare ? 10.0f : 24.0f, glare ? 1.2f : 2.4f, glare ? 1.2f : 2.0f, 14.0f };
        v.p[1] = { 0.085f, 13.0f, glare ? 0.6f : 1.0f, 0.35f };
        bool gold = rng.Chance(0.5f);
        v.c[0] = C4(0.95f, 0.97f, 1.0f);
        v.c[1] = C4(0.85f, 0.9f, 1.0f);
        v.c[2] = gold ? C4(1.0f, 0.68f, 0.32f) : C4(0.7f, 0.45f, 1.0f);
        v.c[3] = C4(0.8f, 0.9f, 1.0f);
        v.exposure = glare ? 0.75f : 0.85f; v.bloom = glare ? 0.35f : 0.7f; v.wide = glare ? 0.2f : 0.45f;
        v.occluder = 1;
        break;
    }
    case Kind::Tzo:
        v.shader = ShTzo;
        v.camDist = rng.Range(2.9f, 3.3f);
        v.spinRate = 0.012f;
        v.p[0] = { 0.14f, 9.0f, 0.35f, 2.2f };
        v.p[1] = { 0.7f, 1.0f, 0.5f, rng.Range(6.0f, 8.0f) };
        v.c[0] = C4(1.0f, 0.38f, 0.1f);
        v.c[1] = C4(0.35f, 0.04f, 0.02f);
        v.c[2] = C4(0.55f, 0.8f, 1.0f);
        v.c[3] = C4(0.75f, 0.88f, 1.0f);
        v.exposure = 1.0f; v.bloom = 0.5f; v.wide = 0.35f;
        break;
    case Kind::Strange:
        v.shader = ShStrange;
        v.camDist = rng.Range(5.2f, 6.2f);
        v.spinRate = 0.5f;
        v.precessRate = 0.18f;
        v.axis = Norm(V3{ rng.Range(-0.6f, 0.6f), 1.0f, rng.Range(-0.6f, 0.6f) });
        v.precess = Norm(V3{ 0.0f, 1.0f, 0.0f });
        v.p[0] = { 0.12f, 1.0f, 4.5f, 9.0f };
        v.p[1] = { 0.05f, 3.0f, 1.0f, 1.0f };
        v.c[0] = C4(0.25f, 0.02f, 0.45f);
        v.c[1] = C4(0.75f, 0.08f, 0.85f);
        v.c[2] = C4(0.35f, 1.0f, 0.08f);
        v.c[3] = C4(1.0f, 0.95f, 0.15f);
        v.exposure = 1.0f; v.bloom = 0.5f; v.wide = 0.35f; v.ca = 0.006f;
        break;
    case Kind::Ember:
        v.shader = ShEmber;
        v.camDist = rng.Range(6.0f, 7.0f);
        v.spinRate = 0.05f;
        v.p[0] = { 0.06f, 0.55f, 1.0f, 3.5f };
        v.c[0] = C4(0.9f, 0.1f, 0.02f);
        v.c[1] = C4(0.035f, 0.018f, 0.015f);
        v.c[2] = C4(0.6f, 0.06f, 0.02f);
        v.exposure = 1.3f; v.bloom = 0.5f; v.wide = 0.3f;
        break;
    case Kind::RedDwarf:
        v.shader = ShStar;
        v.camDist = rng.Range(3.8f, 4.4f);
        v.p[0] = { 3.0f, 1.4f, 1.0f, 0.35f };
        v.p[1] = { 0.6f, 0.6f, 0.25f, 0.5f };
        v.p[2] = { 0.25f, 1.0f, 0.0f, 22.0f };
        v.exposure = 0.8f;
        v.c[0] = C4(1.0f, 0.24f, 0.04f);
        v.c[1] = C4(0.25f, 0.02f, 0.01f);
        v.c[2] = C4(1.0f, 0.33f, 0.2f);
        break;
    case Kind::SunLike:
        v.shader = ShStar;
        v.camDist = rng.Range(3.8f, 4.4f);
        v.p[0] = { 3.0f, 1.0f, 1.0f, 0.6f };
        v.p[1] = { 0.55f, 0.7f, 0.35f, 0.7f };
        v.p[2] = { 0.3f, 0.9f, 0.0f, 26.0f };
        v.exposure = 0.8f;
        v.c[0] = C4(1.0f, 0.58f, 0.14f);
        v.c[1] = C4(0.7f, 0.22f, 0.02f);
        v.c[2] = C4(1.0f, 0.8f, 0.53f);
        break;
    case Kind::BlueGiant:
        v.shader = ShStar;
        v.camDist = rng.Range(4.3f, 4.9f);
        v.p[0] = { 2.5f, 1.8f, 1.1f, 0.0f };
        v.p[1] = { 0.4f, 0.9f, 0.5f, 0.6f };
        v.p[2] = { 0.15f, 1.5f, 0.0f, 18.0f };
        v.exposure = 0.85f;
        v.c[0] = C4(0.66f, 0.8f, 1.0f);
        v.c[1] = C4(0.08f, 0.14f, 0.4f);
        v.c[2] = C4(0.63f, 0.75f, 1.0f);
        break;
    case Kind::RedGiant:
    default:
        v.shader = ShStar;
        v.camDist = rng.Range(3.2f, 3.6f);
        v.spinRate = 0.008f;
        v.p[0] = { 1.6f, 0.5f, 1.3f, 0.1f };
        v.p[1] = { 0.75f, 0.5f, 0.6f, 0.3f };
        v.p[2] = { 0.35f, 0.8f, 0.015f, 7.0f };
        v.exposure = 0.8f;
        v.c[0] = C4(1.0f, 0.36f, 0.07f);
        v.c[1] = C4(0.3f, 0.05f, 0.02f);
        v.c[2] = C4(1.0f, 0.4f, 0.2f);
        break;
    }

    m_visit = v;
    m_started = true;
    m_objTime = rng.Range(5.0f, 40.0f);
    m_spin = rng.Range(0.0f, kTwoPi);
    m_particles.clear();
    m_loops.clear();
    m_flareTimer = rng.Range(0.5f, 3.0f);
    m_cmeTimer = rng.Range(4.0f, 12.0f);
    m_burstTimer = rng.Range(1.0f, 4.0f);
    m_emitAcc = m_emitAcc2 = 0;
    // Pre-roll the particles so winds, loops and streams are already under way.
    for (int i = 0; i < 60; ++i) {
        UpdateEmitters(0.1f);
        UpdateParticles(0.1f);
    }
}

// ---- simulation ----

void CelestialsSaver::Emit(const Particle& p) {
    if (m_particles.size() < kMaxParticles) m_particles.push_back(p);
}

// A flare: a flash at the surface and a spray of plasma that arcs up and falls back.
void CelestialsSaver::Flare(float scale) {
    Rng& rng = *m_rng;
    V3 n = RandomUnit(rng);
    V3 hot = Rgb(m_visit.c[0].x, m_visit.c[0].y, m_visit.c[0].z) * 2.5f + Rgb(0.6f, 0.5f, 0.35f);
    Emit({ n * 1.01f, {}, hot * 1.6f, 0.22f * scale, 0.6f, 0, 1.4f, 0, 0, 0 });
    int count = static_cast<int>(130 * scale);
    for (int i = 0; i < count; ++i) {
        V3 vel = n * rng.Range(0.3f, 0.85f) + RandomUnit(rng) * 0.14f;
        Emit({ n * 1.01f, vel * scale, hot * 0.6f, rng.Range(0.01f, 0.022f) * scale, 0, 0, rng.Range(2.5f, 4.5f), 0.1f, 0.5f, 0.22f });
    }
}

// A coronal mass ejection (as in Stargazer v1): a few huge, very soft blobs swelling outward
// in a cone, threaded with fine sparks.
void CelestialsSaver::Cme(float scale) {
    Rng& rng = *m_rng;
    V3 n = RandomUnit(rng);
    V3 col = Rgb(m_visit.c[2].x, m_visit.c[2].y, m_visit.c[2].z);
    for (int i = 0; i < 7; ++i) {
        V3 dir = Norm(n + RandomUnit(rng) * rng.Range(0.0f, 0.22f));
        Emit({ dir * rng.Range(1.1f, 1.3f), dir * rng.Range(0.16f, 0.3f), col * (0.07f * scale), rng.Range(0.35f, 0.6f) * scale,
               0.18f, 0, rng.Range(11.0f, 15.0f), 0, 0, 0 });
    }
    int count = static_cast<int>(160 * scale);
    for (int i = 0; i < count; ++i) {
        V3 dir = Norm(n + RandomUnit(rng) * rng.Range(0.0f, 0.35f));
        Emit({ dir * rng.Range(1.02f, 1.15f), dir * rng.Range(0.18f, 0.5f), col * rng.Range(0.25f, 0.6f),
               rng.Range(0.012f, 0.025f), 0, 0, rng.Range(9.0f, 14.0f), 0, 0.04f, 0 });
    }
}

void CelestialsSaver::UpdateEmitters(float dt) {
    Rng& rng = *m_rng;
    const Visit& v = m_visit;
    m_flareTimer -= dt;
    m_cmeTimer -= dt;
    m_burstTimer -= dt;

    auto loopsTo = [&](int target, V3 col) {
        if (static_cast<int>(m_loops.size()) >= target || !rng.Chance(dt * 0.6f)) return;
        V3 a = RandomUnit(rng);
        V3 b = Norm(a + RandomUnit(rng) * rng.Range(0.18f, 0.32f));
        m_loops.push_back({ a, b, col, rng.Range(0.1f, 0.32f), 0, rng.Range(14.0f, 26.0f) });
    };

    switch (v.kind) {
    case Kind::RedDwarf:
        if (m_flareTimer <= 0) { Flare(1.3f); m_flareTimer = rng.Range(2.0f, 5.0f); }
        if (m_cmeTimer <= 0) { Cme(0.8f); m_cmeTimer = rng.Range(12.0f, 25.0f); }
        loopsTo(2, Rgb(1.0f, 0.3f, 0.12f) * 2.0f);
        break;
    case Kind::SunLike:
        if (m_flareTimer <= 0) { Flare(1.0f); m_flareTimer = rng.Range(5.0f, 11.0f); }
        if (m_cmeTimer <= 0) { Cme(1.0f); m_cmeTimer = rng.Range(16.0f, 30.0f); }
        loopsTo(3, Rgb(1.0f, 0.42f, 0.18f) * 2.2f);
        break;
    case Kind::BlueGiant:
        if (m_flareTimer <= 0) { Flare(0.8f); m_flareTimer = rng.Range(9.0f, 16.0f); }
        m_emitAcc += dt * 90.0f;
        while (m_emitAcc >= 1.0f) {
            m_emitAcc -= 1.0f;
            V3 n = RandomUnit(rng);
            Emit({ n * 1.02f, n * rng.Range(0.6f, 1.4f), Rgb(0.55f, 0.7f, 1.0f) * rng.Range(0.3f, 0.8f),
                   rng.Range(0.015f, 0.035f), 0, 0, rng.Range(5.0f, 7.0f), 0, 0, 0.25f });
        }
        loopsTo(1, Rgb(0.6f, 0.75f, 1.0f) * 2.0f);
        break;
    case Kind::RedGiant:
        m_emitAcc += dt * 16.0f;
        while (m_emitAcc >= 1.0f) {
            m_emitAcc -= 1.0f;
            V3 n = RandomUnit(rng);
            Emit({ n * 1.03f, n * rng.Range(0.06f, 0.16f) + RandomUnit(rng) * 0.03f, Rgb(1.0f, 0.35f, 0.12f) * rng.Range(0.06f, 0.16f),
                   rng.Range(0.05f, 0.12f), 0.2f, 0, rng.Range(14.0f, 20.0f), 0, 0, 0 });
        }
        loopsTo(1, Rgb(1.0f, 0.35f, 0.12f) * 1.6f);
        break;
    case Kind::Tzo:
        if (m_burstTimer <= 0) {
            m_burstTimer = rng.Range(4.0f, 8.0f);
            V3 n = RandomUnit(rng);
            for (int i = 0; i < 260; ++i) {
                V3 dir = Norm(n + RandomUnit(rng) * rng.Range(0.0f, 0.3f));
                bool blue = rng.Chance(0.12f);
                V3 col = blue ? Rgb(0.6f, 0.8f, 1.0f) * 0.45f : Rgb(1.0f, 0.3f, 0.06f) * 0.35f;
                Emit({ dir * 1.01f, dir * rng.Range(0.07f, 0.24f) + RandomUnit(rng) * 0.03f, col * rng.Range(0.5f, 1.0f),
                       rng.Range(0.012f, 0.035f), 0.15f, 0, rng.Range(12.0f, 18.0f), 0, 0.02f, 0 });
            }
        }
        m_emitAcc += dt * 8.0f;
        while (m_emitAcc >= 1.0f) {
            m_emitAcc -= 1.0f;
            V3 n = RandomUnit(rng);
            Emit({ n * 1.02f, n * rng.Range(0.03f, 0.08f), Rgb(1.0f, 0.3f, 0.08f) * 0.15f, rng.Range(0.012f, 0.03f), 0.1f, 0, 14.0f, 0, 0, 0 });
        }
        break;
    case Kind::Boson:
        m_emitAcc += dt * 110.0f;
        while (m_emitAcc >= 1.0f) {
            m_emitAcc -= 1.0f;
            V3 pos = v.stream * -20.0f + v.streamU * rng.Range(-7.0f, 7.0f) + v.streamW * rng.Range(-2.5f, 2.5f);
            float grey = rng.Range(0.07f, 0.2f);
            V3 col = Rgb(grey, grey * rng.Range(0.9f, 1.0f), grey * rng.Range(0.95f, 1.15f));
            Emit({ pos, v.stream * rng.Range(1.0f, 1.4f), col, rng.Range(0.025f, 0.05f), 0, 0, 34.0f, 0, 3.2f, 0.05f });
        }
        break;
    case Kind::Strange: {
        V3 axis = Norm(RotateAxis(v.axis, v.precess, m_objTime * v.precessRate));
        m_emitAcc += dt * 240.0f;
        const V3 jetCols[3] = { Rgb(0.35f, 1.0f, 0.1f), Rgb(0.8f, 0.15f, 1.0f), Rgb(1.0f, 0.95f, 0.2f) };
        while (m_emitAcc >= 1.0f) {
            m_emitAcc -= 1.0f;
            float side = rng.Chance(0.5f) ? 1.0f : -1.0f;
            V3 dir = axis * side;
            Emit({ dir * 1.1f + RandomUnit(rng) * 0.02f, dir * rng.Range(5.0f, 9.0f) + RandomUnit(rng) * 0.08f,
                   jetCols[rng.Int(0, 2)] * rng.Range(1.5f, 3.5f), rng.Range(0.018f, 0.032f), 0, 0, rng.Range(1.0f, 1.6f), 0, 0, 0.05f });
        }
        m_emitAcc2 += dt * 22.0f;
        while (m_emitAcc2 >= 1.0f) {
            m_emitAcc2 -= 1.0f;
            V3 n = RandomUnit(rng);
            Emit({ n * 1.01f, n * rng.Range(0.3f, 0.7f) + Perp(n) * rng.Range(-0.3f, 0.3f), jetCols[rng.Int(0, 2)] * 2.0f,
                   rng.Range(0.012f, 0.022f), 0, 0, rng.Range(0.8f, 1.6f), 0, 0.35f, 0.08f });
        }
        break;
    }
    case Kind::WhiteHole:
        m_emitAcc += dt * 360.0f;
        while (m_emitAcc >= 1.0f) {
            m_emitAcc -= 1.0f;
            V3 n = RandomUnit(rng);
            Emit({ n * 1.02f, n * rng.Range(1.2f, 3.0f), Rgb(0.85f, 0.92f, 1.0f) * rng.Range(1.5f, 3.5f),
                   rng.Range(0.02f, 0.05f), 0, 0, rng.Range(5.0f, 8.0f), 0, -1.6f, 0.12f });
        }
        m_emitAcc2 += dt * 160.0f;
        while (m_emitAcc2 >= 1.0f) {
            m_emitAcc2 -= 1.0f;
            float a = rng.Range(0.0f, kTwoPi), r = rng.Range(1.3f, 2.6f);
            V3 radial{ std::cos(a), 0.0f, std::sin(a) };
            V3 tangent{ -std::sin(a), 0.0f, std::cos(a) };
            const XMFLOAT4& c = rng.Chance(0.5f) ? v.c[1] : v.c[2];
            Emit({ radial * r + V3{ 0.0f, rng.Range(-0.05f, 0.05f), 0.0f }, radial * rng.Range(0.8f, 1.6f) + tangent * 0.35f,
                   Rgb(c.x, c.y, c.z) * rng.Range(1.0f, 2.2f), rng.Range(0.025f, 0.05f), 0, 0, rng.Range(6.0f, 9.0f), 0, -0.8f, 0.15f });
        }
        break;
    case Kind::Ember:
        m_emitAcc += dt * 5.0f;
        while (m_emitAcc >= 1.0f) {
            m_emitAcc -= 1.0f;
            V3 pos = RandomUnit(rng) * rng.Range(1.3f, 4.0f);
            Emit({ pos, RandomUnit(rng) * rng.Range(0.02f, 0.07f), Rgb(1.0f, 0.25f, 0.05f) * rng.Range(0.4f, 1.3f),
                   rng.Range(0.012f, 0.03f), 0, 0, rng.Range(10.0f, 18.0f), 0, 0.02f, 0 });
        }
        break;
    default:
        break;
    }
}

void CelestialsSaver::UpdateParticles(float dt) {
    const float soft = m_visit.kind == Kind::Boson ? m_visit.p[0].x * m_visit.p[0].x : 0.05f;
    for (size_t i = 0; i < m_particles.size();) {
        Particle& p = m_particles[i];
        p.age += dt;
        float r2 = Dot(p.pos, p.pos);
        if (p.age >= p.life || r2 > 1600.0f) {
            p = m_particles.back();
            m_particles.pop_back();
            continue;
        }
        float d2 = r2 + soft;
        V3 acc = p.pos * (-p.pull / (d2 * std::sqrt(d2)));
        p.vel = (p.vel + acc * dt) * std::max(0.0f, 1.0f - p.drag * dt);
        p.pos = p.pos + p.vel * dt;
        ++i;
    }
    for (size_t i = 0; i < m_loops.size();) {
        m_loops[i].age += dt;
        if (m_loops[i].age >= m_loops[i].life) {
            m_loops[i] = m_loops.back();
            m_loops.pop_back();
            continue;
        }
        ++i;
    }
}

void CelestialsSaver::Update(float dt, double) {
    m_clock += dt;
    m_objTime += dt;
    m_spin += dt * m_visit.spinRate;
    m_visit.azimuth += dt * (0.012f + 0.008f * m_settings.orbit);
    m_phaseTime += dt;

    const bool warp = m_settings.warp;
    const float half = warp ? kWarpTime : kFadeTime;
    switch (m_phase) {
    case Phase::Hold:
        m_warp = 0; m_fade = 1; m_dolly = 1; m_fovBoost = 0;
        if (m_phaseTime >= static_cast<float>(m_settings.seconds)) { m_phase = Phase::Out; m_phaseTime = 0; }
        break;
    case Phase::Out: {
        float u = Saturate(m_phaseTime / half);
        if (warp) {
            m_warp = u * u;
            m_dolly = 1.0f - 0.45f * u * u;
            m_fovBoost = 22.0f * u * u;
            m_fade = 1.0f - 0.55f * std::pow(u, 4.0f);
        } else {
            m_fade = 1.0f - u;
        }
        if (u >= 1.0f) {
            StartVisit(PickNext());
            m_phase = Phase::In;
            m_phaseTime = 0;
        }
        break;
    }
    case Phase::In: {
        float u = Saturate(m_phaseTime / half);
        float r = 1.0f - u;
        if (warp) {
            m_warp = r * r;
            m_dolly = 1.0f + 1.3f * r * r;
            m_fovBoost = 22.0f * r * r;
            m_fade = 1.0f - 0.55f * std::pow(r, 4.0f);
        } else {
            m_fade = u;
            m_dolly = 1.0f;
        }
        if (u >= 1.0f) { m_phase = Phase::Hold; m_phaseTime = 0; }
        break;
    }
    }

    UpdateEmitters(dt);
    UpdateParticles(dt);

    for (WarpStar& w : m_warpStars) {
        w.r += w.speed * (0.05f + 2.4f * m_warp * m_warp) * dt;
        if (w.r > 1.15f) {
            w.r = m_rng->Range(0.02f, 0.25f);
            w.angle = m_rng->Range(0.0f, kTwoPi);
        }
    }
}

// ---- rendering ----

void CelestialsSaver::RenderParticles(Device& device, const V3& eye, const V3& fwd, const V3& up, const V3& right, float fovY) {
    const float W = static_cast<float>(m_scene.Width()), H = static_cast<float>(m_scene.Height());
    const float tanHalf = std::tan(fovY * 0.5f), aspect = W / H;
    const float occ = m_visit.occluder;

    // Projects a world point into scene-target pixels; returns false when behind the eye.
    auto project = [&](V3 p, float& x, float& y, float& z) {
        V3 d = p - eye;
        z = Dot(d, fwd);
        if (z < 0.05f) return false;
        x = (Dot(d, right) / (z * tanHalf * aspect) * 0.5f + 0.5f) * W;
        y = (0.5f - Dot(d, up) / (z * tanHalf) * 0.5f) * H;
        return true;
    };
    auto hidden = [&](V3 p) {
        if (occ <= 0.0f) return false;
        if (Dot(p, p) < occ * occ) return true;
        V3 d = p - eye;
        float L = Len(d);
        float t = SphereHit(eye, d * (1.0f / L), occ);
        return t > 0.0f && t < L;
    };
    const float pxPerUnit = H / (2.0f * tanHalf);

    m_sprites.Begin(m_scene.Width(), m_scene.Height());
    for (const Particle& p : m_particles) {
        if (hidden(p.pos)) continue;
        float x, y, z;
        if (!project(p.pos, x, y, z)) continue;
        float f = p.age / p.life;
        float env = Smoothstep(0.0f, 0.08f, f) * std::pow(1.0f - f, 1.5f);
        float size = std::max(p.size * (1.0f + p.grow * p.age) * pxPerUnit / z, 1.2f);
        // Keep sub-pixel particles from shimmering: shrink the brightness, not the sprite.
        float fade = std::min(1.0f, p.size * pxPerUnit / z / 1.2f);
        XMFLOAT4 col{ p.col.x * env * fade, p.col.y * env * fade, p.col.z * env * fade, 1.0f };
        if (p.streak > 0.0f) {
            float x2, y2, z2;
            if (project(p.pos - p.vel * p.streak, x2, y2, z2)) {
                float dx = x - x2, dy = y - y2;
                float len = std::sqrt(dx * dx + dy * dy);
                m_sprites.Push((x + x2) * 0.5f, (y + y2) * 0.5f, len + size, size, col, { 0, 0, 1, 1 }, std::atan2(dy, dx));
                continue;
            }
        }
        m_sprites.Push(x, y, size, size, col);
    }

    // Prominence loops: glowing arcs of plasma between two footpoints, pulsing along their length.
    for (const Loop& l : m_loops) {
        float f = l.age / l.life;
        float env = Smoothstep(0.0f, 0.15f, f) * (1.0f - Smoothstep(0.75f, 1.0f, f));
        const int n = 40;
        for (int i = 0; i <= n; ++i) {
            float s = static_cast<float>(i) / n;
            V3 dir = Norm(l.a * (1.0f - s) + l.b * s);
            V3 p = dir * (1.0f + l.height * std::sin(kPi * s) * (0.85f + 0.15f * f));
            if (hidden(p)) continue;
            float x, y, z;
            if (!project(p, x, y, z)) continue;
            float pulse = 0.6f + 0.4f * std::sin(s * 18.0f - l.age * 2.5f);
            float size = std::max(0.09f * pxPerUnit / z, 1.5f);
            float k = env * pulse * 0.6f;
            m_sprites.Push(x, y, size, size, { l.col.x * k, l.col.y * k, l.col.z * k, 1.0f });
        }
    }

    // Warp streaks radiating from the centre.
    if (m_warp > 0.01f) {
        float cx = W * 0.5f, cy = H * 0.5f, halfDiag = std::sqrt(cx * cx + cy * cy);
        float width = std::max(1.2f, 1.6f * m_ctx.dpiScale * RenderScale());
        for (const WarpStar& w : m_warpStars) {
            float r = w.r * halfDiag;
            float len = std::max(2.0f, r * m_warp * 0.55f);
            float ca = std::cos(w.angle), sa = std::sin(w.angle);
            float mid = r - len * 0.5f;
            float k = w.bright * m_warp * 1.8f * Smoothstep(0.0f, 0.2f, w.r);
            m_sprites.Push(cx + ca * mid, cy + sa * mid, len, width, { 0.65f * k, 0.78f * k, 1.0f * k, 1.0f }, { 0, 0, 1, 1 }, w.angle);
        }
    }

    const States& states = m_post.GetStates();
    m_sprites.End(device, &m_dot, states.Additive());
}

void CelestialsSaver::Render(Device& device, SwapChain& swap) {
    ID3D11DeviceContext* ctx = device.Ctx();
    const States& states = m_post.GetStates();
    if (!m_scene.Valid()) CreateTargets(device);
    const Visit& v = m_visit;

    // Camera: slow orbit, gentle bob in elevation, a little roll.
    float el = v.elevation + 0.06f * std::sin(m_objTime * 0.07f);
    float dist = v.camDist * m_dolly;
    V3 eye{ dist * std::cos(el) * std::cos(v.azimuth), dist * std::sin(el), dist * std::cos(el) * std::sin(v.azimuth) };
    V3 fwd = Norm(eye * -1.0f);
    V3 right = Norm(Cross({ 0, 1, 0 }, fwd));
    V3 up = Cross(fwd, right);
    float cr = std::cos(v.roll), sr = std::sin(v.roll);
    V3 r2 = right * cr + up * sr;
    V3 u2 = up * cr - right * sr;
    right = r2;
    up = u2;
    float fovY = ToRadians(kBaseFov + m_fovBoost);
    float tanHalf = std::tan(fovY * 0.5f);
    float aspect = static_cast<float>(m_scene.Width()) / m_scene.Height();

    V3 axis = v.precessRate > 0.0f ? Norm(RotateAxis(v.axis, v.precess, m_objTime * v.precessRate)) : v.axis;
    static const int kSteps[3] = { 150, 220, 320 };

    SceneCB cb{};
    cb.camPos = F4(eye, m_objTime);
    cb.camRight = F4(right, aspect);
    cb.camUp = F4(up, tanHalf);
    cb.camFwd = F4(fwd, 2.0f * tanHalf / m_scene.Height());
    cb.axis = F4(axis, v.seed);
    for (int i = 0; i < 4; ++i) { cb.p[i] = v.p[i]; cb.c[i] = v.c[i]; }
    cb.sky = v.sky;
    cb.misc = { static_cast<float>(kSteps[m_settings.quality]), m_settings.disk ? 1.0f : 0.0f, 0.0f, m_spin };
    m_sceneCb.Update(ctx, cb);

    // 1. The object (and its sky) into the HDR scene target.
    states.Set2D(ctx, states.Opaque());
    m_scene.Bind(ctx);
    m_sceneCb.BindPS(ctx, 0);
    m_post.Draw(ctx, m_objectPs[v.shader].Get());

    // 2. Particles, prominences and warp streaks on top, additively.
    RenderParticles(device, eye, fwd, up, right, fovY);

    // 3. Bloom: bright-pass downsample to half size, blur, then a much wider level.
    ID3D11ShaderResourceView* none[3] = { nullptr, nullptr, nullptr };
    BrightCB bc{ { 1.0f, 0.5f, 1.0f / m_scene.Width(), 1.0f / m_scene.Height() } };
    m_brightCb.Update(ctx, bc);
    states.Set2D(ctx, states.Opaque());
    m_half.Bind(ctx);
    m_brightCb.BindPS(ctx, 0);
    m_scene.BindPS(ctx, 0);
    ID3D11SamplerState* samp = states.LinearClamp();
    ctx->PSSetSamplers(0, 1, &samp);
    m_post.Draw(ctx, m_brightPs.Get());
    ctx->PSSetShaderResources(0, 1, none);
    m_post.GaussianBlur(device, m_half, m_halfTmp, m_half, 2);
    m_wide.Bind(ctx);
    m_post.Copy(ctx, m_half.SRV());
    ctx->PSSetShaderResources(0, 1, none);
    m_post.GaussianBlur(device, m_wide, m_wideTmp, m_wide, 3);

    // 4. Composite into this saver's viewport.
    swap.Bind();
    swap.SetViewport(m_ctx.viewport);
    CompositeCB cc{};
    cc.a = { v.exposure, v.bloom, v.wide, v.ca };
    cc.b = { m_warp, m_fade, std::fmod(m_clock, 97.0f), 0.35f };
    m_compositeCb.Update(ctx, cc);
    m_compositeCb.BindPS(ctx, 0);
    states.Set2D(ctx, states.Opaque());
    ID3D11ShaderResourceView* srvs[3] = { m_scene.SRV(), m_half.SRV(), m_wide.SRV() };
    ctx->PSSetShaderResources(0, 3, srvs);
    ctx->PSSetSamplers(0, 1, &samp);
    m_post.Draw(ctx, m_compositePs.Get());
    ctx->PSSetShaderResources(0, 3, none);
}

void CelestialsSaver::Resize(int width, int height) {
    m_ctx.width = width;
    m_ctx.height = height;
    m_ctx.viewport = { 0, 0, width, height };
    m_scene = RenderTexture{};
    m_half = RenderTexture{};
    m_halfTmp = RenderTexture{};
    m_wide = RenderTexture{};
    m_wideTmp = RenderTexture{};
}
