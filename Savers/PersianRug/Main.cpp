// scrnsave.lib entry points for PersianRug.
#include <windows.h>
#include <scrnsave.h>
#include "Host/Host.h"
#include "PersianRugSaver.h"

TCHAR szAppName[APPNAMEBUFFERLEN] = TEXT("PersianRug");   // required by ScrnSavW.lib

BOOL PersianRugConfigDialog(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI ScreenSaverProcW(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    return rs::Host::Proc(hwnd, msg, wParam, lParam, L"PersianRug", [] { return std::make_unique<PersianRugSaver>(); });
}

BOOL WINAPI ScreenSaverConfigureDialog(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    return PersianRugConfigDialog(dlg, msg, wParam, lParam);
}

BOOL WINAPI RegisterDialogClasses(HANDLE) {
    return rs::RunConfigDialog(reinterpret_cast<DLGPROC>(ScreenSaverConfigureDialog));
}
