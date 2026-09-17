#include "AuroraSaver.h"
#include "Gfx/Device.h"
#include "Gfx/SwapChain.h"
#include "Util/MathUtil.h"
#include "Shaders/Aurora_ps.h"
#include "Shaders/AuroraComposite_ps.h"

using namespace DirectX;
using namespace rs;

AuroraSettings AuroraSettings::Load(const Settings& s) {
    AuroraSettings v;
    v.speed = Clamp(s.GetInt(L"Speed", 5), 1, 10);
    v.brightness = Clamp(s.GetInt(L"Brightness", 5), 1, 10);
    v.amplitude = Clamp(s.GetInt(L"Amplitude", 5), 1, 10);
    v.layers = Clamp(s.GetInt(L"Layers", 6), 1, 10);
    return v;
}

void AuroraSettings::Save(Settings& s) const {
    s.SetInt(L"Speed", speed);
    s.SetInt(L"Brightness", brightness);
    s.SetInt(L"Amplitude", amplitude);
    s.SetInt(L"Layers", layers);
}

void AuroraSaver::Initialize(Device& device, const SaverContext& ctx) {
    m_ctx = ctx;
    m_settings = AuroraSettings::Load(*ctx.settings);
    m_seed = ctx.rng->Range(0.0f, 100.0f);
    m_post.Create(device);
    ThrowIfFailed(device.Get()->CreatePixelShader(g_Aurora_ps, sizeof(g_Aurora_ps), nullptr, &m_auroraPs), "CreatePixelShader(Aurora)");
    ThrowIfFailed(device.Get()->CreatePixelShader(g_AuroraComposite_ps, sizeof(g_AuroraComposite_ps), nullptr, &m_compositePs), "CreatePixelShader(AuroraComposite)");
    m_auroraCb.Create(device);
    m_compositeCb.Create(device);
    CreateTargets(device);
}

void AuroraSaver::CreateTargets(Device& device) {
    int w = std::max(m_ctx.width / 2, 8), h = std::max(m_ctx.height / 2, 8);
    m_base.Resize(device, w, h);
    int bw = std::max(w / 2, 4), bh = std::max(h / 2, 4);
    m_blurTmp.Resize(device, bw, bh);
    m_bloom.Resize(device, bw, bh);
}

void AuroraSaver::Update(float dt, double) {
    m_time += dt;
}

void AuroraSaver::Render(Device& device, SwapChain& swap) {
    ID3D11DeviceContext* ctx = device.Ctx();
    const States& states = m_post.GetStates();
    if (!m_base.Valid()) CreateTargets(device);

    // 1. Aurora at half resolution.
    AuroraCB a{};
    a.timeRes = { static_cast<float>(m_time), static_cast<float>(m_base.Width()), static_cast<float>(m_base.Height()), 0.3f + 0.17f * m_settings.amplitude };
    a.params = { 0.3f + 0.17f * m_settings.speed, 0.35f + 0.16f * m_settings.brightness, static_cast<float>(m_settings.layers), m_seed };
    m_auroraCb.Update(ctx, a);
    m_auroraCb.BindPS(ctx, 0);
    states.Set2D(ctx, states.Opaque());
    m_base.Bind(ctx);
    m_post.Draw(ctx, m_auroraPs.Get());

    // 2. Bloom: downsample + separable blur (two passes for a wide, soft glow).
    m_bloom.Bind(ctx);
    m_post.Copy(ctx, m_base.SRV());
    m_post.GaussianBlur(device, m_bloom, m_blurTmp, m_bloom, 2);

    // 3. Composite into this saver's viewport.
    swap.Bind();
    swap.SetViewport(m_ctx.viewport);
    CompositeCB c{ { 0.9f, 0.55f, 1.15f, static_cast<float>(std::fmod(m_time, 1000.0)) } };
    m_compositeCb.Update(ctx, c);
    m_compositeCb.BindPS(ctx, 0);
    states.Set2D(ctx, states.Opaque());
    ID3D11ShaderResourceView* srvs[2] = { m_base.SRV(), m_bloom.SRV() };
    ctx->PSSetShaderResources(0, 2, srvs);
    ID3D11SamplerState* s = states.LinearClamp();
    ctx->PSSetSamplers(0, 1, &s);
    m_post.Draw(ctx, m_compositePs.Get());
    ID3D11ShaderResourceView* none[2] = { nullptr, nullptr };
    ctx->PSSetShaderResources(0, 2, none);
}

void AuroraSaver::Resize(int width, int height) {
    m_ctx.width = width;
    m_ctx.height = height;
    m_ctx.viewport = { 0, 0, width, height };
    m_base = RenderTexture{};   // recreated lazily below
    m_blurTmp = RenderTexture{};
    m_bloom = RenderTexture{};
}
