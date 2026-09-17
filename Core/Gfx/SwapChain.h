#pragma once
#include "Device.h"
#include <DirectXMath.h>

namespace rs {

// Flip-model swap chain on an HWND with a matching depth buffer.
class SwapChain {
public:
    SwapChain(Device& device, HWND hwnd, int width, int height);

    void Resize(int width, int height);
    void Bind();                                   // OMSetRenderTargets(rtv, dsv)
    void Clear(const DirectX::XMFLOAT4& color);    // whole target + depth
    void ClearRect(const DirectX::XMFLOAT4& color, const RECT& rect);
    void ClearDepth();
    void SetViewport(const RECT& rect);            // viewport + scissor
    HRESULT Present(UINT syncInterval);

    ID3D11RenderTargetView* RTV() const { return m_rtv.Get(); }
    ID3D11DepthStencilView* DSV() const { return m_dsv.Get(); }
    int Width() const { return m_width; }
    int Height() const { return m_height; }

private:
    void CreateViews();

    Device& m_device;
    ComPtr<IDXGISwapChain1> m_swapChain;
    ComPtr<ID3D11RenderTargetView> m_rtv;
    ComPtr<ID3D11DepthStencilView> m_dsv;
    int m_width = 0;
    int m_height = 0;
};

} // namespace rs
