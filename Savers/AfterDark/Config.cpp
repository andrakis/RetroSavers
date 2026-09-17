#include <windows.h>
#include <commctrl.h>
#include "Host/DialogUtil.h"
#include "Host/Settings.h"
#include "AfterDarkSaver.h"
#include "resource.h"

using namespace rs;

namespace {

const wchar_t* const kModes[] = { L"Starry Night", L"Warp", L"Rain", L"Flying Toasters" };

void Mirror(HWND dlg) {
    dlg::MirrorTrackbar(dlg, IDC_DENSITY, IDC_DENSITY_LABEL);
    dlg::MirrorTrackbar(dlg, IDC_SPEED, IDC_SPEED_LABEL);
}

void EnableRain(HWND dlg) {
    EnableWindow(GetDlgItem(dlg, IDC_RAIN_DESKTOP), dlg::GetComboIndex(dlg, IDC_MODE) == AfterDarkSettings::Rain);
}

void Apply(HWND dlg, const AfterDarkSettings& s) {
    dlg::FillCombo(dlg, IDC_MODE, kModes, 4, s.mode);
    dlg::SetTrackbar(dlg, IDC_DENSITY, 1, 10, s.density, 1);
    dlg::SetTrackbar(dlg, IDC_SPEED, 1, 10, s.speed, 1);
    dlg::SetCheck(dlg, IDC_RAIN_DESKTOP, s.rainOnDesktop);
    Mirror(dlg);
    EnableRain(dlg);
}

AfterDarkSettings Collect(HWND dlg) {
    AfterDarkSettings s;
    s.mode = dlg::GetComboIndex(dlg, IDC_MODE);
    s.density = dlg::GetTrackbar(dlg, IDC_DENSITY);
    s.speed = dlg::GetTrackbar(dlg, IDC_SPEED);
    s.rainOnDesktop = dlg::GetCheck(dlg, IDC_RAIN_DESKTOP);
    return s;
}

} // namespace

BOOL AfterDarkConfigDialog(HWND dlg, UINT msg, WPARAM wParam, LPARAM) {
    switch (msg) {
    case WM_INITDIALOG: {
        Settings settings(L"AfterDark");
        Apply(dlg, AfterDarkSettings::Load(settings));
        return TRUE;
    }
    case WM_HSCROLL:
        Mirror(dlg);
        return TRUE;
    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_MODE && HIWORD(wParam) == CBN_SELCHANGE) { EnableRain(dlg); return TRUE; }
        switch (LOWORD(wParam)) {
        case IDOK: {
            Settings settings(L"AfterDark");
            Collect(dlg).Save(settings);
            EndDialog(dlg, IDOK);
            return TRUE;
        }
        case IDCANCEL:
            EndDialog(dlg, IDCANCEL);
            return TRUE;
        case IDC_DEFAULTS:
            Apply(dlg, AfterDarkSettings{});
            return TRUE;
        }
        break;
    }
    return FALSE;
}
