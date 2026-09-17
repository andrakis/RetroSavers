#include <windows.h>
#include <commctrl.h>
#include "Host/DialogUtil.h"
#include "Host/Settings.h"
#include "PlasmaSaver.h"
#include "resource.h"

using namespace rs;

namespace {

const wchar_t* const kPalettes[] = { L"Rainbow", L"Fire", L"Ocean", L"Neon", L"Greyscale" };
const wchar_t* const kResolutions[] = { L"Half (smooth)", L"Full", L"Quarter (chunky)" };

void Mirror(HWND dlg) {
    dlg::MirrorTrackbar(dlg, IDC_SPEED, IDC_SPEED_LABEL);
    dlg::MirrorTrackbar(dlg, IDC_SCALE, IDC_SCALE_LABEL);
}

void Apply(HWND dlg, const PlasmaSettings& s) {
    dlg::SetTrackbar(dlg, IDC_SPEED, 1, 10, s.speed, 1);
    dlg::SetTrackbar(dlg, IDC_SCALE, 1, 10, s.scale, 1);
    dlg::FillCombo(dlg, IDC_PALETTE, kPalettes, 5, s.palette);
    dlg::FillCombo(dlg, IDC_RESOLUTION, kResolutions, 3, s.resolution);
    Mirror(dlg);
}

PlasmaSettings Collect(HWND dlg) {
    PlasmaSettings s;
    s.speed = dlg::GetTrackbar(dlg, IDC_SPEED);
    s.scale = dlg::GetTrackbar(dlg, IDC_SCALE);
    s.palette = dlg::GetComboIndex(dlg, IDC_PALETTE);
    s.resolution = dlg::GetComboIndex(dlg, IDC_RESOLUTION);
    return s;
}

} // namespace

BOOL PlasmaConfigDialog(HWND dlg, UINT msg, WPARAM wParam, LPARAM) {
    switch (msg) {
    case WM_INITDIALOG: {
        Settings settings(L"Plasma");
        Apply(dlg, PlasmaSettings::Load(settings));
        return TRUE;
    }
    case WM_HSCROLL:
        Mirror(dlg);
        return TRUE;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDOK: {
            Settings settings(L"Plasma");
            Collect(dlg).Save(settings);
            EndDialog(dlg, IDOK);
            return TRUE;
        }
        case IDCANCEL:
            EndDialog(dlg, IDCANCEL);
            return TRUE;
        case IDC_DEFAULTS:
            Apply(dlg, PlasmaSettings{});
            return TRUE;
        }
        break;
    }
    return FALSE;
}
