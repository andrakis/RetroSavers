// scrnsave.lib entry points for Aurora.
#include <windows.h>
#include <scrnsave.h>
#include "Host/Host.h"
#include "AuroraSaver.h"

TCHAR szAppName[APPNAMEBUFFERLEN] = TEXT("Aurora");   // required by ScrnSavW.lib

BOOL AuroraConfigDialog(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI ScreenSaverProcW(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    return rs::Host::Proc(hwnd, msg, wParam, lParam, L"Aurora", [] { return std::make_unique<AuroraSaver>(); });
}

BOOL WINAPI ScreenSaverConfigureDialog(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    return AuroraConfigDialog(dlg, msg, wParam, lParam);
}

BOOL WINAPI RegisterDialogClasses(HANDLE) {
    return rs::RunConfigDialog(reinterpret_cast<DLGPROC>(ScreenSaverConfigureDialog));
}
