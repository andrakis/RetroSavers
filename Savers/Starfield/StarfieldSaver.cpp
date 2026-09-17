#include "StarfieldSaver.h"
#include "Gfx/Device.h"
#include "Gfx/SwapChain.h"
#include "Util/MathUtil.h"
#include "Shaders/Star_vs.h"
#include "Shaders/Star_ps.h"

using namespace DirectX;
using namespace rs;

StarfieldSettings StarfieldSettings::Load(const Settings& s) {
    StarfieldSettings v;
    v.starCount = Clamp(s.GetInt(L"StarCount", 50), 10, 200);
    v.warpSpeed = Clamp(s.GetInt(L"WarpSpeed", 5), 1, 10);
    return v;
}

void StarfieldSettings::Save(Settings& s) const {
    s.SetInt(L"StarCount", starCount);
    s.SetInt(L"WarpSpeed", warpSpeed);
}

void StarfieldSaver::Initialize(Device& device, const SaverContext& ctx) {
    m_ctx = ctx;
    m_settings = StarfieldSettings::Load(*ctx.settings);

    ID3D11Device* d = device.Get();
    ThrowIfFailed(d->CreateVertexShader(g_Star_vs, sizeof(g_Star_vs), nullptr, &m_vs), "CreateVertexShader(Star)");
    ThrowIfFailed(d->CreatePixelShader(g_Star_ps, sizeof(g_Star_ps), nullptr, &m_ps), "CreatePixelShader(Star)");
    const D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32_FLOAT, 0, 8, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "TEXCOORD", 1, DXGI_FORMAT_R32_FLOAT, 0, 12, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
    };
    ThrowIfFailed(d->CreateInputLayout(layout, 3, g_Star_vs, sizeof(g_Star_vs), &m_layout), "CreateInputLayout(Star)");
    m_cb.Create(device);
    m_states.Create(device);

    m_stars.resize(m_settings.starCount);
    for (auto& s : m_stars) Respawn(s, true);
    m_vb.Reserve(device, m_stars.size());
}

void StarfieldSaver::Respawn(Star& s, bool anywhere) {
    Rng& rng = *m_ctx.rng;
    s.x = rng.Range(-1.0f, 1.0f);
    s.y = rng.Range(-1.0f, 1.0f);
    s.z = anywhere ? rng.Range(0.05f, 1.0f) : 1.0f;
}

void StarfieldSaver::Update(float dt, double) {
    // Speed 1..10 maps to a z-velocity so that speed 5 crosses the field in ~2.5 s.
    float speed = 0.08f * m_settings.warpSpeed;
    float cx = m_ctx.width * 0.5f, cy = m_ctx.height * 0.5f;

    m_instances.clear();
    m_instances.reserve(m_stars.size());
    for (auto& s : m_stars) {
        s.z -= speed * dt;
        if (s.z <= 0.02f) { Respawn(s, false); continue; }
        float sx = cx + (s.x / s.z) * cx;   // both axes scale by cx so pixels stay square
        float sy = cy + (s.y / s.z) * cx;
        if (sx < -4 || sy < -4 || sx > m_ctx.width + 4 || sy > m_ctx.height + 4) { Respawn(s, false); continue; }
        float depth = 1.0f - s.z;                              // 0 = far, 1 = near
        float size = (1.0f + 3.0f * depth * depth) * m_ctx.dpiScale;
        float bright = 0.35f + 0.65f * depth;
        m_instances.push_back({ { std::floor(sx), std::floor(sy) }, std::floor(size + 0.5f), bright });
    }
}

void StarfieldSaver::Render(Device& device, SwapChain&) {
    if (m_instances.empty()) return;
    ID3D11DeviceContext* ctx = device.Ctx();
    m_vb.Update(device, m_instances.data(), m_instances.size());
    ViewportCB cb{ { static_cast<float>(m_ctx.width), static_cast<float>(m_ctx.height), 0, 0 } };
    m_cb.Update(ctx, cb);

    m_states.Set2D(ctx, m_states.Opaque());
    ctx->IASetInputLayout(m_layout.Get());
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_vb.Bind(ctx, 0);
    ctx->VSSetShader(m_vs.Get(), nullptr, 0);
    ctx->PSSetShader(m_ps.Get(), nullptr, 0);
    ctx->GSSetShader(nullptr, nullptr, 0);
    m_cb.BindVS(ctx, 0);
    ctx->DrawInstanced(6, static_cast<UINT>(m_instances.size()), 0, 0);
}

void StarfieldSaver::Resize(int width, int height) {
    m_ctx.width = width;
    m_ctx.height = height;
}
