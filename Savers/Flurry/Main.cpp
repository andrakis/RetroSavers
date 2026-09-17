// scrnsave.lib entry points for Flurry.
#include <windows.h>
#include <scrnsave.h>
#include "Host/Host.h"
#include "FlurrySaver.h"

TCHAR szAppName[APPNAMEBUFFERLEN] = TEXT("Flurry");   // required by ScrnSavW.lib

BOOL FlurryConfigDialog(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI ScreenSaverProcW(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    return rs::Host::Proc(hwnd, msg, wParam, lParam, L"Flurry", [] { return std::make_unique<FlurrySaver>(); });
}

BOOL WINAPI ScreenSaverConfigureDialog(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    return FlurryConfigDialog(dlg, msg, wParam, lParam);
}

BOOL WINAPI RegisterDialogClasses(HANDLE) {
    return rs::RunConfigDialog(reinterpret_cast<DLGPROC>(ScreenSaverConfigureDialog));
}
