// scrnsave.lib entry points for FlyingWindows.
#include <windows.h>
#include <scrnsave.h>
#include "Host/Host.h"
#include "FlyingWindowsSaver.h"

TCHAR szAppName[APPNAMEBUFFERLEN] = TEXT("FlyingWindows");   // required by ScrnSavW.lib

BOOL FlyingWindowsConfigDialog(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI ScreenSaverProcW(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    return rs::Host::Proc(hwnd, msg, wParam, lParam, L"FlyingWindows", [] { return std::make_unique<FlyingWindowsSaver>(); });
}

BOOL WINAPI ScreenSaverConfigureDialog(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    return FlyingWindowsConfigDialog(dlg, msg, wParam, lParam);
}

BOOL WINAPI RegisterDialogClasses(HANDLE) {
    return rs::RunConfigDialog(reinterpret_cast<DLGPROC>(ScreenSaverConfigureDialog));
}
