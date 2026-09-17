#include <windows.h>
#include <commctrl.h>
#include "Host/DialogUtil.h"
#include "Host/Settings.h"
#include "Maze3DSaver.h"
#include "resource.h"

using namespace rs;

namespace {

void Mirror(HWND dlg) {
    int size = dlg::GetTrackbar(dlg, IDC_MAZE_SIZE) | 1;
    wchar_t buf[32];
    swprintf_s(buf, L"%d x %d", size, size);
    dlg::SetText(dlg, IDC_MAZE_SIZE_LABEL, buf);
    dlg::MirrorTrackbar(dlg, IDC_SPEED, IDC_SPEED_LABEL);
}

void Apply(HWND dlg, const Maze3DSettings& s) {
    dlg::SetText(dlg, IDC_WALL_PATH, s.wallTexture);
    dlg::SetText(dlg, IDC_FLOOR_PATH, s.floorTexture);
    dlg::SetText(dlg, IDC_CEILING_PATH, s.ceilingTexture);
    dlg::SetCheck(dlg, IDC_SHOW_RAT, s.showRat);
    dlg::SetCheck(dlg, IDC_SHOW_LOGO, s.showLogo);
    dlg::SetTrackbar(dlg, IDC_MAZE_SIZE, 9, 31, s.mazeSize, 2);
    dlg::SetTrackbar(dlg, IDC_SPEED, 1, 10, s.speed, 1);
    Mirror(dlg);
}

Maze3DSettings Collect(HWND dlg) {
    Maze3DSettings s;
    s.wallTexture = dlg::GetText(dlg, IDC_WALL_PATH);
    s.floorTexture = dlg::GetText(dlg, IDC_FLOOR_PATH);
    s.ceilingTexture = dlg::GetText(dlg, IDC_CEILING_PATH);
    s.showRat = dlg::GetCheck(dlg, IDC_SHOW_RAT);
    s.showLogo = dlg::GetCheck(dlg, IDC_SHOW_LOGO);
    s.mazeSize = dlg::GetTrackbar(dlg, IDC_MAZE_SIZE) | 1;
    s.speed = dlg::GetTrackbar(dlg, IDC_SPEED);
    return s;
}

void Browse(HWND dlg, int editId) {
    std::wstring path = dlg::GetText(dlg, editId);
    if (dlg::PickBmpFile(dlg, path)) dlg::SetText(dlg, editId, path);
}

} // namespace

BOOL Maze3DConfigDialog(HWND dlg, UINT msg, WPARAM wParam, LPARAM) {
    switch (msg) {
    case WM_INITDIALOG: {
        Settings settings(L"Maze3D");
        Apply(dlg, Maze3DSettings::Load(settings));
        return TRUE;
    }
    case WM_HSCROLL:
        Mirror(dlg);
        return TRUE;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_WALL_BROWSE: Browse(dlg, IDC_WALL_PATH); return TRUE;
        case IDC_FLOOR_BROWSE: Browse(dlg, IDC_FLOOR_PATH); return TRUE;
        case IDC_CEILING_BROWSE: Browse(dlg, IDC_CEILING_PATH); return TRUE;
        case IDOK: {
            Settings settings(L"Maze3D");
            Collect(dlg).Save(settings);
            EndDialog(dlg, IDOK);
            return TRUE;
        }
        case IDCANCEL:
            EndDialog(dlg, IDCANCEL);
            return TRUE;
        case IDC_DEFAULTS:
            Apply(dlg, Maze3DSettings{});
            return TRUE;
        }
        break;
    }
    return FALSE;
}
