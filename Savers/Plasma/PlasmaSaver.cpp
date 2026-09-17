#include "PlasmaSaver.h"
#include "Gfx/Device.h"
#include "Gfx/SwapChain.h"
#include "Util/MathUtil.h"
#include "Shaders/Plasma_ps.h"
#include <algorithm>

using namespace DirectX;
using namespace rs;

PlasmaSettings PlasmaSettings::Load(const Settings& s) {
    PlasmaSettings v;
    v.speed = Clamp(s.GetInt(L"Speed", v.speed), 1, 10);
    v.scale = Clamp(s.GetInt(L"Scale", v.scale), 1, 10);
    v.palette = Clamp(s.GetInt(L"Palette", v.palette), 0, 4);
    v.resolution = Clamp(s.GetInt(L"Resolution", v.resolution), 0, 2);
    return v;
}

void PlasmaSettings::Save(Settings& s) const {
    s.SetInt(L"Speed", speed);
    s.SetInt(L"Scale", scale);
    s.SetInt(L"Palette", palette);
    s.SetInt(L"Resolution", resolution);
}

void PlasmaSaver::Initialize(Device& device, const SaverContext& ctx) {
    m_ctx = ctx;
    m_settings = PlasmaSettings::Load(*ctx.settings);
    m_time = ctx.rng->Range(0.0f, 500.0f);
    m_post.Create(device);
    ThrowIfFailed(device.Get()->CreatePixelShader(g_Plasma_ps, sizeof(g_Plasma_ps), nullptr, &m_ps), "CreatePixelShader(Plasma)");
    m_cb.Create(device);
}

void PlasmaSaver::Update(float dt, double) {
    m_time += dt * (0.3f + 0.14f * m_settings.speed);
}

void PlasmaSaver::Render(Device& device, SwapChain& swap) {
    ID3D11DeviceContext* ctx = device.Ctx();
    const States& states = m_post.GetStates();

    // Cosine palettes (Inigo Quilez style): colour = a + b cos(2pi (c t + d)).
    PlasmaCB cb{};
    switch (m_settings.palette) {
    case PlasmaSettings::Fire:      cb.a = { 0.5f, 0.2f, 0.05f, 0 }; cb.b = { 0.5f, 0.35f, 0.2f, 0 }; cb.c = { 1, 1, 1, 0 }; cb.d = { 0.0f, 0.15f, 0.3f, 0 }; break;
    case PlasmaSettings::Ocean:     cb.a = { 0.1f, 0.4f, 0.6f, 0 }; cb.b = { 0.2f, 0.4f, 0.4f, 0 }; cb.c = { 1, 1, 1, 0 }; cb.d = { 0.5f, 0.35f, 0.2f, 0 }; break;
    case PlasmaSettings::Neon:      cb.a = { 0.5f, 0.5f, 0.5f, 0 }; cb.b = { 0.5f, 0.5f, 0.5f, 0 }; cb.c = { 2, 1, 0, 0 }; cb.d = { 0.5f, 0.2f, 0.25f, 0 }; break;
    case PlasmaSettings::Greyscale: cb.a = { 0.5f, 0.5f, 0.5f, 0 }; cb.b = { 0.5f, 0.5f, 0.5f, 0 }; cb.c = { 1, 1, 1, 0 }; cb.d = { 0, 0, 0, 0 }; break;
    default:                        cb.a = { 0.5f, 0.5f, 0.5f, 0 }; cb.b = { 0.5f, 0.5f, 0.5f, 0 }; cb.c = { 1, 1, 1, 0 }; cb.d = { 0.0f, 0.33f, 0.67f, 0 }; break;
    }
    int div = m_settings.resolution == PlasmaSettings::Full ? 1 : (m_settings.resolution == PlasmaSettings::Quarter ? 4 : 2);
    int w = std::max(m_ctx.width / div, 8), h = std::max(m_ctx.height / div, 8);
    cb.timeRes = { static_cast<float>(m_time), static_cast<float>(w), static_cast<float>(h), 1.5f + 0.5f * m_settings.scale };
    m_cb.Update(ctx, cb);
    m_cb.BindPS(ctx, 0);
    states.Set2D(ctx, states.Opaque());

    if (div == 1) {
        m_post.Draw(ctx, m_ps.Get());
        return;
    }
    m_target.Resize(device, w, h);
    m_target.Bind(ctx);
    m_post.Draw(ctx, m_ps.Get());
    swap.Bind();
    swap.SetViewport(m_ctx.viewport);
    m_post.Copy(ctx, m_target.SRV());
}

void PlasmaSaver::Resize(int width, int height) {
    m_ctx.width = width;
    m_ctx.height = height;
    m_ctx.viewport = { 0, 0, width, height };
}
