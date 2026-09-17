#include "SwapChain.h"
#include <algorithm>

namespace rs {

SwapChain::SwapChain(Device& device, HWND hwnd, int width, int height)
    : m_device(device), m_width(std::max(width, 1)), m_height(std::max(height, 1)) {
    DXGI_SWAP_CHAIN_DESC1 desc{};
    desc.Width = m_width;
    desc.Height = m_height;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 2;
    desc.Scaling = DXGI_SCALING_STRETCH;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;

    HRESULT hr = device.Factory()->CreateSwapChainForHwnd(device.Get(), hwnd, &desc, nullptr, nullptr, &m_swapChain);
    if (FAILED(hr)) { // bitblt-model fallback (some child-window / remote-session cases)
        desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
        desc.BufferCount = 1;
        hr = device.Factory()->CreateSwapChainForHwnd(device.Get(), hwnd, &desc, nullptr, nullptr, &m_swapChain);
    }
    ThrowIfFailed(hr, "CreateSwapChainForHwnd");
    device.Factory()->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER | DXGI_MWA_NO_PRINT_SCREEN | DXGI_MWA_NO_WINDOW_CHANGES);
    CreateViews();
}

void SwapChain::CreateViews() {
    ComPtr<ID3D11Texture2D> back;
    ThrowIfFailed(m_swapChain->GetBuffer(0, IID_PPV_ARGS(&back)), "IDXGISwapChain::GetBuffer");
    ThrowIfFailed(m_device.Get()->CreateRenderTargetView(back.Get(), nullptr, &m_rtv), "CreateRenderTargetView(backbuffer)");

    D3D11_TEXTURE2D_DESC dd{};
    dd.Width = m_width;
    dd.Height = m_height;
    dd.MipLevels = 1;
    dd.ArraySize = 1;
    dd.Format = DXGI_FORMAT_D32_FLOAT;
    dd.SampleDesc.Count = 1;
    dd.Usage = D3D11_USAGE_DEFAULT;
    dd.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    ComPtr<ID3D11Texture2D> depth;
    ThrowIfFailed(m_device.Get()->CreateTexture2D(&dd, nullptr, &depth), "CreateTexture2D(depth)");
    ThrowIfFailed(m_device.Get()->CreateDepthStencilView(depth.Get(), nullptr, &m_dsv), "CreateDepthStencilView");
}

void SwapChain::Resize(int width, int height) {
    width = std::max(width, 1);
    height = std::max(height, 1);
    if (width == m_width && height == m_height) return;
    m_device.Ctx()->OMSetRenderTargets(0, nullptr, nullptr);
    m_rtv.Reset();
    m_dsv.Reset();
    m_device.Ctx()->Flush();
    ThrowIfFailed(m_swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0), "ResizeBuffers");
    m_width = width;
    m_height = height;
    CreateViews();
}

void SwapChain::Bind() {
    ID3D11RenderTargetView* rtv = m_rtv.Get();
    m_device.Ctx()->OMSetRenderTargets(1, &rtv, m_dsv.Get());
}

void SwapChain::Clear(const DirectX::XMFLOAT4& color) {
    m_device.Ctx()->ClearRenderTargetView(m_rtv.Get(), &color.x);
    ClearDepth();
}

void SwapChain::ClearRect(const DirectX::XMFLOAT4& color, const RECT& rect) {
    D3D11_RECT r{ rect.left, rect.top, rect.right, rect.bottom };
    m_device.Ctx()->ClearView(m_rtv.Get(), &color.x, &r, 1);
}

void SwapChain::ClearDepth() {
    m_device.Ctx()->ClearDepthStencilView(m_dsv.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
}

void SwapChain::SetViewport(const RECT& rect) {
    D3D11_VIEWPORT vp{};
    vp.TopLeftX = static_cast<float>(rect.left);
    vp.TopLeftY = static_cast<float>(rect.top);
    vp.Width = static_cast<float>(rect.right - rect.left);
    vp.Height = static_cast<float>(rect.bottom - rect.top);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    m_device.Ctx()->RSSetViewports(1, &vp);
    D3D11_RECT sc{ rect.left, rect.top, rect.right, rect.bottom };
    m_device.Ctx()->RSSetScissorRects(1, &sc);
}

HRESULT SwapChain::Present(UINT syncInterval) {
    return m_swapChain->Present(syncInterval, 0);
}

} // namespace rs
