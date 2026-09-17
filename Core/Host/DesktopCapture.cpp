#include "DesktopCapture.h"
#include "Util/Color.h"
#include <windows.h>

namespace rs {

std::optional<Image> CaptureDesktop() {
    int x = GetSystemMetrics(SM_XVIRTUALSCREEN), y = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int w = GetSystemMetrics(SM_CXVIRTUALSCREEN), h = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    if (w <= 0 || h <= 0) return std::nullopt;

    HDC screen = GetDC(nullptr);
    if (!screen) return std::nullopt;
    HDC mem = CreateCompatibleDC(screen);
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP bmp = CreateDIBSection(mem, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    std::optional<Image> result;
    if (bmp && bits) {
        HGDIOBJ old = SelectObject(mem, bmp);
        if (BitBlt(mem, 0, 0, w, h, screen, x, y, SRCCOPY | CAPTUREBLT)) {
            GdiFlush();
            Image img(w, h);
            const uint32_t* src = static_cast<const uint32_t*>(bits);
            for (int i = 0; i < w * h; ++i) {
                uint32_t p = src[i];
                img.pixels[i] = PackRgba(static_cast<uint8_t>((p >> 16) & 0xFF), static_cast<uint8_t>((p >> 8) & 0xFF), static_cast<uint8_t>(p & 0xFF), 255);
            }
            result = std::move(img);
        }
        SelectObject(mem, old);
    }
    if (bmp) DeleteObject(bmp);
    DeleteDC(mem);
    ReleaseDC(nullptr, screen);
    return result;
}

} // namespace rs
