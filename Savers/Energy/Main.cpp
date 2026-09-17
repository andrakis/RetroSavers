// scrnsave.lib entry points for Energy.
#include <windows.h>
#include <scrnsave.h>
#include "Host/Host.h"
#include "EnergySaver.h"

TCHAR szAppName[APPNAMEBUFFERLEN] = TEXT("Energy");   // required by ScrnSavW.lib

BOOL EnergyConfigDialog(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI ScreenSaverProcW(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    return rs::Host::Proc(hwnd, msg, wParam, lParam, L"Energy", [] { return std::make_unique<EnergySaver>(); });
}

BOOL WINAPI ScreenSaverConfigureDialog(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    return EnergyConfigDialog(dlg, msg, wParam, lParam);
}

BOOL WINAPI RegisterDialogClasses(HANDLE) {
    return rs::RunConfigDialog(reinterpret_cast<DLGPROC>(ScreenSaverConfigureDialog));
}
