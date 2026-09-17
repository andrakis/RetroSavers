// scrnsave.lib entry points for AfterDark.
#include <windows.h>
#include <scrnsave.h>
#include "Host/Host.h"
#include "AfterDarkSaver.h"

TCHAR szAppName[APPNAMEBUFFERLEN] = TEXT("AfterDark");   // required by ScrnSavW.lib

BOOL AfterDarkConfigDialog(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI ScreenSaverProcW(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // Rain mode can fall on the captured desktop.
    static const bool captureDesktop = [] {
        rs::Settings s(L"AfterDark");
        return s.GetInt(L"Mode", AfterDarkSettings::Toasters) == AfterDarkSettings::Rain && s.GetBool(L"RainOnDesktop", true);
    }();
    return rs::Host::Proc(hwnd, msg, wParam, lParam, L"AfterDark", [] { return std::make_unique<AfterDarkSaver>(); }, captureDesktop);
}

BOOL WINAPI ScreenSaverConfigureDialog(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    return AfterDarkConfigDialog(dlg, msg, wParam, lParam);
}

BOOL WINAPI RegisterDialogClasses(HANDLE) {
    return rs::RunConfigDialog(reinterpret_cast<DLGPROC>(ScreenSaverConfigureDialog));
}
