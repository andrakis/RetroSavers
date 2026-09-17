// scrnsave.lib entry points for 3D Pipes.
#include <windows.h>
#include <scrnsave.h>
#include "Host/Host.h"
#include "Pipes3DSaver.h"

TCHAR szAppName[APPNAMEBUFFERLEN] = TEXT("Pipes3D");   // required by ScrnSavW.lib

BOOL Pipes3DConfigDialog(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI ScreenSaverProcW(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    return rs::Host::Proc(hwnd, msg, wParam, lParam, L"Pipes3D", [] { return std::make_unique<Pipes3DSaver>(); });
}

BOOL WINAPI ScreenSaverConfigureDialog(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    return Pipes3DConfigDialog(dlg, msg, wParam, lParam);
}

BOOL WINAPI RegisterDialogClasses(HANDLE) {
    return rs::RunConfigDialog(reinterpret_cast<DLGPROC>(ScreenSaverConfigureDialog));
}
