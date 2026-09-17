#include <windows.h>
#include <commctrl.h>
#include "Host/DialogUtil.h"
#include "Host/Settings.h"
#include "AttractorsSaver.h"
#include "resource.h"

using namespace rs;

namespace {

const wchar_t* const kAttractors[] = { L"Lorenz", L"R\x00f6ssler", L"Aizawa", L"Thomas", L"Halvorsen", L"Cycle through all" };
const wchar_t* const kColors[] = { L"By speed", L"By height", L"Cycling hue" };

void Mirror(HWND dlg) {
    dlg::SetText(dlg, IDC_PARTICLES_LABEL, std::to_wstring(dlg::GetTrackbar(dlg, IDC_PARTICLES) * 1000));
    dlg::MirrorTrackbar(dlg, IDC_TRAIL, IDC_TRAIL_LABEL);
    dlg::MirrorTrackbar(dlg, IDC_SPEED, IDC_SPEED_LABEL);
}

void Apply(HWND dlg, const AttractorsSettings& s) {
    dlg::FillCombo(dlg, IDC_ATTRACTOR, kAttractors, 6, s.attractor);
    dlg::FillCombo(dlg, IDC_COLOR_MODE, kColors, 3, s.colorMode);
    dlg::SetTrackbar(dlg, IDC_PARTICLES, 2, 40, s.particles / 1000, 5);
    dlg::SetTrackbar(dlg, IDC_TRAIL, 1, 10, s.trail, 1);
    dlg::SetTrackbar(dlg, IDC_SPEED, 1, 10, s.speed, 1);
    Mirror(dlg);
}

AttractorsSettings Collect(HWND dlg) {
    AttractorsSettings s;
    s.attractor = dlg::GetComboIndex(dlg, IDC_ATTRACTOR);
    s.colorMode = dlg::GetComboIndex(dlg, IDC_COLOR_MODE);
    s.particles = dlg::GetTrackbar(dlg, IDC_PARTICLES) * 1000;
    s.trail = dlg::GetTrackbar(dlg, IDC_TRAIL);
    s.speed = dlg::GetTrackbar(dlg, IDC_SPEED);
    return s;
}

} // namespace

BOOL AttractorsConfigDialog(HWND dlg, UINT msg, WPARAM wParam, LPARAM) {
    switch (msg) {
    case WM_INITDIALOG: {
        Settings settings(L"Attractors");
        Apply(dlg, AttractorsSettings::Load(settings));
        return TRUE;
    }
    case WM_HSCROLL:
        Mirror(dlg);
        return TRUE;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDOK: {
            Settings settings(L"Attractors");
            Collect(dlg).Save(settings);
            EndDialog(dlg, IDOK);
            return TRUE;
        }
        case IDCANCEL:
            EndDialog(dlg, IDCANCEL);
            return TRUE;
        case IDC_DEFAULTS:
            Apply(dlg, AttractorsSettings{});
            return TRUE;
        }
        break;
    }
    return FALSE;
}
