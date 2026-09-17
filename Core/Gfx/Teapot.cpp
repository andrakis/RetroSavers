#include "Teapot.h"
#include "TeapotData.h"
#include "Util/MathUtil.h"
#include <cmath>

using namespace DirectX;

namespace rs {

namespace {

XMVECTOR Bezier(const XMVECTOR p[4], float t) {
    float it = 1.0f - t;
    float b0 = it * it * it, b1 = 3 * t * it * it, b2 = 3 * t * t * it, b3 = t * t * t;
    return XMVectorAdd(XMVectorAdd(XMVectorScale(p[0], b0), XMVectorScale(p[1], b1)), XMVectorAdd(XMVectorScale(p[2], b2), XMVectorScale(p[3], b3)));
}

XMVECTOR BezierDeriv(const XMVECTOR p[4], float t) {
    float it = 1.0f - t;
    XMVECTOR a = XMVectorScale(XMVectorSubtract(p[1], p[0]), 3 * it * it);
    XMVECTOR b = XMVectorScale(XMVectorSubtract(p[2], p[1]), 6 * it * t);
    XMVECTOR c = XMVectorScale(XMVectorSubtract(p[3], p[2]), 3 * t * t);
    return XMVectorAdd(XMVectorAdd(a, b), c);
}

// Evaluate a 4x4 control grid at (u, v) with partial derivatives.
void EvalPatch(const XMVECTOR cp[4][4], float u, float v, XMVECTOR& pos, XMVECTOR& du, XMVECTOR& dv) {
    XMVECTOR rows[4], rowsD[4];
    for (int i = 0; i < 4; ++i) {
        rows[i] = Bezier(cp[i], u);
        rowsD[i] = BezierDeriv(cp[i], u);
    }
    pos = Bezier(rows, v);
    dv = BezierDeriv(rows, v);
    du = Bezier(rowsD, v);
}

void AddPatch(MeshData& m, const XMVECTOR cp[4][4], int n, bool flip) {
    uint32_t base = static_cast<uint32_t>(m.vertices.size());
    for (int i = 0; i <= n; ++i) {
        float v = static_cast<float>(i) / n;
        for (int j = 0; j <= n; ++j) {
            float u = static_cast<float>(j) / n;
            XMVECTOR pos, du, dv;
            EvalPatch(cp, u, v, pos, du, dv);
            XMVECTOR nrm = XMVector3Cross(du, dv);
            // Degenerate corners (lid/bottom poles): nudge inward to get a usable tangent frame.
            if (XMVectorGetX(XMVector3LengthSq(nrm)) < 1e-10f) {
                float uu = Clamp(u, 0.02f, 0.98f), vv = Clamp(v, 0.02f, 0.98f);
                XMVECTOR p2, du2, dv2;
                EvalPatch(cp, uu, vv, p2, du2, dv2);
                nrm = XMVector3Cross(du2, dv2);
            }
            nrm = XMVector3Normalize(nrm);
            if (flip) nrm = XMVectorNegate(nrm);
            VertexPNT vert;
            XMStoreFloat3(&vert.position, pos);
            XMStoreFloat3(&vert.normal, nrm);
            vert.uv = { u, v };
            m.vertices.push_back(vert);
        }
    }
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j) {
            uint32_t a = base + i * (n + 1) + j, b = a + 1, c = a + n + 1, d = c + 1;
            if (!flip) m.indices.insert(m.indices.end(), { a, c, b, b, c, d });
            else       m.indices.insert(m.indices.end(), { a, b, c, b, d, c });
        }
}

} // namespace

MeshData BuildTeapot(float size, int tessellation) {
    using namespace TeapotData;
    MeshData m;
    const int n = std::max(2, tessellation);

    // Decide once whether cross(du,dv) points outward for the un-mirrored data by probing
    // the body patch against its radial direction; mirrored copies invert the sense.
    bool globalFlip = false;
    {
        XMVECTOR cp[4][4];
        for (int j = 0; j < 4; ++j)
            for (int k = 0; k < 4; ++k) {
                const float* a = kPoints[kPatches[1][j * 4 + k]];
                cp[j][k] = XMVectorSet(a[0], a[1], a[2], 1);
            }
        XMVECTOR pos, du, dv;
        EvalPatch(cp, 0.5f, 0.5f, pos, du, dv);
        XMVECTOR nrm = XMVector3Cross(du, dv);
        XMVECTOR radial = XMVectorSet(XMVectorGetX(pos), XMVectorGetY(pos), 0, 0);
        globalFlip = XMVectorGetX(XMVector3Dot(nrm, radial)) < 0.0f;
    }

    for (int p = 0; p < kPatchCount; ++p) {
        XMVECTOR cpA[4][4], cpB[4][4], cpC[4][4], cpD[4][4];
        for (int j = 0; j < 4; ++j)
            for (int k = 0; k < 4; ++k) {
                const float* a = kPoints[kPatches[p][j * 4 + k]];
                const float* b = kPoints[kPatches[p][j * 4 + (3 - k)]];
                cpA[j][k] = XMVectorSet(a[0], a[1], a[2], 1);
                cpB[j][k] = XMVectorSet(b[0], -b[1], b[2], 1);   // mirror across y
                cpC[j][k] = XMVectorSet(-b[0], b[1], b[2], 1);   // mirror across x
                cpD[j][k] = XMVectorSet(-a[0], -a[1], a[2], 1);  // both
            }
        AddPatch(m, cpA, n, globalFlip);
        AddPatch(m, cpB, n, !globalFlip);
        if (p < 6) {
            AddPatch(m, cpC, n, !globalFlip);
            AddPatch(m, cpD, n, globalFlip);
        }
    }

    // Make triangle winding agree with the normals (D3D clockwise-front convention).
    for (size_t i = 0; i + 2 < m.indices.size(); i += 3) {
        const auto& a = m.vertices[m.indices[i]];
        const auto& b = m.vertices[m.indices[i + 1]];
        const auto& c = m.vertices[m.indices[i + 2]];
        XMVECTOR pa = XMLoadFloat3(&a.position), pb = XMLoadFloat3(&b.position), pc = XMLoadFloat3(&c.position);
        XMVECTOR cross = XMVector3Cross(XMVectorSubtract(pb, pa), XMVectorSubtract(pc, pa));
        XMVECTOR nn = XMVectorAdd(XMVectorAdd(XMLoadFloat3(&a.normal), XMLoadFloat3(&b.normal)), XMLoadFloat3(&c.normal));
        if (XMVectorGetX(XMVector3Dot(cross, nn)) < 0.0f) std::swap(m.indices[i + 1], m.indices[i + 2]);
    }

    // z-up -> y-up, centre, scale to `size` tall. Height spans z in [0, 3.15].
    float scale = size / 3.15f;
    XMMATRIX xf = XMMatrixTranslation(0, 0, -1.575f) * XMMatrixRotationX(-XM_PIDIV2) * XMMatrixScaling(scale, scale, scale);
    m.Transform(xf);
    return m;
}

} // namespace rs
