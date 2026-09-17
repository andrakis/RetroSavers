// scrnsave.lib entry points for FlowerBox.
#include <windows.h>
#include <scrnsave.h>
#include "Host/Host.h"
#include "FlowerBoxSaver.h"

TCHAR szAppName[APPNAMEBUFFERLEN] = TEXT("FlowerBox");   // required by ScrnSavW.lib

BOOL FlowerBoxConfigDialog(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI ScreenSaverProcW(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    return rs::Host::Proc(hwnd, msg, wParam, lParam, L"FlowerBox", [] { return std::make_unique<FlowerBoxSaver>(); });
}

BOOL WINAPI ScreenSaverConfigureDialog(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    return FlowerBoxConfigDialog(dlg, msg, wParam, lParam);
}

BOOL WINAPI RegisterDialogClasses(HANDLE) {
    return rs::RunConfigDialog(reinterpret_cast<DLGPROC>(ScreenSaverConfigureDialog));
}
