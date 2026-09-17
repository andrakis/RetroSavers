#pragma once
#include <windows.h>
#include <vector>

namespace rs {

struct MonitorInfo {
    RECT rect{};           // in window client space (virtual-screen origin subtracted)
    float dpiScale = 1.0f;
    bool primary = false;
};

// Run mode: every monitor, rects translated so the virtual-screen origin is (0,0).
// Honours RETROSAVERS_FAKE_MONITORS=N (dev aid) by splitting the primary into N columns.
std::vector<MonitorInfo> EnumerateMonitors();

// Preview / dev mode: the single client rect of the given window.
std::vector<MonitorInfo> SingleMonitor(HWND hwnd);

} // namespace rs
