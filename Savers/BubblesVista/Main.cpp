// scrnsave.lib entry points for BubblesVista.
#include <windows.h>
#include <scrnsave.h>
#include "Host/Host.h"
#include "BubblesVistaSaver.h"

TCHAR szAppName[APPNAMEBUFFERLEN] = TEXT("BubblesVista");   // required by ScrnSavW.lib

BOOL BubblesVistaConfigDialog(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI ScreenSaverProcW(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // Capture the desktop so the bubbles can float over (and refract) it.
    static const bool captureDesktop = rs::Settings(L"BubblesVista").GetBool(L"ShowOnDesktop", true);
    return rs::Host::Proc(hwnd, msg, wParam, lParam, L"BubblesVista", [] { return std::make_unique<BubblesVistaSaver>(); }, captureDesktop);
}

BOOL WINAPI ScreenSaverConfigureDialog(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    return BubblesVistaConfigDialog(dlg, msg, wParam, lParam);
}

BOOL WINAPI RegisterDialogClasses(HANDLE) {
    return rs::RunConfigDialog(reinterpret_cast<DLGPROC>(ScreenSaverConfigureDialog));
}
