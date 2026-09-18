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

// 3D value noise for Rock (the 2D one lives in TextureFactory).
float Hash3(int x, int y, int z, uint32_t seed) {
    uint32_t h = static_cast<uint32_t>(x) * 0x8DA6B343u ^ static_cast<uint32_t>(y) * 0xD8163841u ^ static_cast<uint32_t>(z) * 0xCB1AB31Fu ^ seed * 0x9E3779B9u;
    h ^= h >> 15; h *= 0x2C1B3C6Du; h ^= h >> 12; h *= 0x297A2D39u; h ^= h >> 15;
    return (h & 0xFFFFFFu) / 16777215.0f;
}

float ValueNoise3(float x, float y, float z, uint32_t seed) {
    int xi = static_cast<int>(std::floor(x)), yi = static_cast<int>(std::floor(y)), zi = static_cast<int>(std::floor(z));
    float fx = x - xi, fy = y - yi, fz = z - zi;
    fx = fx * fx * (3 - 2 * fx); fy = fy * fy * (3 - 2 * fy); fz = fz * fz * (3 - 2 * fz);
    float c[8];
    for (int i = 0; i < 8; ++i) c[i] = Hash3(xi + (i & 1), yi + ((i >> 1) & 1), zi + (i >> 2), seed);
    float x0 = Lerp(c[0], c[1], fx), x1 = Lerp(c[2], c[3], fx), x2 = Lerp(c[4], c[5], fx), x3 = Lerp(c[6], c[7], fx);
    return Lerp(Lerp(x0, x1, fy), Lerp(x2, x3, fy), fz);
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

MeshData Rock(float radius, uint32_t seed, float roughness, float squash, int slices, int stacks) {
    MeshData m = Sphere(1.0f, slices, stacks);
    // Displace along the normal by two octaves of noise, then squash and flatten the base.
    for (auto& v : m.vertices) {
        const XMFLOAT3& n = v.normal;
        float f = ValueNoise3(n.x * 1.7f + 5.0f, n.y * 1.7f + 5.0f, n.z * 1.7f + 5.0f, seed) - 0.5f;
        f += 0.5f * (ValueNoise3(n.x * 3.9f + 9.0f, n.y * 3.9f + 9.0f, n.z * 3.9f + 9.0f, seed + 31) - 0.5f);
        float r = radius * (1.0f + roughness * 2.0f * f);
        float y = n.y * r * squash;
        if (y < -0.35f * radius * squash) y = -0.35f * radius * squash;
        v.position = { n.x * r, y + 0.35f * radius * squash, n.z * r };
    }
    // Collapse the pole rows: every vertex on a pole shares one position.
    const int cols = slices + 1;
    for (int j = 1; j < cols; ++j) {
        m.vertices[j].position = m.vertices[0].position;
        m.vertices[stacks * cols + j].position = m.vertices[stacks * cols].position;
    }
    m.ComputeNormals();
    // The seam column (u = 0 / u = 1) shares positions: average the normals so it doesn't show.
    for (int i = 0; i <= stacks; ++i) {
        VertexPNT& a = m.vertices[i * cols];
        VertexPNT& b = m.vertices[i * cols + slices];
        XMVECTOR n = XMVector3Normalize(XMVectorAdd(XMLoadFloat3(&a.normal), XMLoadFloat3(&b.normal)));
        XMStoreFloat3(&a.normal, n);
        XMStoreFloat3(&b.normal, n);
    }
    return m;
}

MeshData Gear(float innerRadius, float outerRadius, float width, int teeth, float toothDepth) {
    MeshData m;
    const float r0 = innerRadius, r1 = outerRadius - toothDepth * 0.5f, r2 = outerRadius + toothDepth * 0.5f;
    const float da = kTwoPi / teeth / 4.0f, hw = width * 0.5f;
    // Every quad gets its own four vertices so faces stay flat, as in gears.c.
    auto quad = [&](const XMFLOAT3& a, const XMFLOAT3& b, const XMFLOAT3& c, const XMFLOAT3& d, const XMFLOAT3& n) {
        uint32_t base = static_cast<uint32_t>(m.vertices.size());
        m.vertices.push_back({ a, n, { 0, 0 } });
        m.vertices.push_back({ b, n, { 1, 0 } });
        m.vertices.push_back({ c, n, { 1, 1 } });
        m.vertices.push_back({ d, n, { 0, 1 } });
        AddQuad(m, base, base + 1, base + 2, base + 3);
    };
    auto P = [](float r, float angle, float z) { return XMFLOAT3{ r * std::cos(angle), r * std::sin(angle), z }; };

    for (int i = 0; i < teeth; ++i) {
        float a = i * kTwoPi / teeth;
        for (float z : { hw, -hw }) {
            XMFLOAT3 n{ 0, 0, z > 0 ? 1.0f : -1.0f };
            // Face: the ring between the bore and the tooth root, one wedge per tooth (the
            // second quad repeats a corner: it is the sliver triangle under the gap).
            quad(P(r0, a, z), P(r1, a, z), P(r1, a + 3 * da, z), P(r0, a + 4 * da, z), n);
            quad(P(r0, a + 4 * da, z), P(r1, a + 3 * da, z), P(r1, a + 4 * da, z), P(r0, a + 4 * da, z), n);
            // Tooth cap on this face.
            quad(P(r1, a, z), P(r2, a + da, z), P(r2, a + 2 * da, z), P(r1, a + 3 * da, z), n);
        }
        // Outward faces of the tooth: root -> flank -> tip -> flank -> root, then the gap.
        const float angles[5] = { a, a + da, a + 2 * da, a + 3 * da, a + 4 * da };
        const float radii[5] = { r1, r2, r2, r1, r1 };
        for (int k = 0; k < 4; ++k) {
            XMFLOAT3 p0 = P(radii[k], angles[k], hw), p1 = P(radii[k + 1], angles[k + 1], hw);
            float dx = p1.x - p0.x, dy = p1.y - p0.y;
            float len = std::sqrt(dx * dx + dy * dy);
            XMFLOAT3 n{ dy / len, -dx / len, 0 };
            quad(p0, P(radii[k], angles[k], -hw), P(radii[k + 1], angles[k + 1], -hw), p1, n);
        }
        // Bore: inside cylinder, normals pointing in.
        XMFLOAT3 n0{ -std::cos(a), -std::sin(a), 0 }, n1{ -std::cos(a + 4 * da), -std::sin(a + 4 * da), 0 };
        uint32_t base = static_cast<uint32_t>(m.vertices.size());
        m.vertices.push_back({ P(r0, a, -hw), n0, { 0, 0 } });
        m.vertices.push_back({ P(r0, a, hw), n0, { 0, 1 } });
        m.vertices.push_back({ P(r0, a + 4 * da, hw), n1, { 1, 1 } });
        m.vertices.push_back({ P(r0, a + 4 * da, -hw), n1, { 1, 0 } });
        AddQuad(m, base, base + 1, base + 2, base + 3);
    }
    FixWinding(m);
    return m;
}

} // namespace rs::Primitives
