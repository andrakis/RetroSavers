#include "Maze3DSaver.h"
#include "Gfx/BmpReader.h"
#include "Gfx/Device.h"
#include "Gfx/Primitives.h"
#include "Gfx/SwapChain.h"
#include "Gfx/TextureFactory.h"
#include "Util/Color.h"
#include "Util/MathUtil.h"
#include <algorithm>

using namespace DirectX;
using namespace rs;

// ---------------------------------------------------------------- settings

Maze3DSettings Maze3DSettings::Load(const Settings& s) {
    Maze3DSettings v;
    v.wallTexture = s.GetString(L"WallTexture", L"");
    v.floorTexture = s.GetString(L"FloorTexture", L"");
    v.ceilingTexture = s.GetString(L"CeilingTexture", L"");
    v.showRat = s.GetBool(L"ShowRat", true);
    v.showLogo = s.GetBool(L"ShowLogo", true);
    v.mazeSize = Clamp(s.GetInt(L"MazeSize", 15), 9, 31) | 1;
    v.speed = Clamp(s.GetInt(L"Speed", 5), 1, 10);
    return v;
}

void Maze3DSettings::Save(Settings& s) const {
    s.SetString(L"WallTexture", wallTexture);
    s.SetString(L"FloorTexture", floorTexture);
    s.SetString(L"CeilingTexture", ceilingTexture);
    s.SetBool(L"ShowRat", showRat);
    s.SetBool(L"ShowLogo", showLogo);
    s.SetInt(L"MazeSize", mazeSize | 1);
    s.SetInt(L"Speed", speed);
}

// ---------------------------------------------------------------- maze

namespace {

// Heading 0 = +X, 1 = +Z, 2 = -X, 3 = -Z. Right of heading h is h-1, left is h+1.
const XMINT2 kHeadingStep[4] = { { 1, 0 }, { 0, 1 }, { -1, 0 }, { 0, -1 } };

float WrapAngle(float a) {
    while (a > kPi) a -= kTwoPi;
    while (a < -kPi) a += kTwoPi;
    return a;
}

XMFLOAT3 ForwardOf(float yaw) { return { std::cos(yaw), 0, std::sin(yaw) }; }
XMFLOAT3 RightOf(float yaw) { return { std::sin(yaw), 0, -std::cos(yaw) }; }

Texture LoadTextureOr(Device& device, const std::wstring& path, Image (*fallback)()) {
    Texture t;
    std::optional<Image> img = BmpReader::Load(path);
    if (!img) img = fallback();
    t.FromImage(device, *img);
    return t;
}

} // namespace

XMINT2 Maze3DSaver::StepCell(const XMINT2& c, int heading) {
    heading = ((heading % 4) + 4) % 4;
    return { c.x + kHeadingStep[heading].x, c.y + kHeadingStep[heading].y };
}

bool Maze3DSaver::Open(int x, int z) const {
    if (x < 0 || z < 0 || x >= m_size || z >= m_size) return false;
    return m_cells[static_cast<size_t>(z) * m_size + x] == 0;
}

XMFLOAT3 Maze3DSaver::CellCenter(const XMINT2& c, float y) const {
    return { c.x + 0.5f, y, c.y + 0.5f };
}

void Maze3DSaver::Generate() {
    Rng& rng = *m_ctx.rng;
    m_size = m_settings.mazeSize;
    m_cells.assign(static_cast<size_t>(m_size) * m_size, 1);

    // Recursive backtracker over the odd cells.
    std::vector<XMINT2> stack;
    auto carve = [&](int x, int z) { m_cells[static_cast<size_t>(z) * m_size + x] = 0; };
    carve(1, 1);
    stack.push_back({ 1, 1 });
    while (!stack.empty()) {
        XMINT2 c = stack.back();
        int order[4] = { 0, 1, 2, 3 };
        for (int i = 3; i > 0; --i) std::swap(order[i], order[rng.Int(0, i)]);
        bool moved = false;
        for (int h : order) {
            int nx = c.x + kHeadingStep[h].x * 2, nz = c.y + kHeadingStep[h].y * 2;
            if (nx <= 0 || nz <= 0 || nx >= m_size - 1 || nz >= m_size - 1) continue;
            if (Open(nx, nz)) continue;
            carve(c.x + kHeadingStep[h].x, c.y + kHeadingStep[h].y);
            carve(nx, nz);
            stack.push_back({ nx, nz });
            moved = true;
            break;
        }
        if (!moved) stack.pop_back();
    }

    m_start = { 1, 1 };
    m_exit = { m_size - 2, m_size - 2 };
    m_startHeading = 0;
    for (int h = 0; h < 4; ++h) {
        XMINT2 n = StepCell(m_start, h);
        if (Open(n.x, n.y)) { m_startHeading = h; break; }
    }

    m_rockCell = RandomCorridorCell(false);
    m_logoCell = RandomCorridorCell(false);
    m_globes.clear();
    for (int i = 0; i < 3; ++i) {
        XMINT2 c = RandomCorridorCell(true);
        bool dup = false;
        for (const auto& g : m_globes) dup = dup || (g.cell.x == c.x && g.cell.y == c.y);
        if (!dup) m_globes.push_back({ c, rng.Range(0.0f, kTwoPi) });
    }
    m_rats.clear();
    int ratCount = m_settings.showRat ? rng.Int(2, 3) : 0;
    for (int i = 0; i < ratCount; ++i) {
        Rat r;
        r.cell = RandomCorridorCell(false);
        r.heading = RandomOpenHeading(r.cell, -1);
        r.next = StepCell(r.cell, r.heading);
        r.progress = rng.Float();
        m_rats.push_back(r);
    }

    // Walker at the start, facing into the maze.
    m_cell = m_start;
    m_heading = m_startHeading;
    m_yaw = HeadingYaw(m_heading);
    m_roll = m_rollTarget = 0.0f;
    m_flipped = false;
}

XMINT2 Maze3DSaver::RandomCorridorCell(bool deadEndPreferred) {
    Rng& rng = *m_ctx.rng;
    XMINT2 best{ 1, 1 };
    for (int attempt = 0; attempt < 400; ++attempt) {
        XMINT2 c{ rng.Int(1, m_size - 2), rng.Int(1, m_size - 2) };
        if (!Open(c.x, c.y)) continue;
        if ((c.x == m_start.x && c.y == m_start.y) || (c.x == m_exit.x && c.y == m_exit.y)) continue;
        best = c;
        if (!deadEndPreferred) return c;
        int openCount = 0;
        for (int h = 0; h < 4; ++h) { XMINT2 n = StepCell(c, h); if (Open(n.x, n.y)) ++openCount; }
        if (openCount == 1) return c;
    }
    return best;
}

int Maze3DSaver::RandomOpenHeading(const XMINT2& cell, int avoidHeading) {
    Rng& rng = *m_ctx.rng;
    int options[4];
    int n = 0;
    for (int h = 0; h < 4; ++h) {
        if (h == avoidHeading) continue;
        XMINT2 next = StepCell(cell, h);
        if (Open(next.x, next.y)) options[n++] = h;
    }
    if (n == 0) return avoidHeading >= 0 ? avoidHeading : 0;
    return options[rng.Int(0, n - 1)];
}

// ---------------------------------------------------------------- geometry

void Maze3DSaver::BuildGeometry(Device& device) {
    MeshData walls, floor, ceiling;
    auto quad = [](MeshData& m, const XMFLOAT3 p[4], const XMFLOAT3& n) {
        uint32_t base = static_cast<uint32_t>(m.vertices.size());
        const XMFLOAT2 uv[4] = { { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 1 } };
        for (int k = 0; k < 4; ++k) m.vertices.push_back({ p[k], n, uv[k] });
        m.indices.insert(m.indices.end(), { base, base + 1, base + 2, base, base + 2, base + 3 });
    };

    for (int z = 0; z < m_size; ++z)
        for (int x = 0; x < m_size; ++x) {
            float cx = x + 0.5f, cz = z + 0.5f;
            if (Open(x, z)) {
                XMFLOAT3 f[4] = { { cx - 0.5f, 0, cz + 0.5f }, { cx + 0.5f, 0, cz + 0.5f }, { cx + 0.5f, 0, cz - 0.5f }, { cx - 0.5f, 0, cz - 0.5f } };
                quad(floor, f, { 0, 1, 0 });
                XMFLOAT3 c[4] = { { cx - 0.5f, 1, cz + 0.5f }, { cx + 0.5f, 1, cz + 0.5f }, { cx + 0.5f, 1, cz - 0.5f }, { cx - 0.5f, 1, cz - 0.5f } };
                quad(ceiling, c, { 0, -1, 0 });
                continue;
            }
            for (int h = 0; h < 4; ++h) {
                XMINT2 n = StepCell({ x, z }, h);
                if (!Open(n.x, n.y)) continue;
                XMFLOAT3 dir{ static_cast<float>(kHeadingStep[h].x), 0, static_cast<float>(kHeadingStep[h].y) };
                XMFLOAT3 side{ -dir.z, 0, dir.x };
                XMFLOAT3 fc{ cx + dir.x * 0.5f, 0, cz + dir.z * 0.5f };
                XMFLOAT3 p[4] = {
                    { fc.x - side.x * 0.5f, 1, fc.z - side.z * 0.5f }, { fc.x + side.x * 0.5f, 1, fc.z + side.z * 0.5f },
                    { fc.x + side.x * 0.5f, 0, fc.z + side.z * 0.5f }, { fc.x - side.x * 0.5f, 0, fc.z - side.z * 0.5f },
                };
                quad(walls, p, dir);
            }
        }
    walls.FixWinding();
    floor.FixWinding();
    ceiling.FixWinding();
    m_walls.Create(device, walls);
    m_floor.Create(device, floor);
    m_ceiling.Create(device, ceiling);
}

void Maze3DSaver::LoadTextures(Device& device) {
    m_wallTex = LoadTextureOr(device, m_settings.wallTexture, [] { return TextureFactory::Brick(); });
    m_floorTex = LoadTextureOr(device, m_settings.floorTexture, [] { return TextureFactory::WoodPlanks(); });
    m_ceilingTex = LoadTextureOr(device, m_settings.ceilingTexture, [] { return TextureFactory::CeilingTiles(); });
    m_logoTex.FromImage(device, TextureFactory::LogoText(L"OpenGL", 256, 128, RGB(255, 255, 255), RGB(0, 70, 150)));
    m_globeTex.FromImage(device, TextureFactory::Globe());
    m_smileyTex.FromImage(device, TextureFactory::Smiley(128));
    m_doorTex.FromImage(device, TextureFactory::Door(128));
    m_ratTex.FromImage(device, TextureFactory::Rat(128));
}

// ---------------------------------------------------------------- lifecycle

void Maze3DSaver::Initialize(Device& device, const SaverContext& ctx) {
    m_ctx = ctx;
    m_device = &device;
    m_settings = Maze3DSettings::Load(*ctx.settings);
    m_speedScale = 0.5f + 0.1f * m_settings.speed;

    m_forward.Create(device);
    m_billboard.Create(device);
    LoadTextures(device);
    m_cube.Create(device, Primitives::Box(0.34f, 0.34f, 0.34f));
    m_globe.Create(device, Primitives::Sphere(0.22f, 24, 16));
    {
        // Rock: a sphere with jittered radii.
        MeshData rock = Primitives::Sphere(0.18f, 10, 7);
        Rng& rng = *ctx.rng;
        std::vector<float> ringScale(8 * 11);
        for (auto& s : ringScale) s = rng.Range(0.7f, 1.25f);
        for (size_t i = 0; i < rock.vertices.size(); ++i) {
            float s = ringScale[i % ringScale.size()];
            auto& p = rock.vertices[i].position;
            p = { p.x * s, p.y * s, p.z * s };
        }
        rock.ComputeNormals();
        rock.FixWinding();
        m_rock.Create(device, rock);
    }

    Generate();
    BuildGeometry(device);
    m_state = State::FadeIn;
    m_fade = 0.0f;
}

// ---------------------------------------------------------------- navigation

void Maze3DSaver::Decide() {
    // Right-hand rule: right, forward, left, back.
    const int order[4] = { m_heading - 1, m_heading, m_heading + 1, m_heading + 2 };
    for (int h : order) {
        int hh = ((h % 4) + 4) % 4;
        XMINT2 next = StepCell(m_cell, hh);
        if (!Open(next.x, next.y)) continue;
        if (hh == m_heading) BeginMove();
        else BeginTurn(hh);
        return;
    }
}

void Maze3DSaver::BeginTurn(int newHeading) {
    m_heading = newHeading;
    m_yawFrom = m_yaw;
    m_yawTo = HeadingYaw(newHeading);
    float delta = std::fabs(WrapAngle(m_yawTo - m_yawFrom));
    m_duration = (delta > kPi * 0.75f ? 0.8f : 0.45f) / m_speedScale;
    m_t = 0.0f;
    m_state = State::Turning;
}

void Maze3DSaver::BeginMove() {
    m_nextCell = StepCell(m_cell, m_heading);
    m_duration = 0.55f / m_speedScale;
    m_t = 0.0f;
    m_state = State::Moving;
}

void Maze3DSaver::OnEnterCell(const XMINT2& cell) {
    if (cell.x == m_exit.x && cell.y == m_exit.y) {
        m_state = State::FadeOut;
        return;
    }
    if (cell.x == m_rockCell.x && cell.y == m_rockCell.y) {
        m_flipped = !m_flipped;
        m_rollTarget = m_flipped ? kPi : 0.0f;
    }
    m_state = State::Deciding;
}

void Maze3DSaver::UpdateRats(float dt) {
    const float ratSpeed = 1.5f;
    for (auto& r : m_rats) {
        r.progress += dt * ratSpeed;
        while (r.progress >= 1.0f) {
            r.progress -= 1.0f;
            r.cell = r.next;
            int back = (r.heading + 2) % 4;
            r.heading = RandomOpenHeading(r.cell, back);
            r.next = StepCell(r.cell, r.heading);
            if (!Open(r.next.x, r.next.y)) { r.heading = back; r.next = StepCell(r.cell, r.heading); }
        }
    }
}

void Maze3DSaver::Update(float dt, double) {
    m_time += dt;
    switch (m_state) {
    case State::FadeIn:
        m_fade += dt / 1.0f;
        if (m_fade >= 1.0f) { m_fade = 1.0f; m_state = State::Deciding; }
        break;
    case State::FadeOut:
        m_fade -= dt / 1.0f;
        if (m_fade <= 0.0f) {
            m_fade = 0.0f;
            Generate();
            BuildGeometry(*m_device);
            m_state = State::FadeIn;
        }
        break;
    case State::Deciding:
        Decide();
        break;
    case State::Turning:
        m_t += dt / m_duration;
        if (m_t >= 1.0f) { m_yaw = m_yawTo; BeginMove(); }
        else m_yaw = m_yawFrom + WrapAngle(m_yawTo - m_yawFrom) * Smoothstep(0.0f, 1.0f, m_t);
        break;
    case State::Moving:
        m_t += dt / m_duration;
        if (m_t >= 1.0f) { m_cell = m_nextCell; OnEnterCell(m_cell); }
        break;
    }

    // Upside-down roll.
    float rollStep = dt * (kPi / 0.5f);
    if (m_roll < m_rollTarget) m_roll = std::min(m_roll + rollStep, m_rollTarget);
    else if (m_roll > m_rollTarget) m_roll = std::max(m_roll - rollStep, m_rollTarget);

    UpdateRats(dt);

    // Camera.
    XMFLOAT3 pos = CellCenter(m_cell, 0.5f);
    if (m_state == State::Moving) {
        XMFLOAT3 next = CellCenter(m_nextCell, 0.5f);
        float t = Saturate(m_t);
        pos = { Lerp(pos.x, next.x, t), 0.5f, Lerp(pos.z, next.z, t) };
    }
    XMFLOAT3 fwd = ForwardOf(m_yaw), right = RightOf(m_yaw);
    float cr = std::cos(m_roll), sr = std::sin(m_roll);
    m_camera.eye = pos;
    m_camera.target = { pos.x + fwd.x, pos.y, pos.z + fwd.z };
    m_camera.up = { sr * right.x, cr, sr * right.z };
    m_camera.fovY = ToRadians(70.0f);
    m_camera.aspect = static_cast<float>(m_ctx.width) / static_cast<float>(std::max(m_ctx.height, 1));
    m_camera.nearZ = 0.05f;
    m_camera.farZ = 80.0f;
}

// ---------------------------------------------------------------- render

void Maze3DSaver::Render(Device& device, SwapChain&) {
    ID3D11DeviceContext* ctx = device.Ctx();
    const float fade = Smoothstep(0.0f, 1.0f, m_fade);

    XMFLOAT3 fwd = ForwardOf(m_yaw);
    FrameConstants f{};
    XMStoreFloat4x4(&f.viewProj, XMMatrixTranspose(m_camera.ViewProj()));
    f.eyePos = { m_camera.eye.x, m_camera.eye.y, m_camera.eye.z, 1 };
    XMStoreFloat4(&f.lightDir, XMVector3Normalize(XMVectorSet(-fwd.x, 0.6f, -fwd.z, 0)));
    f.lightColor = { 0.55f * fade, 0.55f * fade, 0.53f * fade, 1 };
    f.ambient = { 0.6f * fade, 0.6f * fade, 0.62f * fade, 1 };
    f.fogColor = { 0, 0, 0, 1 };
    f.fogParams = { 5.0f, 18.0f, 0, 0 };

    m_forward.GetStates().SetOpaque3D(ctx);
    m_forward.BeginFrame(ctx, f);

    Material wall;
    wall.texture = &m_wallTex;
    wall.specPower = 16.0f;
    wall.specIntensity = 0.08f;
    m_forward.Draw(ctx, m_walls, XMMatrixIdentity(), wall);
    Material floor = wall;
    floor.texture = &m_floorTex;
    floor.specIntensity = 0.25f;
    m_forward.Draw(ctx, m_floor, XMMatrixIdentity(), floor);
    Material ceil = wall;
    ceil.texture = &m_ceilingTex;
    ceil.specIntensity = 0.02f;
    m_forward.Draw(ctx, m_ceiling, XMMatrixIdentity(), ceil);

    DrawObjects(device, fade);
}

void Maze3DSaver::DrawObjects(Device& device, float fade) {
    ID3D11DeviceContext* ctx = device.Ctx();
    const float t = static_cast<float>(m_time);

    // Solid objects first.
    if (m_settings.showLogo && m_logoCell.x >= 0) {
        Material m;
        m.texture = &m_logoTex;
        m.specPower = 32.0f;
        m.specIntensity = 0.5f;
        XMFLOAT3 c = CellCenter(m_logoCell, 0.5f);
        m_forward.Draw(ctx, m_cube, XMMatrixRotationRollPitchYaw(t * 0.7f, t * 1.1f, 0) * XMMatrixTranslation(c.x, c.y, c.z), m);
    }
    if (m_rockCell.x >= 0) {
        Material m;
        m.color = { 0.55f, 0.55f, 0.57f, 1 };
        m.specPower = 12.0f;
        m.specIntensity = 0.2f;
        XMFLOAT3 c = CellCenter(m_rockCell, 0.5f);
        m_forward.Draw(ctx, m_rock, XMMatrixRotationRollPitchYaw(t * 0.9f, t * 1.4f, t * 0.3f) * XMMatrixTranslation(c.x, c.y, c.z), m);
    }
    for (const auto& g : m_globes) {
        Material m;
        m.texture = &m_globeTex;
        m.specPower = 24.0f;
        m.specIntensity = 0.35f;
        XMFLOAT3 c = CellCenter(g.cell, 0.5f);
        m_forward.Draw(ctx, m_globe, XMMatrixRotationY(g.phase + t * 0.8f) * XMMatrixRotationZ(0.35f) * XMMatrixTranslation(c.x, c.y, c.z), m);
    }

    // Sprites.
    m_billboard.BeginFrame(ctx, m_camera);
    XMFLOAT4 tint{ fade, fade, fade, 1 };

    {   // Start door on the wall behind the start cell.
        float yaw = HeadingYaw(m_startHeading);
        XMFLOAT3 fwd = ForwardOf(yaw), right = RightOf(yaw);
        XMFLOAT3 c = CellCenter(m_start, 0.5f);
        XMFLOAT3 center{ c.x - fwd.x * 0.495f, 0.48f, c.z - fwd.z * 0.495f };
        m_billboard.DrawOriented(ctx, m_doorTex, center, { right.x * 0.42f, 0, right.z * 0.42f }, { 0, 0.48f, 0 }, tint, true);
    }
    for (const auto& r : m_rats) {
        XMFLOAT3 a = CellCenter(r.cell, 0.14f), b = CellCenter(r.next, 0.14f);
        XMFLOAT3 p{ Lerp(a.x, b.x, r.progress), 0.14f, Lerp(a.z, b.z, r.progress) };
        XMFLOAT3 move{ static_cast<float>(kHeadingStep[r.heading].x), 0, static_cast<float>(kHeadingStep[r.heading].y) };
        XMFLOAT3 camRight;
        XMStoreFloat3(&camRight, m_camera.Right());
        bool flip = (move.x * camRight.x + move.z * camRight.z) < 0.0f;
        XMFLOAT4 uv = flip ? XMFLOAT4{ 1, 0, 0, 1 } : XMFLOAT4{ 0, 0, 1, 1 };
        m_billboard.Draw(ctx, m_ratTex, p, 0.42f, 0.28f, tint, true, uv);
    }
    {   // Smiley at the exit, slightly translucent.
        XMFLOAT3 c = CellCenter(m_exit, 0.5f);
        XMFLOAT4 col{ fade, fade, fade, 0.85f };
        m_billboard.Draw(ctx, m_smileyTex, c, 0.7f, 0.7f, col, false);
    }
}

void Maze3DSaver::Resize(int width, int height) {
    m_ctx.width = width;
    m_ctx.height = height;
    m_ctx.viewport = { 0, 0, width, height };
}
