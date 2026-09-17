#pragma once
#include "Device.h"
#include <DirectXMath.h>

namespace rs {

// Offscreen colour target with an SRV. No depth buffer.
class RenderTexture {
public:
    void Create(Device& device, int width, int height, DXGI_FORMAT format = DXGI_FORMAT_R16G16B16A16_FLOAT);
    void Resize(Device& device, int width, int height);  // no-op if unchanged
    void Bind(ID3D11DeviceContext* ctx, bool setViewport = true) const;
    void Clear(ID3D11DeviceContext* ctx, const DirectX::XMFLOAT4& color) const;
    void BindPS(ID3D11DeviceContext* ctx, UINT slot) const { ID3D11ShaderResourceView* s = m_srv.Get(); ctx->PSSetShaderResources(slot, 1, &s); }

    ID3D11RenderTargetView* RTV() const { return m_rtv.Get(); }
    ID3D11ShaderResourceView* SRV() const { return m_srv.Get(); }
    int Width() const { return m_width; }
    int Height() const { return m_height; }
    bool Valid() const { return m_rtv != nullptr; }

private:
    ComPtr<ID3D11Texture2D> m_texture;
    ComPtr<ID3D11RenderTargetView> m_rtv;
    ComPtr<ID3D11ShaderResourceView> m_srv;
    DXGI_FORMAT m_format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    int m_width = 0;
    int m_height = 0;
};

} // namespace rs
