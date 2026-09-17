// scrnsave.lib entry points for RibbonsVista.
#include <windows.h>
#include <scrnsave.h>
#include "Host/Host.h"
#include "RibbonsVistaSaver.h"

TCHAR szAppName[APPNAMEBUFFERLEN] = TEXT("RibbonsVista");   // required by ScrnSavW.lib

BOOL RibbonsVistaConfigDialog(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI ScreenSaverProcW(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    return rs::Host::Proc(hwnd, msg, wParam, lParam, L"RibbonsVista", [] { return std::make_unique<RibbonsVistaSaver>(); });
}

BOOL WINAPI ScreenSaverConfigureDialog(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    return RibbonsVistaConfigDialog(dlg, msg, wParam, lParam);
}

BOOL WINAPI RegisterDialogClasses(HANDLE) {
    return rs::RunConfigDialog(reinterpret_cast<DLGPROC>(ScreenSaverConfigureDialog));
}
