// scrnsave.lib entry points for Mystify Your Mind (original).
#include <windows.h>
#include <scrnsave.h>
#include "Host/Host.h"
#include "Mystify95Saver.h"

TCHAR szAppName[APPNAMEBUFFERLEN] = TEXT("Mystify95");   // required by ScrnSavW.lib

BOOL Mystify95ConfigDialog(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI ScreenSaverProcW(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // Capture the desktop so "Clear Screen" off can draw over it like the original.
    static const bool captureDesktop = !rs::Settings(L"Mystify95").GetBool(L"ClearScreen", true);
    return rs::Host::Proc(hwnd, msg, wParam, lParam, L"Mystify95", [] { return std::make_unique<Mystify95Saver>(); }, captureDesktop);
}

BOOL WINAPI ScreenSaverConfigureDialog(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    return Mystify95ConfigDialog(dlg, msg, wParam, lParam);
}

BOOL WINAPI RegisterDialogClasses(HANDLE) {
    return rs::RunConfigDialog(reinterpret_cast<DLGPROC>(ScreenSaverConfigureDialog));
}
