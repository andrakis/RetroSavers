#pragma once
#include "Device.h"
#include "ConstantBuffer.h"
#include "PostProcess.h"
#include "RenderTexture.h"

namespace rs {

// A persistent offscreen target that decays a little every frame: particles, streaks and
// afterglow accumulate into it and the saver copies it to the screen. fp16 by default so a
// geometric fade really reaches black (8-bit targets stall a few steps above it).
class TrailBuffer {
public:
    void Create(Device& device, int width, int height, DXGI_FORMAT format = DXGI_FORMAT_R16G16B16A16_FLOAT);
    void Resize(Device& device, int width, int height);   // recreates (and clears) only if the size changed
    void Clear(ID3D11DeviceContext* ctx, const DirectX::XMFLOAT4& color = { 0, 0, 0, 0 });

    // Applies this frame's decay and leaves the trail target bound (its own viewport + scissor)
    // so the caller can draw into it. fade multiplies the previous contents (1 = keep forever).
    // zoom == 1 and blur == 0 fade in place; otherwise the previous frame is resampled through the
    // feedback shader (zoom about the centre, blur in texels) into the other target.
    void Begin(Device& device, float fade, float zoom = 1.0f, float blur = 0.0f);
    // Copies the trail into whatever target/viewport is currently bound (rebind the swap chain first).
    void Present(Device& device, ID3D11BlendState* blend = nullptr);

    const RenderTexture& Current() const { return m_targets[m_current]; }
    PostProcess& Post() { return m_post; }
    const States& GetStates() const { return m_post.GetStates(); }
    int Width() const { return m_targets[m_current].Width(); }
    int Height() const { return m_targets[m_current].Height(); }
    bool Valid() const { return m_targets[m_current].Valid(); }

private:
    struct FeedbackCB { DirectX::XMFLOAT4 params; };

    RenderTexture m_targets[2];
    int m_current = 0;
    DXGI_FORMAT m_format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    ComPtr<ID3D11PixelShader> m_feedbackPs;
    ConstantBuffer<FeedbackCB> m_cb;
    PostProcess m_post;
    bool m_created = false;
};

} // namespace rs
