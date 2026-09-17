#include <windows.h>
#include <commctrl.h>
#include "Host/DialogUtil.h"
#include "Host/Settings.h"
#include "PersianRugSaver.h"
#include "resource.h"

using namespace rs;

namespace {

const wchar_t* const kPalettes[] = { L"Persian", L"Rainbow", L"Cool", L"Random", L"Greyscale" };
const wchar_t* const kLayouts[] = { L"Square", L"Stretch", L"Tile" };

void Mirror(HWND dlg) {
    int detail = dlg::GetTrackbar(dlg, IDC_DETAIL);
    int cells = (1 << detail) + 1;
    dlg::SetText(dlg, IDC_DETAIL_LABEL, std::to_wstring(cells) + L" x " + std::to_wstring(cells));
    dlg::MirrorTrackbar(dlg, IDC_COLORS, IDC_COLORS_LABEL);
    dlg::MirrorTrackbar(dlg, IDC_SPEED, IDC_SPEED_LABEL);
    dlg::SetText(dlg, IDC_HOLD_LABEL, std::to_wstring(dlg::GetTrackbar(dlg, IDC_HOLD)) + L" s");
}

void Apply(HWND dlg, const PersianRugSettings& s) {
    dlg::SetTrackbar(dlg, IDC_DETAIL, 6, 9, s.detail, 1);
    dlg::SetTrackbar(dlg, IDC_COLORS, 4, 64, s.colors, 4);
    dlg::SetTrackbar(dlg, IDC_SPEED, 1, 10, s.speed, 1);
    dlg::SetTrackbar(dlg, IDC_HOLD, 2, 60, s.hold, 5);
    dlg::FillCombo(dlg, IDC_PALETTE, kPalettes, 5, s.palette);
    dlg::FillCombo(dlg, IDC_LAYOUT, kLayouts, 3, s.layout);
    dlg::SetCheck(dlg, IDC_CYCLE, s.cycle);
    Mirror(dlg);
}

PersianRugSettings Collect(HWND dlg) {
    PersianRugSettings s;
    s.detail = dlg::GetTrackbar(dlg, IDC_DETAIL);
    s.colors = dlg::GetTrackbar(dlg, IDC_COLORS);
    s.speed = dlg::GetTrackbar(dlg, IDC_SPEED);
    s.hold = dlg::GetTrackbar(dlg, IDC_HOLD);
    s.palette = dlg::GetComboIndex(dlg, IDC_PALETTE);
    s.layout = dlg::GetComboIndex(dlg, IDC_LAYOUT);
    s.cycle = dlg::GetCheck(dlg, IDC_CYCLE);
    return s;
}

} // namespace

BOOL PersianRugConfigDialog(HWND dlg, UINT msg, WPARAM wParam, LPARAM) {
    switch (msg) {
    case WM_INITDIALOG: {
        Settings settings(L"PersianRug");
        Apply(dlg, PersianRugSettings::Load(settings));
        return TRUE;
    }
    case WM_HSCROLL:
        Mirror(dlg);
        return TRUE;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDOK: {
            Settings settings(L"PersianRug");
            Collect(dlg).Save(settings);
            EndDialog(dlg, IDOK);
            return TRUE;
        }
        case IDCANCEL:
            EndDialog(dlg, IDCANCEL);
            return TRUE;
        case IDC_DEFAULTS:
            Apply(dlg, PersianRugSettings{});
            return TRUE;
        }
        break;
    }
    return FALSE;
}
