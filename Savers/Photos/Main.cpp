// scrnsave.lib entry points for Photos.
#include <windows.h>
#include <scrnsave.h>
#include "Host/Host.h"
#include "PhotosSaver.h"

TCHAR szAppName[APPNAMEBUFFERLEN] = TEXT("Photos");   // required by ScrnSavW.lib

BOOL PhotosConfigDialog(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI ScreenSaverProcW(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    return rs::Host::Proc(hwnd, msg, wParam, lParam, L"Photos", [] { return std::make_unique<PhotosSaver>(); });
}

BOOL WINAPI ScreenSaverConfigureDialog(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    return PhotosConfigDialog(dlg, msg, wParam, lParam);
}

BOOL WINAPI RegisterDialogClasses(HANDLE) {
    return rs::RunConfigDialog(reinterpret_cast<DLGPROC>(ScreenSaverConfigureDialog));
}
