#include "Pipes3DSaver.h"
#include "Gfx/BmpReader.h"
#include "Gfx/Device.h"
#include "Gfx/Primitives.h"
#include "Gfx/SwapChain.h"
#include "Gfx/Teapot.h"
#include "Gfx/TextureFactory.h"
#include "Util/Color.h"
#include "Util/MathUtil.h"

using namespace DirectX;
using namespace rs;

// ---------------------------------------------------------------- settings

Pipes3DSettings Pipes3DSettings::Load(const Settings& s) {
    Pipes3DSettings v;
    v.multiple = s.GetBool(L"Multiple", true);
    v.jointType = Clamp(s.GetInt(L"JointType", Mixed), 0, 3);
    v.surface = Clamp(s.GetInt(L"Surface", Solid), 0, 1);
    v.texturePath = s.GetString(L"TexturePath", L"");
    v.resolution = Clamp(s.GetInt(L"Resolution", 16), 6, 32);
    v.speed = Clamp(s.GetInt(L"Speed", 5), 1, 10);
    v.smoothGrowth = s.GetBool(L"SmoothGrowth", false);
    return v;
}

void Pipes3DSettings::Save(Settings& s) const {
    s.SetBool(L"Multiple", multiple);
    s.SetInt(L"JointType", jointType);
    s.SetInt(L"Surface", surface);
    s.SetString(L"TexturePath", texturePath);
    s.SetInt(L"Resolution", resolution);
    s.SetInt(L"Speed", speed);
    s.SetBool(L"SmoothGrowth", smoothGrowth);
}

// ---------------------------------------------------------------- grid helpers

XMINT3 Pipes3DSaver::Step(const XMINT3& c, int dir) {
    static const XMINT3 d[6] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
    return { c.x + d[dir].x, c.y + d[dir].y, c.z + d[dir].z };
}

XMFLOAT3 Pipes3DSaver::DirVec(int dir) {
    static const XMFLOAT3 d[6] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
    return d[dir];
}

XMFLOAT3 Pipes3DSaver::Center(const XMINT3& c) {
    return { c.x + 0.5f, c.y + 0.5f, c.z + 0.5f };
}

bool Pipes3DSaver::InGrid(const XMINT3& c) const {
    return c.x >= 0 && c.x < kW && c.y >= 0 && c.y < kH && c.z >= 0 && c.z < kD;
}

bool Pipes3DSaver::Occupied(const XMINT3& c) const {
    return !InGrid(c) || m_grid[(static_cast<size_t>(c.z) * kH + c.y) * kW + c.x] != 0;
}

void Pipes3DSaver::Occupy(const XMINT3& c) {
    uint8_t& v = m_grid[(static_cast<size_t>(c.z) * kH + c.y) * kW + c.x];
    if (!v) { v = 1; ++m_occupied; }
}

XMFLOAT4 Pipes3DSaver::RandomColor() {
    Rng& rng = *m_ctx.rng;
    if (m_settings.surface == Pipes3DSettings::Textured) return { 1, 1, 1, 1 };
    return HsvToRgb(rng.Float(), rng.Range(0.75f, 1.0f), rng.Range(0.8f, 1.0f));
}

// ---------------------------------------------------------------- lifecycle

void Pipes3DSaver::Initialize(Device& device, const SaverContext& ctx) {
    m_ctx = ctx;
    m_device = &device;
    m_settings = Pipes3DSettings::Load(*ctx.settings);
    m_forward.Create(device);

    const int slices = m_settings.resolution;
    m_cylinder.Create(device, Primitives::Cylinder(kPipeRadius, 1.0f, slices, true));
    m_sphere.Create(device, Primitives::Sphere(1.0f, slices, std::max(6, slices * 3 / 4)));
    m_elbow.Create(device, Primitives::ElbowTube(kPipeRadius, kBendRadius, slices, std::max(4, slices / 2)));
    m_teapot.Create(device, BuildTeapot(1.15f, std::max(4, slices / 2)));

    if (m_settings.surface == Pipes3DSettings::Textured) {
        std::optional<Image> img = BmpReader::Load(m_settings.texturePath);
        if (!img) img = TextureFactory::Marble();
        m_texture.FromImage(device, *img);
        m_material.texture = &m_texture;
        m_material.uvScale = 1.0f;
    }
    m_material.specPower = 48.0f;
    m_material.specIntensity = 0.55f;

    m_stepInterval = 1.0f / (4.0f + 2.2f * m_settings.speed);
    m_grid.assign(static_cast<size_t>(kW) * kH * kD, 0);
    ResetScene();
}

void Pipes3DSaver::SetupCamera() {
    Rng& rng = *m_ctx.rng;
    m_camera.eye = { kW * 0.5f + rng.Range(-5.0f, 5.0f), kH * 0.5f + rng.Range(1.0f, 7.0f), -5.0f - rng.Range(0.0f, 4.0f) };
    m_camera.target = { kW * 0.5f + rng.Range(-3.0f, 3.0f), kH * 0.45f, kD * 0.5f };
    m_camera.up = { 0, 1, 0 };
    m_camera.fovY = ToRadians(58.0f);
    m_camera.aspect = static_cast<float>(m_ctx.width) / static_cast<float>(std::max(m_ctx.height, 1));
    m_camera.nearZ = 0.15f;
    m_camera.farZ = 120.0f;
}

void Pipes3DSaver::ResetScene() {
    std::fill(m_grid.begin(), m_grid.end(), static_cast<uint8_t>(0));
    m_occupied = 0;
    m_spawnFailures = 0;
    m_cylinders.Clear();
    m_spheres.Clear();
    m_elbows.Clear();
    m_teapots.Clear();
    m_pipeCount = m_settings.multiple ? kMaxPipes : 1;
    for (auto& p : m_pipes) p = Pipe{};
    for (int i = 0; i < m_pipeCount; ++i) SpawnPipe(m_pipes[i]);
    SetupCamera();
    m_resetFlash = 0.15f;   // brief black frame between scenes, like the original
}

bool Pipes3DSaver::SpawnPipe(Pipe& p) {
    Rng& rng = *m_ctx.rng;
    for (int attempt = 0; attempt < 50; ++attempt) {
        XMINT3 c{ rng.Int(0, kW - 1), rng.Int(0, kH - 1), rng.Int(0, kD - 1) };
        if (Occupied(c)) continue;
        int dir = rng.Int(0, 5);
        if (Occupied(Step(c, dir))) continue;
        p = Pipe{};
        p.cell = c;
        p.dir = dir;
        p.color = RandomColor();
        p.alive = true;
        switch (m_settings.jointType) {
        case Pipes3DSettings::Elbow: p.joint = 0; break;
        case Pipes3DSettings::Ball: p.joint = 1; break;
        case Pipes3DSettings::Mixed: p.joint = -1; break;                  // decided per turn
        default: p.joint = (m_cycleIndex++ % 2); break;                     // Cycle: alternate per pipe
        }
        Occupy(c);
        AddSphere(Center(c), kBallRadius, p.color);
        p.headStart = Center(c);
        p.headEnd = Center(Step(c, dir));
        p.hasHead = true;
        p.cell = Step(c, dir);
        Occupy(p.cell);
        m_spawnFailures = 0;
        return true;
    }
    ++m_spawnFailures;
    return false;
}

int Pipes3DSaver::ChooseDirection(const Pipe& p) {
    Rng& rng = *m_ctx.rng;
    bool straightFree = !Occupied(Step(p.cell, p.dir));
    if (straightFree && rng.Chance(0.7f)) return p.dir;
    int options[4];
    int n = 0;
    for (int d = 0; d < 6; ++d) {
        if (d == p.dir || d == (p.dir ^ 1)) continue;   // not straight, not backwards
        if (!Occupied(Step(p.cell, d))) options[n++] = d;
    }
    if (n > 0) return options[rng.Int(0, n - 1)];
    return straightFree ? p.dir : -1;
}

void Pipes3DSaver::Advance(Pipe& p) {
    Rng& rng = *m_ctx.rng;
    // 1. Commit the link that just finished growing.
    if (p.hasHead) {
        p.lastCylinder = AddCylinder(p.headStart, p.headEnd, p.color);
        p.lastLinkStart = p.headStart;
        p.hasHead = false;
    }
    // 2. Pick where to go next.
    int nd = ChooseDirection(p);
    if (nd < 0) {
        p.alive = false;
        AddSphere(Center(p.cell), kBallRadius, p.color);
        return;
    }
    // 3. Joint at the current cell when turning.
    XMFLOAT3 c = Center(p.cell);
    XMFLOAT3 linkStart = c;
    if (nd != p.dir) {
        int joint = p.joint >= 0 ? p.joint : rng.Int(0, 1);
        if (rng.Chance(kTeapotChance)) {
            AddTeapot(c, p.color);
        } else if (joint == 1) {
            AddSphere(c, kBallRadius, p.color);
        } else {
            // Elbow spans entry face -> exit face; trim the incoming link to the entry face.
            XMFLOAT3 in = DirVec(p.dir), out = DirVec(nd);
            XMFLOAT3 entry{ c.x - in.x * kBendRadius, c.y - in.y * kBendRadius, c.z - in.z * kBendRadius };
            if (p.lastCylinder >= 0) SetCylinder(p.lastCylinder, p.lastLinkStart, entry);
            AddElbow(c, p.dir, nd, p.color);
            linkStart = { c.x + out.x * kBendRadius, c.y + out.y * kBendRadius, c.z + out.z * kBendRadius };
        }
    }
    // 4. Start the next link.
    p.dir = nd;
    p.cell = Step(p.cell, nd);
    Occupy(p.cell);
    p.headStart = linkStart;
    p.headEnd = Center(p.cell);
    p.hasHead = true;
}

void Pipes3DSaver::Update(float dt, double) {
    if (m_resetFlash > 0.0f) { m_resetFlash -= dt; return; }
    m_stepAccumulator += dt;
    int steps = 0;
    while (m_stepAccumulator >= m_stepInterval && steps < 4) {
        m_stepAccumulator -= m_stepInterval;
        ++steps;
        bool anyAlive = false;
        for (int i = 0; i < m_pipeCount; ++i) {
            Pipe& p = m_pipes[i];
            if (!p.alive) { if (SpawnPipe(p)) anyAlive = true; continue; }
            Advance(p);
            anyAlive = anyAlive || p.alive;
        }
        const int total = kW * kH * kD;
        if (m_occupied > total * 55 / 100 || (!anyAlive && m_spawnFailures > 10)) {
            ResetScene();
            m_stepAccumulator = 0.0f;
            return;
        }
    }
    if (m_stepAccumulator > m_stepInterval) m_stepAccumulator = 0.0f;
}

// ---------------------------------------------------------------- geometry instancing

XMMATRIX Pipes3DSaver::CylinderWorld(const XMFLOAT3& a, const XMFLOAT3& b) {
    XMVECTOR va = XMLoadFloat3(&a), vb = XMLoadFloat3(&b);
    XMVECTOR d = XMVectorSubtract(vb, va);
    float len = XMVectorGetX(XMVector3Length(d));
    if (len < 1e-4f) return XMMatrixScaling(0, 0, 0);
    XMVECTOR z = XMVectorScale(d, 1.0f / len);
    XMVECTOR helper = std::fabs(XMVectorGetY(z)) < 0.9f ? XMVectorSet(0, 1, 0, 0) : XMVectorSet(1, 0, 0, 0);
    XMVECTOR x = XMVector3Normalize(XMVector3Cross(helper, z));
    XMVECTOR y = XMVector3Cross(z, x);
    XMMATRIX rot(x, y, z, XMVectorSet(0, 0, 0, 1));
    return XMMatrixScaling(1, 1, len) * rot * XMMatrixTranslationFromVector(va);
}

int Pipes3DSaver::AddCylinder(const XMFLOAT3& a, const XMFLOAT3& b, const XMFLOAT4& color) {
    InstanceData inst;
    XMStoreFloat4x4(&inst.world, CylinderWorld(a, b));
    inst.color = color;
    m_cylinders.items.push_back(inst);
    m_cylinders.dirty = true;
    return static_cast<int>(m_cylinders.items.size()) - 1;
}

void Pipes3DSaver::SetCylinder(int index, const XMFLOAT3& a, const XMFLOAT3& b) {
    if (index < 0 || index >= static_cast<int>(m_cylinders.items.size())) return;
    XMStoreFloat4x4(&m_cylinders.items[index].world, CylinderWorld(a, b));
    m_cylinders.dirty = true;
}

void Pipes3DSaver::AddSphere(const XMFLOAT3& c, float radius, const XMFLOAT4& color) {
    InstanceData inst;
    XMStoreFloat4x4(&inst.world, XMMatrixScaling(radius, radius, radius) * XMMatrixTranslation(c.x, c.y, c.z));
    inst.color = color;
    m_spheres.items.push_back(inst);
    m_spheres.dirty = true;
}

void Pipes3DSaver::AddElbow(const XMFLOAT3& cellCenter, int inDir, int outDir, const XMFLOAT4& color) {
    // Local frame: +Z = incoming direction, +Y = outgoing direction, origin = entry face.
    XMFLOAT3 inVec = DirVec(inDir), outVec = DirVec(outDir);
    XMVECTOR z = XMLoadFloat3(&inVec);
    XMVECTOR y = XMLoadFloat3(&outVec);
    XMVECTOR x = XMVector3Cross(y, z);
    XMVECTOR origin = XMVectorSubtract(XMLoadFloat3(&cellCenter), XMVectorScale(z, kBendRadius));
    XMMATRIX world(x, y, z, XMVectorSetW(origin, 1.0f));
    InstanceData inst;
    XMStoreFloat4x4(&inst.world, world);
    inst.color = color;
    m_elbows.items.push_back(inst);
    m_elbows.dirty = true;
}

void Pipes3DSaver::AddTeapot(const XMFLOAT3& c, const XMFLOAT4& color) {
    float yaw = m_ctx.rng->Range(0.0f, kTwoPi);
    InstanceData inst;
    XMStoreFloat4x4(&inst.world, XMMatrixRotationY(yaw) * XMMatrixTranslation(c.x, c.y, c.z));
    inst.color = color;
    m_teapots.items.push_back(inst);
    m_teapots.dirty = true;
}

void Pipes3DSaver::UploadInstances(Device& device, InstanceList& list) {
    if (!list.dirty) return;
    list.buffer.Update(device, list.items.data(), list.items.size());
    list.dirty = false;
}

// ---------------------------------------------------------------- render

void Pipes3DSaver::Render(Device& device, SwapChain&) {
    if (m_resetFlash > 0.0f) return;   // black frame
    ID3D11DeviceContext* ctx = device.Ctx();

    m_camera.aspect = static_cast<float>(m_ctx.width) / static_cast<float>(std::max(m_ctx.height, 1));
    m_forward.GetStates().SetOpaque3D(ctx);
    m_forward.BeginFrame(ctx, m_camera, { -0.35f, 0.8f, -0.5f }, { 0.95f, 0.95f, 0.95f }, { 0.22f, 0.22f, 0.24f });

    UploadInstances(device, m_cylinders);
    UploadInstances(device, m_spheres);
    UploadInstances(device, m_elbows);
    UploadInstances(device, m_teapots);
    m_forward.DrawInstanced(ctx, m_cylinder, m_cylinders.buffer, m_material);
    m_forward.DrawInstanced(ctx, m_sphere, m_spheres.buffer, m_material);
    m_forward.DrawInstanced(ctx, m_elbow, m_elbows.buffer, m_material);
    m_forward.DrawInstanced(ctx, m_teapot, m_teapots.buffer, m_material);

    // Growing links: full length unless smooth growth is on.
    float t = m_settings.smoothGrowth ? Saturate(m_stepAccumulator / m_stepInterval) : 1.0f;
    for (int i = 0; i < m_pipeCount; ++i) {
        const Pipe& p = m_pipes[i];
        if (!p.alive || !p.hasHead || t <= 0.0f) continue;
        XMFLOAT3 end{ Lerp(p.headStart.x, p.headEnd.x, t), Lerp(p.headStart.y, p.headEnd.y, t), Lerp(p.headStart.z, p.headEnd.z, t) };
        Material m = m_material;
        m.color = p.color;
        m_forward.Draw(ctx, m_cylinder, CylinderWorld(p.headStart, end), m);
    }
}

void Pipes3DSaver::Resize(int width, int height) {
    m_ctx.width = width;
    m_ctx.height = height;
    m_ctx.viewport = { 0, 0, width, height };
}
