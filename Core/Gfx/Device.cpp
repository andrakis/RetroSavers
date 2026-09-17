#include "Device.h"
#include "Host/Log.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

namespace rs {

static HRESULT TryCreate(D3D_DRIVER_TYPE type, UINT flags, ComPtr<ID3D11Device>& dev, ComPtr<ID3D11DeviceContext>& ctx) {
    static const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };
    D3D_FEATURE_LEVEL got{};
    HRESULT hr = D3D11CreateDevice(nullptr, type, nullptr, flags, levels, ARRAYSIZE(levels), D3D11_SDK_VERSION, &dev, &got, &ctx);
    if (hr == E_INVALIDARG) // driver does not know 11_1: retry with 11_0 only
        hr = D3D11CreateDevice(nullptr, type, nullptr, flags, &levels[1], 1, D3D11_SDK_VERSION, &dev, &got, &ctx);
    return hr;
}

Device::Device() {
    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    ComPtr<ID3D11Device> dev;
    ComPtr<ID3D11DeviceContext> ctx;
    HRESULT hr = E_FAIL;

#ifdef _DEBUG
    hr = TryCreate(D3D_DRIVER_TYPE_HARDWARE, flags | D3D11_CREATE_DEVICE_DEBUG, dev, ctx);
    if (SUCCEEDED(hr)) m_debug = true;
#endif
    if (FAILED(hr)) hr = TryCreate(D3D_DRIVER_TYPE_HARDWARE, flags, dev, ctx);
    if (FAILED(hr)) {
        LogLine("Hardware D3D11 device unavailable; using WARP");
        hr = TryCreate(D3D_DRIVER_TYPE_WARP, flags, dev, ctx);
        m_warp = true;
    }
    ThrowIfFailed(hr, "D3D11CreateDevice");

    ThrowIfFailed(dev.As(&m_device), "QueryInterface ID3D11Device1");
    ThrowIfFailed(ctx.As(&m_context), "QueryInterface ID3D11DeviceContext1");

    ComPtr<IDXGIDevice> dxgiDevice;
    ThrowIfFailed(m_device.As(&dxgiDevice), "QueryInterface IDXGIDevice");
    ComPtr<IDXGIAdapter> adapter;
    ThrowIfFailed(dxgiDevice->GetAdapter(&adapter), "IDXGIDevice::GetAdapter");
    ThrowIfFailed(adapter->GetParent(IID_PPV_ARGS(&m_factory)), "IDXGIAdapter::GetParent IDXGIFactory2");
}

} // namespace rs
