#include <windows.h>
#include <commctrl.h>
#include "Host/DialogUtil.h"
#include "Host/Settings.h"
#include "LifeSaver.h"
#include "resource.h"

using namespace rs;

namespace {

const wchar_t* const kPalettes[] = { L"Classic green", L"Hues by age", L"White", L"Amber terminal" };

void Mirror(HWND dlg) {
    dlg::MirrorTrackbar(dlg, IDC_CELL, IDC_CELL_LABEL);
    dlg::MirrorTrackbar(dlg, IDC_TICK, IDC_TICK_LABEL);
    dlg::SetText(dlg, IDC_DENSITY_LABEL, std::to_wstring(dlg::GetTrackbar(dlg, IDC_DENSITY)) + L" %");
}

void Apply(HWND dlg, const LifeSettings& s) {
    dlg::SetTrackbar(dlg, IDC_CELL, 2, 16, s.cellSize, 2);
    dlg::SetTrackbar(dlg, IDC_TICK, 1, 30, s.tickRate, 5);
    dlg::SetTrackbar(dlg, IDC_DENSITY, 5, 60, s.density, 5);
    dlg::FillCombo(dlg, IDC_PALETTE, kPalettes, 4, s.palette);
    dlg::SetCheck(dlg, IDC_WRAP, s.wrap);
    dlg::SetCheck(dlg, IDC_RESEED, s.reseed);
    Mirror(dlg);
}

LifeSettings Collect(HWND dlg) {
    LifeSettings s;
    s.cellSize = dlg::GetTrackbar(dlg, IDC_CELL);
    s.tickRate = dlg::GetTrackbar(dlg, IDC_TICK);
    s.density = dlg::GetTrackbar(dlg, IDC_DENSITY);
    s.palette = dlg::GetComboIndex(dlg, IDC_PALETTE);
    s.wrap = dlg::GetCheck(dlg, IDC_WRAP);
    s.reseed = dlg::GetCheck(dlg, IDC_RESEED);
    return s;
}

} // namespace

BOOL LifeConfigDialog(HWND dlg, UINT msg, WPARAM wParam, LPARAM) {
    switch (msg) {
    case WM_INITDIALOG: {
        Settings settings(L"Life");
        Apply(dlg, LifeSettings::Load(settings));
        return TRUE;
    }
    case WM_HSCROLL:
        Mirror(dlg);
        return TRUE;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDOK: {
            Settings settings(L"Life");
            Collect(dlg).Save(settings);
            EndDialog(dlg, IDOK);
            return TRUE;
        }
        case IDCANCEL:
            EndDialog(dlg, IDCANCEL);
            return TRUE;
        case IDC_DEFAULTS:
            Apply(dlg, LifeSettings{});
            return TRUE;
        }
        break;
    }
    return FALSE;
}
