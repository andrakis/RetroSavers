#pragma once
#include <DirectXMath.h>
#include <windows.h>
#include <optional>
#include "Host/Settings.h"
#include "Util/Rng.h"

namespace rs {

class Device;
class SwapChain;

// Everything a saver instance needs to know about the surface it draws on.
// One Saver instance exists per monitor; all share the device and swap chain.
struct SaverContext {
    bool previewMode = false;   // true when hosted inside the Windows preview (or PreviewHost)
    RECT viewport{};            // this saver's rect inside the swap chain, in pixels
    int width = 1;              // viewport width in pixels
    int height = 1;             // viewport height in pixels
    float dpiScale = 1.0f;      // 1.0 = 96 dpi
    int monitorIndex = 0;
    int monitorCount = 1;
    Settings* settings = nullptr;
    Rng* rng = nullptr;
};

class Saver {
public:
    virtual ~Saver() = default;

    // Called once on the render thread with the device ready. Load settings and create resources here.
    virtual void Initialize(Device&, const SaverContext&) {}
    // dt in seconds (clamped to 100 ms), time in seconds since the saver started.
    virtual void Update(float /*dt*/, double /*time*/) {}
    // Viewport and scissor are already set to this saver's rect; the render target is bound.
    virtual void Render(Device&, SwapChain&) = 0;
    // The rect changed (preview / dev host resize only).
    virtual void Resize(int /*width*/, int /*height*/) {}
    // Colour the host clears this saver's rect to before Render; nullopt = do not clear.
    virtual std::optional<DirectX::XMFLOAT4> ClearColor() const { return DirectX::XMFLOAT4{ 0, 0, 0, 1 }; }
};

} // namespace rs
