#include "PostProcess.h"
#include "Shaders/Fullscreen_vs.h"
#include "Shaders/Copy_ps.h"
#include "Shaders/Blur_ps.h"
#include "Shaders/Fill_ps.h"

namespace rs {

void PostProcess::Create(Device& device) {
    ID3D11Device* d = device.Get();
    ThrowIfFailed(d->CreateVertexShader(g_Fullscreen_vs, sizeof(g_Fullscreen_vs), nullptr, &m_vs), "CreateVertexShader(Fullscreen)");
    ThrowIfFailed(d->CreatePixelShader(g_Copy_ps, sizeof(g_Copy_ps), nullptr, &m_copyPs), "CreatePixelShader(Copy)");
    ThrowIfFailed(d->CreatePixelShader(g_Blur_ps, sizeof(g_Blur_ps), nullptr, &m_blurPs), "CreatePixelShader(Blur)");
    ThrowIfFailed(d->CreatePixelShader(g_Fill_ps, sizeof(g_Fill_ps), nullptr, &m_fillPs), "CreatePixelShader(Fill)");
    m_blurCb.Create(device);
    m_fillCb.Create(device);
    m_states.Create(device);
}

void PostProcess::Draw(ID3D11DeviceContext* ctx, ID3D11PixelShader* ps) const {
    ctx->IASetInputLayout(nullptr);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ID3D11Buffer* none = nullptr;
    UINT zero = 0;
    ctx->IASetVertexBuffers(0, 1, &none, &zero, &zero);
    ctx->VSSetShader(m_vs.Get(), nullptr, 0);
    ctx->GSSetShader(nullptr, nullptr, 0);
    ctx->PSSetShader(ps, nullptr, 0);
    ctx->Draw(3, 0);
}

void PostProcess::Copy(ID3D11DeviceContext* ctx, ID3D11ShaderResourceView* src, ID3D11BlendState* blend) const {
    m_states.Set2D(ctx, blend ? blend : m_states.Opaque());
    ID3D11SamplerState* s = m_states.LinearClamp();
    ctx->PSSetSamplers(0, 1, &s);
    ctx->PSSetShaderResources(0, 1, &src);
    Draw(ctx, m_copyPs.Get());
    ID3D11ShaderResourceView* null = nullptr;
    ctx->PSSetShaderResources(0, 1, &null);
}

void PostProcess::Fill(ID3D11DeviceContext* ctx, const DirectX::XMFLOAT4& color, ID3D11BlendState* blend) {
    m_states.Set2D(ctx, blend ? blend : m_states.Opaque());
    FillCB cb{ color };
    m_fillCb.Update(ctx, cb);
    m_fillCb.BindPS(ctx, 0);
    Draw(ctx, m_fillPs.Get());
}

void PostProcess::GaussianBlur(Device& device, const RenderTexture& src, RenderTexture& tmp, RenderTexture& dst, int passes) {
    ID3D11DeviceContext* ctx = device.Ctx();
    m_states.Set2D(ctx, m_states.Opaque());
    ID3D11SamplerState* s = m_states.LinearClamp();
    ctx->PSSetSamplers(0, 1, &s);
    ID3D11ShaderResourceView* null = nullptr;

    const RenderTexture* in = &src;
    for (int p = 0; p < passes; ++p) {
        BlurCB h{ { 1.0f / tmp.Width(), 0, 0, 0 } };
        m_blurCb.Update(ctx, h);
        m_blurCb.BindPS(ctx, 0);
        tmp.Bind(ctx);
        in->BindPS(ctx, 0);
        Draw(ctx, m_blurPs.Get());
        ctx->PSSetShaderResources(0, 1, &null);

        BlurCB v{ { 0, 1.0f / tmp.Height(), 0, 0 } };
        m_blurCb.Update(ctx, v);
        dst.Bind(ctx);
        tmp.BindPS(ctx, 0);
        Draw(ctx, m_blurPs.Get());
        ctx->PSSetShaderResources(0, 1, &null);
        in = &dst;
    }
}

} // namespace rs
