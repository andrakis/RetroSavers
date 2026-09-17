// scrnsave.lib entry points for DVDBounce.
#include <windows.h>
#include <scrnsave.h>
#include "Host/Host.h"
#include "DVDBounceSaver.h"

TCHAR szAppName[APPNAMEBUFFERLEN] = TEXT("DVDBounce");   // required by ScrnSavW.lib

BOOL DVDBounceConfigDialog(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI ScreenSaverProcW(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    return rs::Host::Proc(hwnd, msg, wParam, lParam, L"DVDBounce", [] { return std::make_unique<DVDBounceSaver>(); });
}

BOOL WINAPI ScreenSaverConfigureDialog(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    return DVDBounceConfigDialog(dlg, msg, wParam, lParam);
}

BOOL WINAPI RegisterDialogClasses(HANDLE) {
    return rs::RunConfigDialog(reinterpret_cast<DLGPROC>(ScreenSaverConfigureDialog));
}
