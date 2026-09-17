#include "LineRenderer2D.h"
#include "Shaders/Line2D_vs.h"
#include "Shaders/Line2D_ps.h"

namespace rs {

void LineRenderer2D::Create(Device& device) {
    ID3D11Device* d = device.Get();
    ThrowIfFailed(d->CreateVertexShader(g_Line2D_vs, sizeof(g_Line2D_vs), nullptr, &m_vs), "CreateVertexShader(Line2D)");
    ThrowIfFailed(d->CreatePixelShader(g_Line2D_ps, sizeof(g_Line2D_ps), nullptr, &m_ps), "CreatePixelShader(Line2D)");
    UINT n = 0;
    const auto* layout = LayoutPC(n);
    ThrowIfFailed(d->CreateInputLayout(layout, n, g_Line2D_vs, sizeof(g_Line2D_vs), &m_layout), "CreateInputLayout(PC)");
    m_cb.Create(device);
    m_states.Create(device);
    m_vb.Reserve(device, 4096);
}

void LineRenderer2D::Begin(int viewportWidth, int viewportHeight) {
    m_width = viewportWidth > 0 ? viewportWidth : 1;
    m_height = viewportHeight > 0 ? viewportHeight : 1;
    m_verts.clear();
}

void LineRenderer2D::Line(float x0, float y0, float x1, float y1, const DirectX::XMFLOAT4& color) {
    m_verts.push_back({ { x0, y0, 0 }, color });
    m_verts.push_back({ { x1, y1, 0 }, color });
}

void LineRenderer2D::Polyline(const DirectX::XMFLOAT2* points, size_t count, bool closed, const DirectX::XMFLOAT4& color) {
    if (count < 2) return;
    for (size_t i = 0; i + 1 < count; ++i) Line(points[i].x, points[i].y, points[i + 1].x, points[i + 1].y, color);
    if (closed) Line(points[count - 1].x, points[count - 1].y, points[0].x, points[0].y, color);
}

void LineRenderer2D::End(Device& device, ID3D11BlendState* blend) {
    if (m_verts.empty()) return;
    ID3D11DeviceContext* ctx = device.Ctx();
    m_vb.Update(device, m_verts.data(), m_verts.size());
    ViewportCB cb{ { static_cast<float>(m_width), static_cast<float>(m_height), 0, 0 } };
    m_cb.Update(ctx, cb);

    m_states.Set2D(ctx, blend ? blend : m_states.Opaque());
    ctx->IASetInputLayout(m_layout.Get());
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    m_vb.Bind(ctx, 0);
    ctx->VSSetShader(m_vs.Get(), nullptr, 0);
    ctx->PSSetShader(m_ps.Get(), nullptr, 0);
    ctx->GSSetShader(nullptr, nullptr, 0);
    m_cb.BindVS(ctx, 0);
    ctx->Draw(static_cast<UINT>(m_verts.size()), 0);
    m_verts.clear();
}

} // namespace rs
