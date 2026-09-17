#include "Primitives.h"
#include "Util/MathUtil.h"
#include <cmath>

using namespace DirectX;

namespace rs::Primitives {

namespace {

// Make every triangle's (b-a)x(c-a) agree with its vertex normals, so that with
// D3D's default clockwise-front rule the outward faces are the front faces.
void FixWinding(MeshData& m) {
    for (size_t i = 0; i + 2 < m.indices.size(); i += 3) {
        const auto& a = m.vertices[m.indices[i]];
        const auto& b = m.vertices[m.indices[i + 1]];
        const auto& c = m.vertices[m.indices[i + 2]];
        XMVECTOR pa = XMLoadFloat3(&a.position), pb = XMLoadFloat3(&b.position), pc = XMLoadFloat3(&c.position);
        XMVECTOR cross = XMVector3Cross(XMVectorSubtract(pb, pa), XMVectorSubtract(pc, pa));
        XMVECTOR n = XMVectorAdd(XMVectorAdd(XMLoadFloat3(&a.normal), XMLoadFloat3(&b.normal)), XMLoadFloat3(&c.normal));
        if (XMVectorGetX(XMVector3Dot(cross, n)) < 0.0f) std::swap(m.indices[i + 1], m.indices[i + 2]);
    }
}

void AddQuad(MeshData& m, uint32_t a, uint32_t b, uint32_t c, uint32_t d) {
    m.indices.insert(m.indices.end(), { a, b, c, a, c, d });
}

} // namespace

MeshData Sphere(float radius, int slices, int stacks) {
    MeshData m;
    for (int i = 0; i <= stacks; ++i) {
        float v = static_cast<float>(i) / stacks;
        float phi = v * kPi;
        float y = std::cos(phi), r = std::sin(phi);
        for (int j = 0; j <= slices; ++j) {
            float u = static_cast<float>(j) / slices;
            float theta = u * kTwoPi;
            XMFLOAT3 n{ r * std::sin(theta), y, r * std::cos(theta) };
            m.vertices.push_back({ { n.x * radius, n.y * radius, n.z * radius }, n, { u, v } });
        }
    }
    for (int i = 0; i < stacks; ++i)
        for (int j = 0; j < slices; ++j) {
            uint32_t a = i * (slices + 1) + j, b = a + 1, c = a + slices + 1, d = c + 1;
            AddQuad(m, a, b, d, c);
        }
    FixWinding(m);
    return m;
}

MeshData Cylinder(float radius, float length, int slices, bool caps) {
    MeshData m;
    for (int ring = 0; ring < 2; ++ring) {
        float z = ring ? length : 0.0f;
        for (int j = 0; j <= slices; ++j) {
            float u = static_cast<float>(j) / slices;
            float theta = u * kTwoPi;
            XMFLOAT3 n{ std::cos(theta), std::sin(theta), 0 };
            m.vertices.push_back({ { n.x * radius, n.y * radius, z }, n, { u, ring ? 1.0f : 0.0f } });
        }
    }
    for (int j = 0; j < slices; ++j) {
        uint32_t a = j, b = j + 1, c = slices + 1 + j, d = c + 1;
        AddQuad(m, a, b, d, c);
    }
    if (caps) {
        for (int ring = 0; ring < 2; ++ring) {
            float z = ring ? length : 0.0f;
            XMFLOAT3 n{ 0, 0, ring ? 1.0f : -1.0f };
            uint32_t center = static_cast<uint32_t>(m.vertices.size());
            m.vertices.push_back({ { 0, 0, z }, n, { 0.5f, 0.5f } });
            for (int j = 0; j <= slices; ++j) {
                float theta = static_cast<float>(j) / slices * kTwoPi;
                float cx = std::cos(theta), cy = std::sin(theta);
                m.vertices.push_back({ { cx * radius, cy * radius, z }, n, { 0.5f + 0.5f * cx, 0.5f + 0.5f * cy } });
            }
            for (int j = 0; j < slices; ++j)
                m.indices.insert(m.indices.end(), { center, center + 1 + j, center + 2 + j });
        }
    }
    FixWinding(m);
    return m;
}

MeshData ElbowTube(float radius, float bend, int slices, int segments) {
    MeshData m;
    for (int s = 0; s <= segments; ++s) {
        float t = static_cast<float>(s) / segments * (kPi * 0.5f);
        float st = std::sin(t), ct = std::cos(t);
        XMFLOAT3 axis{ 0, bend - bend * ct, bend * st };
        XMFLOAT3 radial{ 0, -ct, st };  // unit vector from arc centre to the axis point
        for (int j = 0; j <= slices; ++j) {
            float u = static_cast<float>(j) / slices;
            float phi = u * kTwoPi;
            float cp = std::cos(phi), sp = std::sin(phi);
            XMFLOAT3 n{ cp, sp * radial.y, sp * radial.z };
            m.vertices.push_back({ { axis.x + n.x * radius, axis.y + n.y * radius, axis.z + n.z * radius }, n, { u, static_cast<float>(s) / segments } });
        }
    }
    for (int s = 0; s < segments; ++s)
        for (int j = 0; j < slices; ++j) {
            uint32_t a = s * (slices + 1) + j, b = a + 1, c = a + slices + 1, d = c + 1;
            AddQuad(m, a, b, d, c);
        }
    FixWinding(m);
    return m;
}

MeshData Box(float sx, float sy, float sz) {
    MeshData m;
    float hx = sx * 0.5f, hy = sy * 0.5f, hz = sz * 0.5f;
    struct Face { XMFLOAT3 n, u, v; };
    const Face faces[] = {
        { { 1, 0, 0 }, { 0, 0, -1 }, { 0, 1, 0 } }, { { -1, 0, 0 }, { 0, 0, 1 }, { 0, 1, 0 } },
        { { 0, 1, 0 }, { 1, 0, 0 }, { 0, 0, -1 } }, { { 0, -1, 0 }, { 1, 0, 0 }, { 0, 0, 1 } },
        { { 0, 0, 1 }, { 1, 0, 0 }, { 0, 1, 0 } },  { { 0, 0, -1 }, { -1, 0, 0 }, { 0, 1, 0 } },
    };
    for (const auto& f : faces) {
        uint32_t base = static_cast<uint32_t>(m.vertices.size());
        for (int k = 0; k < 4; ++k) {
            float su = (k == 1 || k == 2) ? 1.0f : -1.0f;
            float sv = (k >= 2) ? -1.0f : 1.0f;
            XMFLOAT3 p{ (f.n.x + f.u.x * su + f.v.x * sv) * hx, (f.n.y + f.u.y * su + f.v.y * sv) * hy, (f.n.z + f.u.z * su + f.v.z * sv) * hz };
            m.vertices.push_back({ p, f.n, { su > 0 ? 1.0f : 0.0f, sv > 0 ? 0.0f : 1.0f } });
        }
        AddQuad(m, base, base + 1, base + 2, base + 3);
    }
    FixWinding(m);
    return m;
}

MeshData Quad(float w, float h) {
    MeshData m;
    float hw = w * 0.5f, hh = h * 0.5f;
    XMFLOAT3 n{ 0, 0, -1 };
    m.vertices = {
        { { -hw, hh, 0 }, n, { 0, 0 } }, { { hw, hh, 0 }, n, { 1, 0 } },
        { { hw, -hh, 0 }, n, { 1, 1 } }, { { -hw, -hh, 0 }, n, { 0, 1 } },
    };
    AddQuad(m, 0, 1, 2, 3);
    FixWinding(m);
    return m;
}

MeshData Grid(float w, float d, int divisionsX, int divisionsZ, float uvRepeat) {
    MeshData m;
    XMFLOAT3 n{ 0, 1, 0 };
    for (int z = 0; z <= divisionsZ; ++z)
        for (int x = 0; x <= divisionsX; ++x) {
            float fx = static_cast<float>(x) / divisionsX, fz = static_cast<float>(z) / divisionsZ;
            m.vertices.push_back({ { (fx - 0.5f) * w, 0, (0.5f - fz) * d }, n, { fx * uvRepeat, fz * uvRepeat } });
        }
    for (int z = 0; z < divisionsZ; ++z)
        for (int x = 0; x < divisionsX; ++x) {
            uint32_t a = z * (divisionsX + 1) + x, b = a + 1, c = a + divisionsX + 1, dd = c + 1;
            AddQuad(m, a, b, dd, c);
        }
    FixWinding(m);
    return m;
}

} // namespace rs::Primitives
