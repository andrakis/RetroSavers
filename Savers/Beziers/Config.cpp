#include <windows.h>
#include <commctrl.h>
#include "Host/DialogUtil.h"
#include "Host/Settings.h"
#include "BeziersSaver.h"
#include "resource.h"

using namespace rs;

static void Mirror(HWND dlg) {
    dlg::MirrorTrackbar(dlg, IDC_CURVES, IDC_CURVES_LABEL);
    dlg::MirrorTrackbar(dlg, IDC_TRAIL, IDC_TRAIL_LABEL);
    dlg::MirrorTrackbar(dlg, IDC_SPEED, IDC_SPEED_LABEL);
}

static void Apply(HWND dlg, const BeziersSettings& s) {
    dlg::SetTrackbar(dlg, IDC_CURVES, 1, 10, s.curves, 1);
    dlg::SetTrackbar(dlg, IDC_TRAIL, 1, 100, s.trail, 10);
    dlg::SetTrackbar(dlg, IDC_SPEED, 1, 10, s.speed, 1);
    Mirror(dlg);
}

BOOL BeziersConfigDialog(HWND dlg, UINT msg, WPARAM wParam, LPARAM) {
    switch (msg) {
    case WM_INITDIALOG: {
        Settings settings(L"Beziers");
        Apply(dlg, BeziersSettings::Load(settings));
        return TRUE;
    }
    case WM_HSCROLL:
        Mirror(dlg);
        return TRUE;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDOK: {
            BeziersSettings s;
            s.curves = dlg::GetTrackbar(dlg, IDC_CURVES);
            s.trail = dlg::GetTrackbar(dlg, IDC_TRAIL);
            s.speed = dlg::GetTrackbar(dlg, IDC_SPEED);
            Settings settings(L"Beziers");
            s.Save(settings);
            EndDialog(dlg, IDOK);
            return TRUE;
        }
        case IDCANCEL:
            EndDialog(dlg, IDCANCEL);
            return TRUE;
        case IDC_DEFAULTS:
            Apply(dlg, BeziersSettings{});
            return TRUE;
        }
        break;
    }
    return FALSE;
}
