#include <windows.h>
#include <commctrl.h>
#include "Host/DialogUtil.h"
#include "Host/Settings.h"
#include "DVDBounceSaver.h"
#include "resource.h"

using namespace rs;

namespace {

const wchar_t* const kColorModes[] = { L"Random", L"Classic palette", L"Image as-is" };
const wchar_t* const kCornerRates[] = { L"Rare", L"Occasional", L"Frequent" };

DVDBounceSettings g_current;
dlg::Swatch g_swatch;

void Mirror(HWND dlg) {
    dlg::MirrorTrackbar(dlg, IDC_SIZE_TB, IDC_SIZE_LABEL, L"");
    dlg::MirrorTrackbar(dlg, IDC_SPEED, IDC_SPEED_LABEL);
}

void Apply(HWND dlg, const DVDBounceSettings& s) {
    g_current = s;
    dlg::SetText(dlg, IDC_IMAGE_PATH, s.imagePath);
    dlg::SetTrackbar(dlg, IDC_SIZE_TB, 5, 50, s.size, 5);
    dlg::SetTrackbar(dlg, IDC_SPEED, 1, 10, s.speed, 1);
    dlg::FillCombo(dlg, IDC_COLOR_MODE, kColorModes, 3, s.colorMode);
    dlg::FillCombo(dlg, IDC_CORNER_RATE, kCornerRates, 3, s.cornerRate);
    dlg::SetCheck(dlg, IDC_COUNTER, s.showCounter);
    g_swatch.Set(dlg, IDC_SWATCH, s.background);
    Mirror(dlg);
}

DVDBounceSettings Collect(HWND dlg) {
    DVDBounceSettings s = g_current;   // keeps the background colour from the picker
    s.imagePath = dlg::GetText(dlg, IDC_IMAGE_PATH);
    s.size = dlg::GetTrackbar(dlg, IDC_SIZE_TB);
    s.speed = dlg::GetTrackbar(dlg, IDC_SPEED);
    s.colorMode = dlg::GetComboIndex(dlg, IDC_COLOR_MODE);
    s.cornerRate = dlg::GetComboIndex(dlg, IDC_CORNER_RATE);
    s.showCounter = dlg::GetCheck(dlg, IDC_COUNTER);
    return s;
}

} // namespace

BOOL DVDBounceConfigDialog(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_INITDIALOG: {
        Settings settings(L"DVDBounce");
        Apply(dlg, DVDBounceSettings::Load(settings));
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
        switch (LOWORD(wParam)) {
        case IDC_IMAGE_BROWSE: {
            std::wstring path = dlg::GetText(dlg, IDC_IMAGE_PATH);
            if (dlg::PickImageFile(dlg, path)) dlg::SetText(dlg, IDC_IMAGE_PATH, path);
            return TRUE;
        }
        case IDC_IMAGE_CLEAR:
            dlg::SetText(dlg, IDC_IMAGE_PATH, L"");
            return TRUE;
        case IDC_BACKGROUND:
            if (dlg::PickColor(dlg, g_current.background)) g_swatch.Set(dlg, IDC_SWATCH, g_current.background);
            return TRUE;
        case IDOK: {
            Settings settings(L"DVDBounce");
            Collect(dlg).Save(settings);
            EndDialog(dlg, IDOK);
            return TRUE;
        }
        case IDCANCEL:
            EndDialog(dlg, IDCANCEL);
            return TRUE;
        case IDC_DEFAULTS:
            Apply(dlg, DVDBounceSettings{});
            return TRUE;
        }
        break;
    case WM_DESTROY:
        g_swatch.Reset();
        break;
    }
    return FALSE;
}
