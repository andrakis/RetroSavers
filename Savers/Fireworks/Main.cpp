// scrnsave.lib entry points for Fireworks.
#include <windows.h>
#include <scrnsave.h>
#include "Host/Host.h"
#include "FireworksSaver.h"

TCHAR szAppName[APPNAMEBUFFERLEN] = TEXT("Fireworks");   // required by ScrnSavW.lib

BOOL FireworksConfigDialog(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI ScreenSaverProcW(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    return rs::Host::Proc(hwnd, msg, wParam, lParam, L"Fireworks", [] { return std::make_unique<FireworksSaver>(); });
}

BOOL WINAPI ScreenSaverConfigureDialog(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    return FireworksConfigDialog(dlg, msg, wParam, lParam);
}

BOOL WINAPI RegisterDialogClasses(HANDLE) {
    return rs::RunConfigDialog(reinterpret_cast<DLGPROC>(ScreenSaverConfigureDialog));
}
