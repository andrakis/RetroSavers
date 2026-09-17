#include <windows.h>
#include <commctrl.h>
#include "Host/DialogUtil.h"
#include "Host/Settings.h"
#include "StarfieldSaver.h"
#include "resource.h"

using namespace rs;

static void Apply(HWND dlg, const StarfieldSettings& s) {
    dlg::SetTrackbar(dlg, IDC_STARS, 10, 200, s.starCount, 10);
    dlg::SetTrackbar(dlg, IDC_SPEED, 1, 10, s.warpSpeed, 1);
    dlg::MirrorTrackbar(dlg, IDC_STARS, IDC_STARS_LABEL);
    dlg::MirrorTrackbar(dlg, IDC_SPEED, IDC_SPEED_LABEL);
}

BOOL StarfieldConfigDialog(HWND dlg, UINT msg, WPARAM wParam, LPARAM) {
    switch (msg) {
    case WM_INITDIALOG: {
        Settings settings(L"Starfield");
        Apply(dlg, StarfieldSettings::Load(settings));
        return TRUE;
    }
    case WM_HSCROLL:
        dlg::MirrorTrackbar(dlg, IDC_STARS, IDC_STARS_LABEL);
        dlg::MirrorTrackbar(dlg, IDC_SPEED, IDC_SPEED_LABEL);
        return TRUE;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDOK: {
            StarfieldSettings s;
            s.starCount = dlg::GetTrackbar(dlg, IDC_STARS);
            s.warpSpeed = dlg::GetTrackbar(dlg, IDC_SPEED);
            Settings settings(L"Starfield");
            s.Save(settings);
            EndDialog(dlg, IDOK);
            return TRUE;
        }
        case IDCANCEL:
            EndDialog(dlg, IDCANCEL);
            return TRUE;
        case IDC_DEFAULTS:
            Apply(dlg, StarfieldSettings{});
            return TRUE;
        }
        break;
    }
    return FALSE;
}
