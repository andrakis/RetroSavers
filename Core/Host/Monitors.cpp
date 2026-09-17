#include "Monitors.h"
#include <shellscalingapi.h>
#include <cstdlib>

#pragma comment(lib, "shcore.lib")

namespace rs {

static float MonitorScale(HMONITOR mon) {
    UINT dx = 96, dy = 96;
    if (SUCCEEDED(GetDpiForMonitor(mon, MDT_EFFECTIVE_DPI, &dx, &dy)) && dx > 0)
        return dx / 96.0f;
    return 1.0f;
}

static BOOL CALLBACK EnumProc(HMONITOR mon, HDC, LPRECT, LPARAM lp) {
    auto* out = reinterpret_cast<std::vector<MonitorInfo>*>(lp);
    MONITORINFO mi{ sizeof(mi) };
    if (!GetMonitorInfoW(mon, &mi)) return TRUE;
    MonitorInfo info;
    info.rect = mi.rcMonitor;
    info.dpiScale = MonitorScale(mon);
    info.primary = (mi.dwFlags & MONITORINFOF_PRIMARY) != 0;
    out->push_back(info);
    return TRUE;
}

std::vector<MonitorInfo> EnumerateMonitors() {
    std::vector<MonitorInfo> monitors;
    EnumDisplayMonitors(nullptr, nullptr, EnumProc, reinterpret_cast<LPARAM>(&monitors));
    int ox = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int oy = GetSystemMetrics(SM_YVIRTUALSCREEN);
    for (auto& m : monitors) OffsetRect(&m.rect, -ox, -oy);
    if (monitors.empty()) {
        MonitorInfo m;
        m.rect = { 0, 0, GetSystemMetrics(SM_CXVIRTUALSCREEN), GetSystemMetrics(SM_CYVIRTUALSCREEN) };
        m.primary = true;
        monitors.push_back(m);
    }

    wchar_t buf[16]{};
    if (GetEnvironmentVariableW(L"RETROSAVERS_FAKE_MONITORS", buf, 16) > 0) {
        int n = _wtoi(buf);
        if (n >= 2 && n <= 8) {
            std::vector<MonitorInfo> fake;
            for (const auto& m : monitors) {
                if (!m.primary) { fake.push_back(m); continue; }
                int w = (m.rect.right - m.rect.left) / n;
                for (int i = 0; i < n; ++i) {
                    MonitorInfo f = m;
                    f.rect.left = m.rect.left + i * w;
                    f.rect.right = (i == n - 1) ? m.rect.right : f.rect.left + w;
                    f.primary = (i == 0);
                    fake.push_back(f);
                }
            }
            monitors.swap(fake);
        }
    }
    return monitors;
}

std::vector<MonitorInfo> SingleMonitor(HWND hwnd) {
    MonitorInfo m;
    GetClientRect(hwnd, &m.rect);
    if (m.rect.right <= m.rect.left) m.rect.right = m.rect.left + 1;
    if (m.rect.bottom <= m.rect.top) m.rect.bottom = m.rect.top + 1;
    UINT dpi = GetDpiForWindow(hwnd);
    m.dpiScale = dpi > 0 ? dpi / 96.0f : 1.0f;
    m.primary = true;
    return { m };
}

} // namespace rs
