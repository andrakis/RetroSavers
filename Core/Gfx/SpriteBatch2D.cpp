#include "SpriteBatch2D.h"
#include "Shaders/Sprite2D_vs.h"
#include "Shaders/Sprite2D_ps.h"

namespace rs {

void SpriteBatch2D::Create(Device& device) {
    ID3D11Device* d = device.Get();
    ThrowIfFailed(d->CreateVertexShader(g_Sprite2D_vs, sizeof(g_Sprite2D_vs), nullptr, &m_vs), "CreateVertexShader(Sprite2D)");
    ThrowIfFailed(d->CreatePixelShader(g_Sprite2D_ps, sizeof(g_Sprite2D_ps), nullptr, &m_ps), "CreatePixelShader(Sprite2D)");
    const D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "TEXCOORD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "TEXCOORD", 2, DXGI_FORMAT_R32_FLOAT, 0, 48, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
    };
    static_assert(sizeof(Sprite) == 52, "Sprite layout must match the input layout offsets");
    ThrowIfFailed(d->CreateInputLayout(layout, 5, g_Sprite2D_vs, sizeof(g_Sprite2D_vs), &m_layout), "CreateInputLayout(Sprite2D)");
    m_cb.Create(device);
    m_states.Create(device);
    m_vb.Reserve(device, 1024);
    uint32_t white = 0xFFFFFFFFu;
    m_white.FromRgba(device, 1, 1, &white, false);
}

void SpriteBatch2D::Begin(int viewportWidth, int viewportHeight) {
    m_width = viewportWidth > 0 ? viewportWidth : 1;
    m_height = viewportHeight > 0 ? viewportHeight : 1;
    m_sprites.clear();
}

void SpriteBatch2D::End(Device& device, const Texture* texture, ID3D11BlendState* blend, ID3D11PixelShader* psOverride, ID3D11SamplerState* sampler) {
    const Texture* tex = (texture && texture->Valid()) ? texture : &m_white;
    End(device, tex->SRV(), blend, psOverride, sampler);
}

void SpriteBatch2D::End(Device& device, ID3D11ShaderResourceView* srv, ID3D11BlendState* blend, ID3D11PixelShader* psOverride, ID3D11SamplerState* sampler) {
    if (m_sprites.empty()) return;
    ID3D11DeviceContext* ctx = device.Ctx();
    m_vb.Update(device, m_sprites.data(), m_sprites.size());
    ViewportCB cb{ { static_cast<float>(m_width), static_cast<float>(m_height), 0, 0 } };
    m_cb.Update(ctx, cb);

    m_states.Set2D(ctx, blend ? blend : m_states.AlphaBlend());
    ctx->IASetInputLayout(m_layout.Get());
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_vb.Bind(ctx, 0);
    ctx->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
    ctx->VSSetShader(m_vs.Get(), nullptr, 0);
    ctx->PSSetShader(psOverride ? psOverride : m_ps.Get(), nullptr, 0);
    ctx->GSSetShader(nullptr, nullptr, 0);
    m_cb.BindVS(ctx, 0);
    ctx->PSSetShaderResources(0, 1, &srv);
    ID3D11SamplerState* s = sampler ? sampler : m_states.LinearClamp();
    ctx->PSSetSamplers(0, 1, &s);
    ctx->DrawInstanced(6, static_cast<UINT>(m_sprites.size()), 0, 0);
    ID3D11ShaderResourceView* null = nullptr;
    ctx->PSSetShaderResources(0, 1, &null);
    m_sprites.clear();
}

} // namespace rs
