// scrnsave.lib entry points for 3D Maze.
#include <windows.h>
#include <scrnsave.h>
#include "Host/Host.h"
#include "Maze3DSaver.h"

TCHAR szAppName[APPNAMEBUFFERLEN] = TEXT("Maze3D");   // required by ScrnSavW.lib

BOOL Maze3DConfigDialog(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI ScreenSaverProcW(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    return rs::Host::Proc(hwnd, msg, wParam, lParam, L"Maze3D", [] { return std::make_unique<Maze3DSaver>(); });
}

BOOL WINAPI ScreenSaverConfigureDialog(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    return Maze3DConfigDialog(dlg, msg, wParam, lParam);
}

BOOL WINAPI RegisterDialogClasses(HANDLE) {
    return rs::RunConfigDialog(reinterpret_cast<DLGPROC>(ScreenSaverConfigureDialog));
}
