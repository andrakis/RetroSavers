#include "Forward.h"
#include "Shaders/Phong_vs.h"
#include "Shaders/PhongInstanced_vs.h"
#include "Shaders/Phong_ps.h"

using namespace DirectX;

namespace rs {

void Forward::Create(Device& device) {
    ID3D11Device* d = device.Get();
    ThrowIfFailed(d->CreateVertexShader(g_Phong_vs, sizeof(g_Phong_vs), nullptr, &m_vs), "CreateVertexShader(Phong)");
    ThrowIfFailed(d->CreateVertexShader(g_PhongInstanced_vs, sizeof(g_PhongInstanced_vs), nullptr, &m_vsInstanced), "CreateVertexShader(PhongInstanced)");
    ThrowIfFailed(d->CreatePixelShader(g_Phong_ps, sizeof(g_Phong_ps), nullptr, &m_ps), "CreatePixelShader(Phong)");
    UINT n = 0;
    const auto* l = LayoutPNT(n);
    ThrowIfFailed(d->CreateInputLayout(l, n, g_Phong_vs, sizeof(g_Phong_vs), &m_layout), "CreateInputLayout(PNT)");
    const auto* li = LayoutPNTInstanced(n);
    ThrowIfFailed(d->CreateInputLayout(li, n, g_PhongInstanced_vs, sizeof(g_PhongInstanced_vs), &m_layoutInstanced), "CreateInputLayout(PNT instanced)");
    m_frameCb.Create(device);
    m_objectCb.Create(device);
    m_states.Create(device);
    uint32_t white = 0xFFFFFFFFu;
    m_white.FromRgba(device, 1, 1, &white, false);
}

void Forward::BeginFrame(ID3D11DeviceContext* ctx, const Camera& camera, const XMFLOAT3& lightDir, const XMFLOAT3& lightColor, const XMFLOAT3& ambient) {
    FrameConstants f{};
    XMStoreFloat4x4(&f.viewProj, XMMatrixTranspose(camera.ViewProj()));
    f.eyePos = { camera.eye.x, camera.eye.y, camera.eye.z, 1 };
    XMVECTOR ld = XMVector3Normalize(XMLoadFloat3(&lightDir));
    XMStoreFloat4(&f.lightDir, ld);
    f.lightColor = { lightColor.x, lightColor.y, lightColor.z, 1 };
    f.ambient = { ambient.x, ambient.y, ambient.z, 1 };
    f.fogColor = { 0, 0, 0, 0 };
    f.fogParams = { 0, 1, 0, 0 };
    BeginFrame(ctx, f);
}

void Forward::BeginFrame(ID3D11DeviceContext* ctx, const FrameConstants& frame) {
    m_frameCb.Update(ctx, frame);
    m_frameCb.BindVS(ctx, 0);
    m_frameCb.BindPS(ctx, 0);
    m_objectCb.BindVS(ctx, 1);
    m_objectCb.BindPS(ctx, 1);
    ctx->PSSetShader(m_ps.Get(), nullptr, 0);
    ctx->GSSetShader(nullptr, nullptr, 0);
}

void Forward::BindMaterial(ID3D11DeviceContext* ctx, const Material& m, const XMMATRIX& world) {
    ObjectConstants o{};
    XMStoreFloat4x4(&o.world, XMMatrixTranspose(world));
    o.color = m.color;
    o.material = { m.specPower, m.specIntensity, m.texture ? 1.0f : 0.0f, m.uvScale };
    o.lightMap = { m.lightMap ? 1.0f : 0.0f, m.lightMapScale, m.lightMapStrength, 0.0f };
    m_objectCb.Update(ctx, o);
    const Texture* tex = m.texture ? m.texture : &m_white;
    tex->BindPS(ctx, 0);
    ID3D11SamplerState* s = m.sampler ? m.sampler : m_states.AnisoWrap();
    ctx->PSSetSamplers(0, 1, &s);
    ID3D11ShaderResourceView* lm = m.lightMap ? m.lightMap : m_white.SRV();
    ctx->PSSetShaderResources(1, 1, &lm);
    ID3D11SamplerState* ls = m_states.LinearWrap();
    ctx->PSSetSamplers(1, 1, &ls);
}

void Forward::Draw(ID3D11DeviceContext* ctx, const Mesh& mesh, const XMMATRIX& world, const Material& material) {
    Draw(ctx, mesh, world, material, nullptr, nullptr, nullptr);
}

void Forward::Draw(ID3D11DeviceContext* ctx, const Mesh& mesh, const XMMATRIX& world, const Material& material,
                   ID3D11VertexShader* vs, ID3D11InputLayout* layout, ID3D11PixelShader* ps) {
    if (!mesh.Valid()) return;
    BindMaterial(ctx, material, world);
    ctx->IASetInputLayout(layout ? layout : m_layout.Get());
    ctx->VSSetShader(vs ? vs : m_vs.Get(), nullptr, 0);
    ctx->PSSetShader(ps ? ps : m_ps.Get(), nullptr, 0);
    mesh.Draw(ctx);
}

void Forward::DrawInstanced(ID3D11DeviceContext* ctx, const Mesh& mesh, const DynamicVertexBuffer<InstanceData>& instances, const Material& material) {
    if (!mesh.Valid() || instances.Count() == 0) return;
    BindMaterial(ctx, material, XMMatrixIdentity());
    ctx->IASetInputLayout(m_layoutInstanced.Get());
    ctx->VSSetShader(m_vsInstanced.Get(), nullptr, 0);
    ctx->PSSetShader(m_ps.Get(), nullptr, 0);
    instances.Bind(ctx, 1);
    mesh.DrawInstanced(ctx, static_cast<UINT>(instances.Count()));
}

} // namespace rs
