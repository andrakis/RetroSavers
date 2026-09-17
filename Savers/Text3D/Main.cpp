// scrnsave.lib entry points for Text3D.
#include <windows.h>
#include <scrnsave.h>
#include "Host/Host.h"
#include "Text3DSaver.h"

TCHAR szAppName[APPNAMEBUFFERLEN] = TEXT("Text3D");   // required by ScrnSavW.lib

BOOL Text3DConfigDialog(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI ScreenSaverProcW(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    return rs::Host::Proc(hwnd, msg, wParam, lParam, L"Text3D", [] { return std::make_unique<Text3DSaver>(); });
}

BOOL WINAPI ScreenSaverConfigureDialog(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    return Text3DConfigDialog(dlg, msg, wParam, lParam);
}

BOOL WINAPI RegisterDialogClasses(HANDLE) {
    return rs::RunConfigDialog(reinterpret_cast<DLGPROC>(ScreenSaverConfigureDialog));
}
