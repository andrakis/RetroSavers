// scrnsave.lib entry points for FlyingObjects.
#include <windows.h>
#include <scrnsave.h>
#include "Host/Host.h"
#include "FlyingObjectsSaver.h"

TCHAR szAppName[APPNAMEBUFFERLEN] = TEXT("FlyingObjects");   // required by ScrnSavW.lib

BOOL FlyingObjectsConfigDialog(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI ScreenSaverProcW(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    return rs::Host::Proc(hwnd, msg, wParam, lParam, L"FlyingObjects", [] { return std::make_unique<FlyingObjectsSaver>(); });
}

BOOL WINAPI ScreenSaverConfigureDialog(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    return FlyingObjectsConfigDialog(dlg, msg, wParam, lParam);
}

BOOL WINAPI RegisterDialogClasses(HANDLE) {
    return rs::RunConfigDialog(reinterpret_cast<DLGPROC>(ScreenSaverConfigureDialog));
}
