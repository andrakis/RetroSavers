// scrnsave.lib entry points for BoingBall.
#include <windows.h>
#include <scrnsave.h>
#include "Host/Host.h"
#include "BoingBallSaver.h"

TCHAR szAppName[APPNAMEBUFFERLEN] = TEXT("BoingBall");   // required by ScrnSavW.lib

BOOL BoingBallConfigDialog(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI ScreenSaverProcW(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    return rs::Host::Proc(hwnd, msg, wParam, lParam, L"BoingBall", [] { return std::make_unique<BoingBallSaver>(); });
}

BOOL WINAPI ScreenSaverConfigureDialog(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    return BoingBallConfigDialog(dlg, msg, wParam, lParam);
}

BOOL WINAPI RegisterDialogClasses(HANDLE) {
    return rs::RunConfigDialog(reinterpret_cast<DLGPROC>(ScreenSaverConfigureDialog));
}
