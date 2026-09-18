#include "FishMesh.h"
#include "Gfx/Primitives.h"
#include "Util/MathUtil.h"
#include <cmath>
#include <vector>

using namespace DirectX;
using namespace rs;

namespace {

// Field order: name | height width peakAt peduncle | tailLen tailHeight forkDepth |
// dorsal s e h | anal s e h | pectoralLen eyeSize | base accent fin tail | mode p1 p2 belly
// sheen tailGradient specPower specIntensity | size cruise swimAmp swimFreq schooling bottom min max
const FishSpecies kSpecies[kSpeciesCount] = {
    { L"Clownfish", 0.26f, 0.11f, 0.40f, 0.35f, 0.22f, 0.20f, 0.10f, 0.30f, 0.85f, 0.09f, 0.55f, 0.85f, 0.07f, 0.15f, 0.040f,
      { 1.00f, 0.45f, 0.06f }, { 0.98f, 0.98f, 0.95f }, { 1.00f, 0.50f, 0.10f }, { 1.00f, 0.50f, 0.10f }, 1, 3.0f, 0.055f, 0.15f,
      0.20f, false, 24.0f, 0.35f, 0.90f, 0.90f, 0.050f, 5.5f, false, false, 1, 1 },
    { L"Blue tang", 0.32f, 0.08f, 0.45f, 0.25f, 0.22f, 0.30f, 0.25f, 0.20f, 0.90f, 0.12f, 0.35f, 0.90f, 0.10f, 0.17f, 0.040f,
      { 0.08f, 0.28f, 0.95f }, { 0.02f, 0.02f, 0.06f }, { 0.08f, 0.28f, 0.95f }, { 1.00f, 0.85f, 0.10f }, 2, 0.60f, 0.80f, 0.10f,
      0.35f, false, 32.0f, 0.45f, 1.10f, 1.10f, 0.045f, 5.0f, false, false, 1, 1 },
    { L"Yellow tang", 0.36f, 0.06f, 0.45f, 0.22f, 0.20f, 0.30f, 0.15f, 0.20f, 0.90f, 0.16f, 0.30f, 0.90f, 0.14f, 0.16f, 0.040f,
      { 1.00f, 0.82f, 0.05f }, { 1.00f, 1.00f, 1.00f }, { 1.00f, 0.82f, 0.05f }, { 1.00f, 0.85f, 0.10f }, 0, 0.0f, 0.0f, 0.05f,
      0.30f, false, 32.0f, 0.40f, 1.00f, 1.00f, 0.045f, 5.0f, false, false, 1, 1 },
    { L"Angelfish", 0.36f, 0.07f, 0.50f, 0.30f, 0.28f, 0.36f, 0.10f, 0.30f, 0.80f, 0.35f, 0.35f, 0.80f, 0.35f, 0.30f, 0.045f,
      { 0.80f, 0.82f, 0.85f }, { 0.05f, 0.05f, 0.06f }, { 0.60f, 0.60f, 0.62f }, { 0.70f, 0.70f, 0.72f }, 1, 3.0f, 0.070f, 0.10f,
      0.50f, false, 40.0f, 0.55f, 1.10f, 0.70f, 0.040f, 4.5f, false, false, 1, 1 },
    { L"Neon tetra", 0.20f, 0.10f, 0.40f, 0.30f, 0.22f, 0.20f, 0.35f, 0.50f, 0.70f, 0.07f, 0.60f, 0.85f, 0.06f, 0.12f, 0.050f,
      { 0.75f, 0.78f, 0.80f }, { 0.10f, 0.85f, 1.00f }, { 0.80f, 0.85f, 0.90f }, { 0.90f, 0.10f, 0.10f }, 5, 0.0f, 0.0f, 0.10f,
      0.60f, false, 48.0f, 0.60f, 0.35f, 1.30f, 0.060f, 6.0f, true, false, 8, 16 },
    { L"Guppy", 0.20f, 0.11f, 0.35f, 0.35f, 0.45f, 0.45f, 0.00f, 0.50f, 0.70f, 0.12f, 0.50f, 0.70f, 0.05f, 0.12f, 0.050f,
      { 0.55f, 0.50f, 0.35f }, { 0.20f, 0.40f, 0.90f }, { 0.90f, 0.60f, 0.20f }, { 1.00f, 0.50f, 0.10f }, 0, 0.0f, 0.0f, 0.25f,
      0.40f, true, 28.0f, 0.40f, 0.45f, 0.90f, 0.060f, 6.0f, true, false, 4, 8 },
    { L"Goldfish", 0.30f, 0.20f, 0.42f, 0.30f, 0.45f, 0.40f, 0.45f, 0.35f, 0.80f, 0.12f, 0.55f, 0.80f, 0.08f, 0.20f, 0.045f,
      { 1.00f, 0.50f, 0.08f }, { 1.00f, 0.85f, 0.50f }, { 1.00f, 0.55f, 0.15f }, { 1.00f, 0.55f, 0.15f }, 4, 0.0f, 0.0f, 0.20f,
      0.35f, false, 28.0f, 0.45f, 0.95f, 0.60f, 0.045f, 4.5f, false, false, 1, 1 },
    { L"Pleco", 0.16f, 0.20f, 0.30f, 0.25f, 0.22f, 0.22f, 0.20f, 0.30f, 0.60f, 0.20f, 0.60f, 0.80f, 0.05f, 0.25f, 0.030f,
      { 0.22f, 0.20f, 0.14f }, { 0.55f, 0.50f, 0.35f }, { 0.25f, 0.22f, 0.16f }, { 0.25f, 0.22f, 0.16f }, 3, 14.0f, 0.28f, 0.30f,
      0.10f, false, 12.0f, 0.15f, 1.10f, 0.35f, 0.030f, 4.0f, false, true, 1, 1 },
};

// Half-extent along the body: a rounded nose up to `peakAt`, then a taper to `endFrac`.
float Profile(float u, float peakAt, float endFrac) {
    if (u < peakAt) {
        float s = u / peakAt;
        return std::sqrt(std::max(0.0f, 1.0f - (1.0f - s) * (1.0f - s)));
    }
    float s = (u - peakAt) / (1.0f - peakAt);
    return 1.0f - (1.0f - endFrac) * std::pow(s, 1.6f);
}

uint32_t AddV(MeshData& m, const XMFLOAT3& p, float u, float v) {
    m.vertices.push_back({ p, { 0, 1, 0 }, { u, v } });
    return static_cast<uint32_t>(m.vertices.size() - 1);
}

void AddTri(MeshData& m, uint32_t a, uint32_t b, uint32_t c) {
    m.indices.insert(m.indices.end(), { a, b, c });
}

} // namespace

const FishSpecies& GetSpecies(int index) {
    return kSpecies[Clamp(index, 0, kSpeciesCount - 1)];
}

MeshData BuildFishMesh(const FishSpecies& s, int quality) {
    const int sections = quality == 0 ? 14 : quality == 1 ? 22 : 30;
    const int ring = quality == 0 ? 8 : quality == 1 ? 12 : 16;
    const int tailRays = quality == 0 ? 5 : quality == 1 ? 8 : 12;
    const int finSegs = quality == 0 ? 4 : quality == 1 ? 6 : 8;

    auto halfH = [&](float u) { return std::max(s.height * Profile(u, s.peakAt, s.peduncle), 0.004f); };
    auto halfW = [&](float u) { return std::max(s.width * Profile(u, std::min(s.peakAt * 1.15f, 0.9f), s.peduncle * 1.2f), 0.004f); };
    auto centreY = [&](float u) { return -0.08f * s.height * std::sin(kPi * u); };   // the belly sags a little

    MeshData m;

    // Body: rings of `ring` vertices per section, u along the body, v = 0 belly .. 1 back.
    for (int i = 0; i <= sections; ++i) {
        float u = static_cast<float>(i) / sections;
        float x = 0.5f - u;
        float h = halfH(u), w = halfW(u), yc = centreY(u);
        for (int j = 0; j < ring; ++j) {
            float th = kTwoPi * j / ring;
            float cy = std::cos(th), sz = std::sin(th);
            float y = h * cy;
            if (y < 0.0f) y *= 1.0f + 0.25f * std::sin(kPi * u);   // deeper belly than back
            AddV(m, { x, yc + y, w * sz }, u, 0.5f + 0.5f * cy);
        }
    }
    for (int i = 0; i < sections; ++i)
        for (int j = 0; j < ring; ++j) {
            uint32_t a = i * ring + j, b = i * ring + (j + 1) % ring;
            uint32_t c = a + ring, d = b + ring;
            AddTri(m, a, c, b);
            AddTri(m, b, c, d);
        }

    // Tail fin: a fan of rays from a short base segment at the tail joint; the middle rays are
    // shortened by the fork. u = 1.1 marks it as the tail, v runs base -> tip.
    {
        float pedH = halfH(1.0f);
        float yc = centreY(1.0f);
        std::vector<uint32_t> base(tailRays + 1), tip(tailRays + 1);
        for (int r = 0; r <= tailRays; ++r) {
            float f = static_cast<float>(r) / tailRays;
            float spread = 2.0f * f - 1.0f;                       // -1 bottom .. 1 top
            float mid = 1.0f - std::abs(spread);
            float len = s.tailLen * (1.0f - s.forkDepth * 0.7f * mid);
            base[r] = AddV(m, { -0.5f, yc + pedH * spread * 0.9f, 0.0f }, 1.1f, 0.0f);
            tip[r] = AddV(m, { -0.5f - len, yc + s.tailHeight * spread * (0.75f + 0.25f * len / s.tailLen), 0.0f }, 1.1f, 1.0f);
        }
        for (int r = 0; r < tailRays; ++r) {
            AddTri(m, base[r], tip[r], tip[r + 1]);
            AddTri(m, base[r], tip[r + 1], base[r + 1]);
        }
    }

    // Dorsal and anal fins: strips along the back / belly whose height follows a sail curve.
    auto finStrip = [&](float u0, float u1, float height, float sign) {
        std::vector<uint32_t> base(finSegs + 1), top(finSegs + 1);
        for (int k = 0; k <= finSegs; ++k) {
            float sf = static_cast<float>(k) / finSegs;
            float u = Lerp(u0, u1, sf);
            float x = 0.5f - u;
            float edge = sign > 0.0f ? halfH(u) * 0.98f : halfH(u) * 1.2f;
            float baseY = centreY(u) + sign * edge;
            float shape = std::sin(kPi * std::pow(sf, 0.75f));
            base[k] = AddV(m, { x, baseY, 0.0f }, 1.4f, 0.0f);
            top[k] = AddV(m, { x - 0.04f * shape, baseY + sign * height * shape, 0.0f }, 1.4f, 1.0f);
        }
        for (int k = 0; k < finSegs; ++k) {
            AddTri(m, base[k], top[k], top[k + 1]);
            AddTri(m, base[k], top[k + 1], base[k + 1]);
        }
    };
    finStrip(s.dorsalStart, s.dorsalEnd, s.dorsalHeight, 1.0f);
    finStrip(s.analStart, s.analEnd, s.analHeight, -1.0f);

    // Pectoral fins: one swept-back quad on each flank.
    for (int side = -1; side <= 1; side += 2) {
        float u0 = 0.3f, x0 = 0.5f - u0;
        float y0 = centreY(u0) - 0.15f * halfH(u0);
        float z0 = side * halfW(u0) * 0.9f;
        float L = s.pectoralLen;
        uint32_t a = AddV(m, { x0, y0, z0 }, 1.5f, 0.0f);
        uint32_t b = AddV(m, { x0 - 0.1f, y0 - 0.02f, z0 * 0.95f }, 1.5f, 0.0f);
        uint32_t c = AddV(m, { x0 - L * 0.95f, y0 - L * 0.25f, z0 + side * L * 0.35f }, 1.5f, 1.0f);
        uint32_t d = AddV(m, { x0 - L * 0.7f, y0 - L * 0.35f, z0 + side * L * 0.7f }, 1.5f, 1.0f);
        AddTri(m, a, d, c);
        AddTri(m, a, c, b);
    }

    // Eyes: small spheres with the pole facing outwards (v = 0 at the pupil), uv.x offset by 2.
    for (int side = -1; side <= 1; side += 2) {
        float ue = 0.13f;
        MeshData eye = Primitives::Sphere(s.eyeSize, 8, 6);
        XMMATRIX t = XMMatrixRotationX(side * XM_PIDIV2) *
                     XMMatrixTranslation(0.5f - ue, centreY(ue) + 0.3f * halfH(ue), side * halfW(ue) * 0.8f);
        eye.Transform(t);
        for (auto& v : eye.vertices) v.uv.x += 2.0f;
        m.Append(eye);
    }

    m.ComputeNormals();
    return m;
}
