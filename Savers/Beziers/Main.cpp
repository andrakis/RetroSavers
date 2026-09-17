// scrnsave.lib entry points for Beziers.
#include <windows.h>
#include <scrnsave.h>
#include "Host/Host.h"
#include "BeziersSaver.h"

TCHAR szAppName[APPNAMEBUFFERLEN] = TEXT("Beziers");   // required by ScrnSavW.lib

BOOL BeziersConfigDialog(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI ScreenSaverProcW(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    return rs::Host::Proc(hwnd, msg, wParam, lParam, L"Beziers", [] { return std::make_unique<BeziersSaver>(); });
}

BOOL WINAPI ScreenSaverConfigureDialog(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    return BeziersConfigDialog(dlg, msg, wParam, lParam);
}

BOOL WINAPI RegisterDialogClasses(HANDLE) {
    return rs::RunConfigDialog(reinterpret_cast<DLGPROC>(ScreenSaverConfigureDialog));
}
