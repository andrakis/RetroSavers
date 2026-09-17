#include "TrailBuffer.h"
#include "Shaders/Feedback_ps.h"
#include <algorithm>

namespace rs {

void TrailBuffer::Create(Device& device, int width, int height, DXGI_FORMAT format) {
    if (!m_created) {
        ThrowIfFailed(device.Get()->CreatePixelShader(g_Feedback_ps, sizeof(g_Feedback_ps), nullptr, &m_feedbackPs), "CreatePixelShader(Feedback)");
        m_cb.Create(device);
        m_post.Create(device);
        m_created = true;
    }
    m_format = format;
    m_targets[0].Create(device, width, height, format);
    m_targets[1] = RenderTexture{};   // created on demand by the feedback path
    m_current = 0;
    Clear(device.Ctx());
}

void TrailBuffer::Resize(Device& device, int width, int height) {
    width = std::max(width, 1);
    height = std::max(height, 1);
    if (Valid() && width == Width() && height == Height()) return;
    Create(device, width, height, m_format);
}

void TrailBuffer::Clear(ID3D11DeviceContext* ctx, const DirectX::XMFLOAT4& color) {
    for (auto& t : m_targets)
        if (t.Valid()) t.Clear(ctx, color);
}

void TrailBuffer::Begin(Device& device, float fade, float zoom, float blur) {
    ID3D11DeviceContext* ctx = device.Ctx();
    const bool feedback = zoom != 1.0f || blur > 0.0f;
    if (!feedback) {
        m_targets[m_current].Bind(ctx);
        if (fade < 1.0f) m_post.Fill(ctx, { 0, 0, 0, 1.0f - std::max(fade, 0.0f) }, m_post.GetStates().AlphaBlend());
        return;
    }
    RenderTexture& src = m_targets[m_current];
    RenderTexture& dst = m_targets[m_current ^ 1];
    if (!dst.Valid() || dst.Width() != src.Width() || dst.Height() != src.Height())
        dst.Create(device, src.Width(), src.Height(), m_format);
    FeedbackCB cb{ { fade, zoom, blur / src.Width(), blur / src.Height() } };
    m_cb.Update(ctx, cb);
    m_cb.BindPS(ctx, 0);
    const States& states = m_post.GetStates();
    states.Set2D(ctx, states.Opaque());
    ID3D11SamplerState* s = states.LinearClamp();
    ctx->PSSetSamplers(0, 1, &s);
    dst.Bind(ctx);
    src.BindPS(ctx, 0);
    m_post.Draw(ctx, m_feedbackPs.Get());
    ID3D11ShaderResourceView* null = nullptr;
    ctx->PSSetShaderResources(0, 1, &null);
    m_current ^= 1;
    // dst stays bound for the caller's drawing.
}

void TrailBuffer::Present(Device& device, ID3D11BlendState* blend) {
    m_post.Copy(device.Ctx(), m_targets[m_current].SRV(), blend);
}

} // namespace rs
