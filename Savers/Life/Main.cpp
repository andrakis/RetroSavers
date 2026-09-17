// scrnsave.lib entry points for Life.
#include <windows.h>
#include <scrnsave.h>
#include "Host/Host.h"
#include "LifeSaver.h"

TCHAR szAppName[APPNAMEBUFFERLEN] = TEXT("Life");   // required by ScrnSavW.lib

BOOL LifeConfigDialog(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI ScreenSaverProcW(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    return rs::Host::Proc(hwnd, msg, wParam, lParam, L"Life", [] { return std::make_unique<LifeSaver>(); });
}

BOOL WINAPI ScreenSaverConfigureDialog(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    return LifeConfigDialog(dlg, msg, wParam, lParam);
}

BOOL WINAPI RegisterDialogClasses(HANDLE) {
    return rs::RunConfigDialog(reinterpret_cast<DLGPROC>(ScreenSaverConfigureDialog));
}
