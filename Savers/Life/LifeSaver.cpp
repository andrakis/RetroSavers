#include "LifeSaver.h"
#include "Gfx/Device.h"
#include "Gfx/SwapChain.h"
#include "Util/Color.h"
#include "Util/MathUtil.h"
#include "Shaders/LifeStep_ps.h"
#include "Shaders/LifeShow_ps.h"
#include <algorithm>

using namespace DirectX;
using namespace rs;

LifeSettings LifeSettings::Load(const Settings& s) {
    LifeSettings v;
    v.cellSize = Clamp(s.GetInt(L"CellSize", v.cellSize), 2, 16);
    v.tickRate = Clamp(s.GetInt(L"TickRate", v.tickRate), 1, 30);
    v.density = Clamp(s.GetInt(L"Density", v.density), 5, 60);
    v.palette = Clamp(s.GetInt(L"Palette", v.palette), 0, 3);
    v.wrap = s.GetBool(L"Wrap", v.wrap);
    v.reseed = s.GetBool(L"Reseed", v.reseed);
    return v;
}

void LifeSettings::Save(Settings& s) const {
    s.SetInt(L"CellSize", cellSize);
    s.SetInt(L"TickRate", tickRate);
    s.SetInt(L"Density", density);
    s.SetInt(L"Palette", palette);
    s.SetBool(L"Wrap", wrap);
    s.SetBool(L"Reseed", reseed);
}

void LifeSaver::Initialize(Device& device, const SaverContext& ctx) {
    m_ctx = ctx;
    m_settings = LifeSettings::Load(*ctx.settings);
    m_post.Create(device);
    ThrowIfFailed(device.Get()->CreatePixelShader(g_LifeStep_ps, sizeof(g_LifeStep_ps), nullptr, &m_stepPs), "CreatePixelShader(LifeStep)");
    ThrowIfFailed(device.Get()->CreatePixelShader(g_LifeShow_ps, sizeof(g_LifeShow_ps), nullptr, &m_showPs), "CreatePixelShader(LifeShow)");
    m_stepCb.Create(device);
    m_showCb.Create(device);
    CreateWorld(device);
}

void LifeSaver::CreateWorld(Device& device) {
    int cell = static_cast<int>(m_settings.cellSize * m_ctx.dpiScale + 0.5f);
    m_cols = std::max(8, m_ctx.width / std::max(cell, 1));
    m_rows = std::max(8, m_ctx.height / std::max(cell, 1));
    for (auto& s : m_state) s.Create(device, m_cols, m_rows, DXGI_FORMAT_R8G8B8A8_UNORM);
    m_current = 0;
    Seed(device);
}

void LifeSaver::Seed(Device& device) {
    Rng& rng = *m_ctx.rng;
    Image img(m_cols, m_rows);
    float p = m_settings.density / 100.0f;
    for (auto& px : img.pixels) px = rng.Chance(p) ? PackRgba(255, 1, 0) : PackRgba(0, 0, 0);   // r = alive, g = age
    m_state[m_current].Upload(device.Ctx(), img);
    m_hashCount = 0;
    m_sinceSeed = 0.0f;
    m_checkTimer = 0.0f;
}

void LifeSaver::Step(Device& device) {
    ID3D11DeviceContext* ctx = device.Ctx();
    const States& states = m_post.GetStates();
    int next = m_current ^ 1;
    StepCB cb{ { 1.0f / m_cols, 1.0f / m_rows, 0, 0 } };
    m_stepCb.Update(ctx, cb);
    m_stepCb.BindPS(ctx, 0);
    states.Set2D(ctx, states.Opaque());
    ID3D11SamplerState* s = m_settings.wrap ? states.PointWrap() : states.PointClamp();
    ctx->PSSetSamplers(0, 1, &s);
    m_state[next].Bind(ctx);
    m_state[m_current].BindPS(ctx, 0);
    m_post.Draw(ctx, m_stepPs.Get());
    ID3D11ShaderResourceView* null = nullptr;
    ctx->PSSetShaderResources(0, 1, &null);
    m_current = next;
}

void LifeSaver::CheckStagnation(Device& device) {
    // Hash the alive bits; a hash seen in the last few checks means a still life or a short
    // oscillator is all that is left. Checks are ~2 s apart so gliders keep changing the hash.
    Image img = m_state[m_current].Readback(device);
    uint64_t h = 1469598103934665603ull;
    int alive = 0;
    for (uint32_t px : img.pixels) {
        bool a = (px & 0xFF) > 127;
        alive += a;
        h = (h ^ (a ? 0x9Eu : 0x31u)) * 1099511628211ull;
    }
    bool stagnant = alive < static_cast<int>(img.pixels.size() * 0.004f);
    for (int i = 0; i < m_hashCount && !stagnant; ++i)
        if (m_hashes[i] == h) stagnant = true;
    if (stagnant && m_sinceSeed > 8.0f) { Seed(device); return; }
    int n = static_cast<int>(std::size(m_hashes));
    if (m_hashCount < n) m_hashes[m_hashCount++] = h;
    else { for (int i = 1; i < n; ++i) m_hashes[i - 1] = m_hashes[i]; m_hashes[n - 1] = h; }
}

void LifeSaver::Update(float dt, double) {
    m_accumulator += dt;
    m_sinceSeed += dt;
    m_checkTimer += dt;
    const float step = 1.0f / m_settings.tickRate;
    m_pendingSteps = 0;
    while (m_accumulator >= step && m_pendingSteps < 8) { m_accumulator -= step; ++m_pendingSteps; }
    if (m_accumulator > step) m_accumulator = 0.0f;
}

void LifeSaver::Render(Device& device, SwapChain& swap) {
    ID3D11DeviceContext* ctx = device.Ctx();
    const States& states = m_post.GetStates();
    if (!m_state[0].Valid()) CreateWorld(device);
    for (int i = 0; i < m_pendingSteps; ++i) Step(device);
    m_pendingSteps = 0;
    if (m_settings.reseed && m_checkTimer >= 2.0f) { m_checkTimer = 0.0f; CheckStagnation(device); }

    swap.Bind();
    swap.SetViewport(m_ctx.viewport);
    float cellPx = static_cast<float>(m_ctx.width) / m_cols;
    ShowCB cb{ { static_cast<float>(m_settings.palette), static_cast<float>(m_cols), static_cast<float>(m_rows), cellPx >= 5.0f ? 0.6f : 0.0f } };
    m_showCb.Update(ctx, cb);
    m_showCb.BindPS(ctx, 0);
    states.Set2D(ctx, states.Opaque());
    ID3D11SamplerState* s = states.PointClamp();
    ctx->PSSetSamplers(0, 1, &s);
    m_state[m_current].BindPS(ctx, 0);
    m_post.Draw(ctx, m_showPs.Get());
    ID3D11ShaderResourceView* null = nullptr;
    ctx->PSSetShaderResources(0, 1, &null);
}

void LifeSaver::Resize(int width, int height) {
    m_ctx.width = width;
    m_ctx.height = height;
    m_ctx.viewport = { 0, 0, width, height };
    for (auto& s : m_state) s = RenderTexture{};   // rebuilt lazily in Render
}
