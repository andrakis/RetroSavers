// scrnsave.lib entry points for Attractors.
#include <windows.h>
#include <scrnsave.h>
#include "Host/Host.h"
#include "AttractorsSaver.h"

TCHAR szAppName[APPNAMEBUFFERLEN] = TEXT("Attractors");   // required by ScrnSavW.lib

BOOL AttractorsConfigDialog(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI ScreenSaverProcW(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    return rs::Host::Proc(hwnd, msg, wParam, lParam, L"Attractors", [] { return std::make_unique<AttractorsSaver>(); });
}

BOOL WINAPI ScreenSaverConfigureDialog(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    return AttractorsConfigDialog(dlg, msg, wParam, lParam);
}

BOOL WINAPI RegisterDialogClasses(HANDLE) {
    return rs::RunConfigDialog(reinterpret_cast<DLGPROC>(ScreenSaverConfigureDialog));
}
