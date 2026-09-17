// scrnsave.lib entry points for Plasma.
#include <windows.h>
#include <scrnsave.h>
#include "Host/Host.h"
#include "PlasmaSaver.h"

TCHAR szAppName[APPNAMEBUFFERLEN] = TEXT("Plasma");   // required by ScrnSavW.lib

BOOL PlasmaConfigDialog(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI ScreenSaverProcW(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    return rs::Host::Proc(hwnd, msg, wParam, lParam, L"Plasma", [] { return std::make_unique<PlasmaSaver>(); });
}

BOOL WINAPI ScreenSaverConfigureDialog(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    return PlasmaConfigDialog(dlg, msg, wParam, lParam);
}

BOOL WINAPI RegisterDialogClasses(HANDLE) {
    return rs::RunConfigDialog(reinterpret_cast<DLGPROC>(ScreenSaverConfigureDialog));
}
