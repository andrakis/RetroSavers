// scrnsave.lib entry points for Matrix.
#include <windows.h>
#include <scrnsave.h>
#include "Host/Host.h"
#include "MatrixSaver.h"

TCHAR szAppName[APPNAMEBUFFERLEN] = TEXT("Matrix");   // required by ScrnSavW.lib

BOOL MatrixConfigDialog(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI ScreenSaverProcW(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    return rs::Host::Proc(hwnd, msg, wParam, lParam, L"Matrix", [] { return std::make_unique<MatrixSaver>(); });
}

BOOL WINAPI ScreenSaverConfigureDialog(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    return MatrixConfigDialog(dlg, msg, wParam, lParam);
}

BOOL WINAPI RegisterDialogClasses(HANDLE) {
    return rs::RunConfigDialog(reinterpret_cast<DLGPROC>(ScreenSaverConfigureDialog));
}
