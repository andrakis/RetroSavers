#include "Billboard.h"
#include "Shaders/Billboard_vs.h"
#include "Shaders/Billboard_ps.h"

using namespace DirectX;

namespace rs {

void Billboard::Create(Device& device) {
    ID3D11Device* d = device.Get();
    ThrowIfFailed(d->CreateVertexShader(g_Billboard_vs, sizeof(g_Billboard_vs), nullptr, &m_vs), "CreateVertexShader(Billboard)");
    ThrowIfFailed(d->CreatePixelShader(g_Billboard_ps, sizeof(g_Billboard_ps), nullptr, &m_ps), "CreatePixelShader(Billboard)");
    m_cb.Create(device);
    m_states.Create(device);
}

void Billboard::BeginFrame(ID3D11DeviceContext*, const Camera& camera) {
    XMStoreFloat4x4(&m_viewProj, XMMatrixTranspose(camera.ViewProj()));
    XMStoreFloat3(&m_camRight, camera.Right());
    XMStoreFloat3(&m_camUp, camera.TrueUp());
}

void Billboard::Draw(ID3D11DeviceContext* ctx, const Texture& texture, const XMFLOAT3& center, float width, float height,
                     const XMFLOAT4& color, bool depthWrite, const XMFLOAT4& uvRect) {
    XMFLOAT3 right{ m_camRight.x * width * 0.5f, m_camRight.y * width * 0.5f, m_camRight.z * width * 0.5f };
    XMFLOAT3 up{ m_camUp.x * height * 0.5f, m_camUp.y * height * 0.5f, m_camUp.z * height * 0.5f };
    CB cb{};
    cb.viewProj = m_viewProj;
    cb.center = { center.x, center.y, center.z, 1 };
    cb.right = { right.x, right.y, right.z, 0 };
    cb.up = { up.x, up.y, up.z, 0 };
    cb.color = color;
    cb.uvRect = uvRect;
    m_cb.Update(ctx, cb);
    m_cb.BindVS(ctx, 0);

    ctx->RSSetState(m_states.CullNone());
    ctx->OMSetDepthStencilState(depthWrite ? m_states.DepthDefault() : m_states.DepthReadOnly(), 0);
    ctx->OMSetBlendState(m_states.AlphaBlend(), nullptr, 0xFFFFFFFF);
    ctx->IASetInputLayout(nullptr);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    ID3D11Buffer* none = nullptr;
    UINT zero = 0;
    ctx->IASetVertexBuffers(0, 1, &none, &zero, &zero);
    ctx->VSSetShader(m_vs.Get(), nullptr, 0);
    ctx->GSSetShader(nullptr, nullptr, 0);
    ctx->PSSetShader(m_ps.Get(), nullptr, 0);
    texture.BindPS(ctx, 0);
    ID3D11SamplerState* s = m_states.LinearClamp();
    ctx->PSSetSamplers(0, 1, &s);
    ctx->Draw(4, 0);
}

void Billboard::DrawOriented(ID3D11DeviceContext* ctx, const Texture& texture, const XMFLOAT3& center, const XMFLOAT3& right,
                             const XMFLOAT3& up, const XMFLOAT4& color, bool depthWrite) {
    XMFLOAT3 savedRight = m_camRight, savedUp = m_camUp;
    m_camRight = right;
    m_camUp = up;
    Draw(ctx, texture, center, 2.0f, 2.0f, color, depthWrite);
    m_camRight = savedRight;
    m_camUp = savedUp;
}

} // namespace rs
