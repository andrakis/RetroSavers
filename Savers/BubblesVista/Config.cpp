#include <windows.h>
#include <commctrl.h>
#include "Host/DialogUtil.h"
#include "Host/Settings.h"
#include "BubblesVistaSaver.h"
#include "resource.h"

using namespace rs;

namespace {

const wchar_t* const kColorModes[] = { L"Iridescent", L"Single colour", L"One hue per bubble" };

BubblesVistaSettings g_current;
dlg::Swatch g_swatch;

void Mirror(HWND dlg) {
    dlg::MirrorTrackbar(dlg, IDC_COUNT, IDC_COUNT_LABEL);
    dlg::MirrorTrackbar(dlg, IDC_SIZE_TB, IDC_SIZE_LABEL);
    dlg::MirrorTrackbar(dlg, IDC_SPEED, IDC_SPEED_LABEL);
}

void EnableColor(HWND dlg) {
    EnableWindow(GetDlgItem(dlg, IDC_COLOR), dlg::GetComboIndex(dlg, IDC_COLOR_MODE) == BubblesVistaSettings::SolidColor);
}

void Apply(HWND dlg, const BubblesVistaSettings& s) {
    g_current = s;
    dlg::SetTrackbar(dlg, IDC_COUNT, 1, 40, s.count, 5);
    dlg::SetTrackbar(dlg, IDC_SIZE_TB, 1, 10, s.size, 1);
    dlg::SetTrackbar(dlg, IDC_SPEED, 1, 10, s.speed, 1);
    dlg::SetCheck(dlg, IDC_DESKTOP, s.showOnDesktop);
    dlg::FillCombo(dlg, IDC_COLOR_MODE, kColorModes, 3, s.colorMode);
    dlg::SetCheck(dlg, IDC_WOBBLE, s.wobble);
    g_swatch.Set(dlg, IDC_SWATCH, s.color);
    Mirror(dlg);
    EnableColor(dlg);
}

BubblesVistaSettings Collect(HWND dlg) {
    BubblesVistaSettings s = g_current;   // keeps the colour from the picker
    s.count = dlg::GetTrackbar(dlg, IDC_COUNT);
    s.size = dlg::GetTrackbar(dlg, IDC_SIZE_TB);
    s.speed = dlg::GetTrackbar(dlg, IDC_SPEED);
    s.showOnDesktop = dlg::GetCheck(dlg, IDC_DESKTOP);
    s.colorMode = dlg::GetComboIndex(dlg, IDC_COLOR_MODE);
    s.wobble = dlg::GetCheck(dlg, IDC_WOBBLE);
    return s;
}

} // namespace

BOOL BubblesVistaConfigDialog(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_INITDIALOG: {
        Settings settings(L"Bubbles");
        Apply(dlg, BubblesVistaSettings::Load(settings));
        return TRUE;
    }
    case WM_HSCROLL:
        Mirror(dlg);
        return TRUE;
    case WM_CTLCOLORSTATIC:
        if (reinterpret_cast<HWND>(lParam) == GetDlgItem(dlg, IDC_SWATCH))
            return static_cast<BOOL>(reinterpret_cast<INT_PTR>(g_swatch.Brush()));
        return FALSE;
    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_COLOR_MODE && HIWORD(wParam) == CBN_SELCHANGE) { EnableColor(dlg); return TRUE; }
        switch (LOWORD(wParam)) {
        case IDC_COLOR:
            if (dlg::PickColor(dlg, g_current.color)) g_swatch.Set(dlg, IDC_SWATCH, g_current.color);
            return TRUE;
        case IDOK: {
            Settings settings(L"Bubbles");
            Collect(dlg).Save(settings);
            EndDialog(dlg, IDOK);
            return TRUE;
        }
        case IDCANCEL:
            EndDialog(dlg, IDCANCEL);
            return TRUE;
        case IDC_DEFAULTS:
            Apply(dlg, BubblesVistaSettings{});
            return TRUE;
        }
        break;
    case WM_DESTROY:
        g_swatch.Reset();
        break;
    }
    return FALSE;
}
