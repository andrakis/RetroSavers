// scrnsave.lib entry points for Gears.
#include <windows.h>
#include <scrnsave.h>
#include "Host/Host.h"
#include "GearsSaver.h"

TCHAR szAppName[APPNAMEBUFFERLEN] = TEXT("Gears");   // required by ScrnSavW.lib

BOOL GearsConfigDialog(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI ScreenSaverProcW(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    return rs::Host::Proc(hwnd, msg, wParam, lParam, L"Gears", [] { return std::make_unique<GearsSaver>(); });
}

BOOL WINAPI ScreenSaverConfigureDialog(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    return GearsConfigDialog(dlg, msg, wParam, lParam);
}

BOOL WINAPI RegisterDialogClasses(HANDLE) {
    return rs::RunConfigDialog(reinterpret_cast<DLGPROC>(ScreenSaverConfigureDialog));
}
