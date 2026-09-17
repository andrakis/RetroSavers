#include "Host.h"
#include "Log.h"
#include "Monitors.h"
#include "Settings.h"
#include "DesktopCapture.h"
#include "Gfx/Device.h"
#include "Gfx/SwapChain.h"
#include "Util/Rng.h"
#include <scrnsave.h>
#include <commctrl.h>
#include <atomic>
#include <optional>
#include <thread>
#include <vector>
#include <string>
#include <cwchar>

#pragma comment(lib, "comctl32.lib")
// Common Controls v6 so the config dialogs get themed trackbars/buttons.
#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

namespace rs {

namespace {

struct HostState {
    HWND hwnd = nullptr;
    HWND parent = nullptr;     // preview mode: the window that hosts us
    DWORD mainThread = 0;
    bool preview = false;
    std::wstring name;
    SaverFactory factory;
    std::vector<MonitorInfo> monitors;
    std::optional<Image> desktop;

    std::thread thread;
    std::atomic<bool> stop{ false };
    std::atomic<bool> resizePending{ false };
    std::atomic<int> pendingWidth{ 0 };
    std::atomic<int> pendingHeight{ 0 };
};

HostState g_state;

struct Slot {
    std::unique_ptr<Saver> saver;
    SaverContext ctx;
};

void FillContext(SaverContext& ctx, const MonitorInfo& m, int index, int count, bool preview, Settings* settings, Rng* rng) {
    ctx.previewMode = preview;
    ctx.viewport = m.rect;
    ctx.width = m.rect.right - m.rect.left;
    ctx.height = m.rect.bottom - m.rect.top;
    ctx.dpiScale = m.dpiScale;
    ctx.monitorIndex = index;
    ctx.monitorCount = count;
    ctx.settings = settings;
    ctx.rng = rng;
}

void RenderLoop(HostState& s) {
    try {
        Device device;
        RECT rc{};
        GetClientRect(s.hwnd, &rc);
        SwapChain swap(device, s.hwnd, rc.right - rc.left, rc.bottom - rc.top);
        Settings settings(s.name);
        Rng rng;

        std::vector<Slot> slots;
        const int count = static_cast<int>(s.monitors.size());
        for (int i = 0; i < count; ++i) {
            Slot slot;
            slot.saver = s.factory();
            FillContext(slot.ctx, s.monitors[i], i, count, s.preview, &settings, &rng);
            slot.saver->Initialize(device, slot.ctx);
            slots.push_back(std::move(slot));
        }

        LARGE_INTEGER freq{}, prev{}, now{};
        QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&prev);
        double time = 0.0;
        int parentCheck = 0;

        while (!s.stop.load(std::memory_order_relaxed)) {
            if (s.resizePending.exchange(false)) {
                int w = s.pendingWidth.load(), h = s.pendingHeight.load();
                swap.Resize(w, h);
                if (s.preview) {
                    s.monitors = SingleMonitor(s.hwnd);
                    for (auto& slot : slots) {
                        FillContext(slot.ctx, s.monitors[0], slot.ctx.monitorIndex, count, true, &settings, &rng);
                        slot.saver->Resize(slot.ctx.width, slot.ctx.height);
                    }
                }
            }

            QueryPerformanceCounter(&now);
            float dt = static_cast<float>(static_cast<double>(now.QuadPart - prev.QuadPart) / static_cast<double>(freq.QuadPart));
            prev = now;
            if (dt > 0.1f) dt = 0.1f;
            if (dt < 0.0f) dt = 0.0f;
            time += dt;

            swap.Bind();
            swap.ClearDepth();
            for (auto& slot : slots) {
                if (auto clear = slot.saver->ClearColor()) swap.ClearRect(*clear, slot.ctx.viewport);
                swap.SetViewport(slot.ctx.viewport);
                slot.saver->Update(dt, time);
                swap.Bind();
                slot.saver->Render(device, swap);
            }

            // Preview mode: if the hosting window vanished without destroying us (host killed), leave.
            if (s.preview && s.parent && ++parentCheck >= 30) {
                parentCheck = 0;
                if (!IsWindow(s.parent)) {
                    if (IsWindow(s.hwnd)) PostMessageW(s.hwnd, WM_CLOSE, 0, 0);
                    else PostThreadMessageW(s.mainThread, WM_QUIT, 0, 0);
                    break;
                }
            }

            HRESULT hr = swap.Present(1);
            if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
                LogLine(L"Device removed/reset during Present; closing");
                PostMessageW(s.hwnd, WM_CLOSE, 0, 0);
                break;
            }
        }
        slots.clear();
    } catch (const std::exception& e) {
        LogLine(std::string("Render thread error: ") + e.what());
        PostMessageW(s.hwnd, WM_CLOSE, 0, 0);
    } catch (...) {
        LogLine(L"Render thread error: unknown exception");
        PostMessageW(s.hwnd, WM_CLOSE, 0, 0);
    }
}

void StopThread() {
    g_state.stop = true;
    if (g_state.thread.joinable()) g_state.thread.join();
}

} // namespace

const Image* Host::DesktopImage() {
    return g_state.desktop ? &*g_state.desktop : nullptr;
}

LRESULT Host::Proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, const wchar_t* saverName, SaverFactory factory, bool captureDesktop) {
    switch (msg) {
    case WM_CREATE: {
        g_state.hwnd = hwnd;
        g_state.preview = fChildPreview != FALSE;
        g_state.parent = g_state.preview ? GetParent(hwnd) : nullptr;
        g_state.mainThread = GetCurrentThreadId();
        g_state.name = saverName;
        g_state.factory = std::move(factory);
        g_state.monitors = g_state.preview ? SingleMonitor(hwnd) : EnumerateMonitors();
        if (captureDesktop && !g_state.preview) g_state.desktop = CaptureDesktop();
        g_state.stop = false;
        g_state.thread = std::thread([] { RenderLoop(g_state); });
        return DefScreenSaverProc(hwnd, msg, wParam, lParam);
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_SIZE:
        if (g_state.preview && wParam != SIZE_MINIMIZED) {
            g_state.pendingWidth = LOWORD(lParam);
            g_state.pendingHeight = HIWORD(lParam);
            g_state.resizePending = true;
        }
        return 0;
    case WM_DESTROY:
        StopThread();
        return DefScreenSaverProc(hwnd, msg, wParam, lParam);
    default:
        return DefScreenSaverProc(hwnd, msg, wParam, lParam);
    }
}

BOOL RegisterCommonDialogClasses() {
    INITCOMMONCONTROLSEX icc{ sizeof(icc), ICC_BAR_CLASSES | ICC_STANDARD_CLASSES | ICC_UPDOWN_CLASS };
    InitCommonControlsEx(&icc);
    return TRUE;
}

// Extracts the HWND from "/c:<hwnd>" or "/c <hwnd>" (also "-c"); nullptr when absent or invalid.
static HWND OwnerFromCommandLine() {
    const wchar_t* cmd = GetCommandLineW();
    const wchar_t* p = wcsstr(cmd, L"/c");
    if (!p) p = wcsstr(cmd, L"-c");
    if (!p) p = wcsstr(cmd, L"/C");
    if (!p) p = wcsstr(cmd, L"-C");
    if (!p) return nullptr;
    p += 2;
    while (*p == L':' || *p == L' ') ++p;
    unsigned long long v = wcstoull(p, nullptr, 10);
    HWND h = reinterpret_cast<HWND>(static_cast<uintptr_t>(v));
    return (h && IsWindow(h)) ? h : nullptr;
}

BOOL RunConfigDialog(DLGPROC proc) {
    RegisterCommonDialogClasses();
    HWND owner = OwnerFromCommandLine();
    INT_PTR r = DialogBoxParamW(hMainInstance, MAKEINTRESOURCEW(DLG_SCRNSAVECONFIGURE), owner, proc, 0);
    if (r == -1 && owner) {
        LogLine(L"Config dialog could not be owned by the given window; retrying without an owner");
        DialogBoxParamW(hMainInstance, MAKEINTRESOURCEW(DLG_SCRNSAVECONFIGURE), nullptr, proc, 0);
    }
    return FALSE;   // tell scrnsave.lib not to show the dialog itself
}

} // namespace rs
