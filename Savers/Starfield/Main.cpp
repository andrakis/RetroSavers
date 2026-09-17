// scrnsave.lib entry points for Starfield.
#include <windows.h>
#include <scrnsave.h>
#include "Host/Host.h"
#include "StarfieldSaver.h"

TCHAR szAppName[APPNAMEBUFFERLEN] = TEXT("Starfield");   // required by ScrnSavW.lib

BOOL StarfieldConfigDialog(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI ScreenSaverProcW(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    return rs::Host::Proc(hwnd, msg, wParam, lParam, L"Starfield", [] { return std::make_unique<StarfieldSaver>(); });
}

BOOL WINAPI ScreenSaverConfigureDialog(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    return StarfieldConfigDialog(dlg, msg, wParam, lParam);
}

BOOL WINAPI RegisterDialogClasses(HANDLE) {
    return rs::RunConfigDialog(reinterpret_cast<DLGPROC>(ScreenSaverConfigureDialog));
}

