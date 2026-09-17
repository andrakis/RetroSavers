#include <windows.h>
#include <commctrl.h>
#include "Host/DialogUtil.h"
#include "Host/Settings.h"
#include "EnergySaver.h"
#include "resource.h"

using namespace rs;

namespace {

void Mirror(HWND dlg) {
    dlg::SetText(dlg, IDC_STREAMERS_LABEL, std::to_wstring(20 + 10 * dlg::GetTrackbar(dlg, IDC_STREAMERS)));
    dlg::MirrorTrackbar(dlg, IDC_AMPLITUDE, IDC_AMPLITUDE_LABEL);
    dlg::MirrorTrackbar(dlg, IDC_SPEED, IDC_SPEED_LABEL);
    dlg::SetText(dlg, IDC_TINT_LABEL, std::to_wstring(dlg::GetTrackbar(dlg, IDC_TINT)) + L"\x00b0");
}

void Apply(HWND dlg, const EnergySettings& s) {
    dlg::SetTrackbar(dlg, IDC_STREAMERS, 1, 10, s.streamers, 1);
    dlg::SetTrackbar(dlg, IDC_AMPLITUDE, 1, 10, s.amplitude, 1);
    dlg::SetTrackbar(dlg, IDC_SPEED, 1, 10, s.speed, 1);
    dlg::SetTrackbar(dlg, IDC_TINT, 0, 359, s.tint, 30);
    dlg::SetCheck(dlg, IDC_BLOOM, s.bloom);
    Mirror(dlg);
}

EnergySettings Collect(HWND dlg) {
    EnergySettings s;
    s.streamers = dlg::GetTrackbar(dlg, IDC_STREAMERS);
    s.amplitude = dlg::GetTrackbar(dlg, IDC_AMPLITUDE);
    s.speed = dlg::GetTrackbar(dlg, IDC_SPEED);
    s.tint = dlg::GetTrackbar(dlg, IDC_TINT);
    s.bloom = dlg::GetCheck(dlg, IDC_BLOOM);
    return s;
}

} // namespace

BOOL EnergyConfigDialog(HWND dlg, UINT msg, WPARAM wParam, LPARAM) {
    switch (msg) {
    case WM_INITDIALOG: {
        Settings settings(L"Energy");
        Apply(dlg, EnergySettings::Load(settings));
        return TRUE;
    }
    case WM_HSCROLL:
        Mirror(dlg);
        return TRUE;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDOK: {
            Settings settings(L"Energy");
            Collect(dlg).Save(settings);
            EndDialog(dlg, IDOK);
            return TRUE;
        }
        case IDCANCEL:
            EndDialog(dlg, IDCANCEL);
            return TRUE;
        case IDC_DEFAULTS:
            Apply(dlg, EnergySettings{});
            return TRUE;
        }
        break;
    }
    return FALSE;
}
