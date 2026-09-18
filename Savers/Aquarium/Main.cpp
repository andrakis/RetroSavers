// scrnsave.lib entry points for Aquarium.
#include <windows.h>
#include <scrnsave.h>
#include "Host/Host.h"
#include "AquariumSaver.h"

TCHAR szAppName[APPNAMEBUFFERLEN] = TEXT("Aquarium");   // required by ScrnSavW.lib

BOOL AquariumConfigDialog(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI ScreenSaverProcW(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    return rs::Host::Proc(hwnd, msg, wParam, lParam, L"Aquarium", [] { return std::make_unique<AquariumSaver>(); });
}

BOOL WINAPI ScreenSaverConfigureDialog(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    return AquariumConfigDialog(dlg, msg, wParam, lParam);
}

BOOL WINAPI RegisterDialogClasses(HANDLE) {
    return rs::RunConfigDialog(reinterpret_cast<DLGPROC>(ScreenSaverConfigureDialog));
}
