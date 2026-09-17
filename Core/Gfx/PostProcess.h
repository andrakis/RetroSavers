#pragma once
#include "Device.h"
#include "ConstantBuffer.h"
#include "RenderTexture.h"
#include "States.h"

namespace rs {

// Fullscreen-triangle helpers. Savers bring their own pixel shaders for custom passes.
class PostProcess {
public:
    void Create(Device& device);

    // Binds the fullscreen VS, no input layout, and the given PS; draws 3 vertices into the
    // currently bound render target / viewport.
    void Draw(ID3D11DeviceContext* ctx, ID3D11PixelShader* ps) const;
    // Copies src into the current render target (current viewport) with the given blend.
    void Copy(ID3D11DeviceContext* ctx, ID3D11ShaderResourceView* src, ID3D11BlendState* blend = nullptr) const;
    // Separable Gaussian: src -> tmp (horizontal) -> dst (vertical). tmp and dst must match in size.
    void GaussianBlur(Device& device, const RenderTexture& src, RenderTexture& tmp, RenderTexture& dst, int passes = 1);

    ID3D11VertexShader* FullscreenVS() const { return m_vs.Get(); }
    const States& GetStates() const { return m_states; }

private:
    struct BlurCB { DirectX::XMFLOAT4 step; };

    ComPtr<ID3D11VertexShader> m_vs;
    ComPtr<ID3D11PixelShader> m_copyPs;
    ComPtr<ID3D11PixelShader> m_blurPs;
    ConstantBuffer<BlurCB> m_blurCb;
    States m_states;
};

} // namespace rs
