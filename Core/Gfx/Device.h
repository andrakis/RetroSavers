#pragma once
#include <d3d11_1.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <stdexcept>
#include <string>

namespace rs {

using Microsoft::WRL::ComPtr;

// Throws on failed HRESULTs so the render thread can log and close.
inline void ThrowIfFailed(HRESULT hr, const char* what) {
    if (FAILED(hr)) {
        char buf[256];
        sprintf_s(buf, "%s failed (hr=0x%08lX)", what, static_cast<unsigned long>(hr));
        throw std::runtime_error(buf);
    }
}

class Device {
public:
    Device();  // hardware FL 11.0+; falls back to WARP (RDP, no GPU)

    ID3D11Device1* Get() const { return m_device.Get(); }
    ID3D11DeviceContext1* Ctx() const { return m_context.Get(); }
    IDXGIFactory2* Factory() const { return m_factory.Get(); }
    bool IsWarp() const { return m_warp; }
    bool DebugLayer() const { return m_debug; }

private:
    ComPtr<ID3D11Device1> m_device;
    ComPtr<ID3D11DeviceContext1> m_context;
    ComPtr<IDXGIFactory2> m_factory;
    bool m_warp = false;
    bool m_debug = false;
};

} // namespace rs
