// scrnsave.lib entry points for Marquee.
#include <windows.h>
#include <scrnsave.h>
#include "Host/Host.h"
#include "MarqueeSaver.h"

TCHAR szAppName[APPNAMEBUFFERLEN] = TEXT("Marquee");   // required by ScrnSavW.lib

BOOL MarqueeConfigDialog(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI ScreenSaverProcW(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    return rs::Host::Proc(hwnd, msg, wParam, lParam, L"Marquee", [] { return std::make_unique<MarqueeSaver>(); });
}

BOOL WINAPI ScreenSaverConfigureDialog(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    return MarqueeConfigDialog(dlg, msg, wParam, lParam);
}

BOOL WINAPI RegisterDialogClasses(HANDLE) {
    return rs::RunConfigDialog(reinterpret_cast<DLGPROC>(ScreenSaverConfigureDialog));
}
