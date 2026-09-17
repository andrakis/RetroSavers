#include <windows.h>
#include <commctrl.h>
#include "Host/DialogUtil.h"
#include "Host/Settings.h"
#include "BoingBallSaver.h"
#include "resource.h"

using namespace rs;

namespace {

BoingBallSettings g_current;
dlg::Swatch g_swatch1, g_swatch2, g_swatchGrid;

void Mirror(HWND dlg) {
    dlg::MirrorTrackbar(dlg, IDC_SPEED, IDC_SPEED_LABEL);
    dlg::MirrorTrackbar(dlg, IDC_BALL_SIZE, IDC_BALL_SIZE_LABEL);
}

void Apply(HWND dlg, const BoingBallSettings& s) {
    g_current = s;
    dlg::SetTrackbar(dlg, IDC_SPEED, 1, 10, s.speed, 1);
    dlg::SetTrackbar(dlg, IDC_BALL_SIZE, 1, 10, s.ballSize, 1);
    dlg::SetCheck(dlg, IDC_SHADOW, s.showShadow);
    g_swatch1.Set(dlg, IDC_SWATCH1, s.color1);
    g_swatch2.Set(dlg, IDC_SWATCH2, s.color2);
    g_swatchGrid.Set(dlg, IDC_SWATCH_GRID, s.gridColor);
    Mirror(dlg);
}

BoingBallSettings Collect(HWND dlg) {
    BoingBallSettings s = g_current;   // keeps the colours from the pickers
    s.speed = dlg::GetTrackbar(dlg, IDC_SPEED);
    s.ballSize = dlg::GetTrackbar(dlg, IDC_BALL_SIZE);
    s.showShadow = dlg::GetCheck(dlg, IDC_SHADOW);
    return s;
}

} // namespace

BOOL BoingBallConfigDialog(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_INITDIALOG: {
        Settings settings(L"BoingBall");
        Apply(dlg, BoingBallSettings::Load(settings));
        return TRUE;
    }
    case WM_HSCROLL:
        Mirror(dlg);
        return TRUE;
    case WM_CTLCOLORSTATIC: {
        HWND ctl = reinterpret_cast<HWND>(lParam);
        if (ctl == GetDlgItem(dlg, IDC_SWATCH1)) return static_cast<BOOL>(reinterpret_cast<INT_PTR>(g_swatch1.Brush()));
        if (ctl == GetDlgItem(dlg, IDC_SWATCH2)) return static_cast<BOOL>(reinterpret_cast<INT_PTR>(g_swatch2.Brush()));
        if (ctl == GetDlgItem(dlg, IDC_SWATCH_GRID)) return static_cast<BOOL>(reinterpret_cast<INT_PTR>(g_swatchGrid.Brush()));
        return FALSE;
    }
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_COLOR1:
            if (dlg::PickColor(dlg, g_current.color1)) g_swatch1.Set(dlg, IDC_SWATCH1, g_current.color1);
            return TRUE;
        case IDC_COLOR2:
            if (dlg::PickColor(dlg, g_current.color2)) g_swatch2.Set(dlg, IDC_SWATCH2, g_current.color2);
            return TRUE;
        case IDC_COLOR_GRID:
            if (dlg::PickColor(dlg, g_current.gridColor)) g_swatchGrid.Set(dlg, IDC_SWATCH_GRID, g_current.gridColor);
            return TRUE;
        case IDOK: {
            Settings settings(L"BoingBall");
            Collect(dlg).Save(settings);
            EndDialog(dlg, IDOK);
            return TRUE;
        }
        case IDCANCEL:
            EndDialog(dlg, IDCANCEL);
            return TRUE;
        case IDC_DEFAULTS:
            Apply(dlg, BoingBallSettings{});
            return TRUE;
        }
        break;
    case WM_DESTROY:
        g_swatch1.Reset();
        g_swatch2.Reset();
        g_swatchGrid.Reset();
        break;
    }
    return FALSE;
}
