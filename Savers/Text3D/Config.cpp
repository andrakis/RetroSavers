#include <windows.h>
#include <commctrl.h>
#include "Host/DialogUtil.h"
#include "Host/Settings.h"
#include "Text3DSaver.h"
#include "resource.h"

using namespace rs;

namespace {

const wchar_t* const kRotations[] = { L"None", L"Spin", L"See-saw", L"Wobble", L"Tumble", L"Random" };
const wchar_t* const kSurfaces[] = { L"Solid colour", L"Marble", L"Checker", L"Image file" };

Text3DSettings g_current;
dlg::Swatch g_swatch;

void Mirror(HWND dlg) {
    dlg::MirrorTrackbar(dlg, IDC_SIZE_TB, IDC_SIZE_LABEL);
    dlg::MirrorTrackbar(dlg, IDC_DEPTH, IDC_DEPTH_LABEL);
    dlg::MirrorTrackbar(dlg, IDC_RESOLUTION, IDC_RESOLUTION_LABEL);
    dlg::MirrorTrackbar(dlg, IDC_SPEED, IDC_SPEED_LABEL);
}

void EnableControls(HWND dlg) {
    bool time = dlg::GetCheck(dlg, IDC_SHOW_TIME);
    EnableWindow(GetDlgItem(dlg, IDC_TEXT), !time);
    EnableWindow(GetDlgItem(dlg, IDC_H12), time);
    EnableWindow(GetDlgItem(dlg, IDC_H24), time);
    bool image = dlg::GetComboIndex(dlg, IDC_SURFACE) == Text3DSettings::ImageFile;
    EnableWindow(GetDlgItem(dlg, IDC_IMAGE_PATH), image);
    EnableWindow(GetDlgItem(dlg, IDC_IMAGE_BROWSE), image);
}

void Apply(HWND dlg, const Text3DSettings& s) {
    g_current = s;
    dlg::SetText(dlg, IDC_TEXT, s.text);
    dlg::SetCheck(dlg, IDC_SHOW_TIME, s.showTime);
    dlg::SetRadio(dlg, IDC_H12, IDC_H24, s.hours24 ? IDC_H24 : IDC_H12);
    dlg::SetText(dlg, IDC_FONT_LABEL, dlg::DescribeFont(s.font));
    dlg::SetTrackbar(dlg, IDC_SIZE_TB, 1, 10, s.size, 1);
    dlg::SetTrackbar(dlg, IDC_DEPTH, 1, 10, s.depth, 1);
    dlg::SetTrackbar(dlg, IDC_RESOLUTION, 1, 10, s.resolution, 1);
    dlg::SetTrackbar(dlg, IDC_SPEED, 1, 10, s.speed, 1);
    dlg::FillCombo(dlg, IDC_ROTATION, kRotations, 6, s.rotation);
    dlg::FillCombo(dlg, IDC_SURFACE, kSurfaces, 4, s.surface);
    dlg::SetText(dlg, IDC_IMAGE_PATH, s.imagePath);
    dlg::SetCheck(dlg, IDC_CYCLE, s.cycleColors);
    g_swatch.Set(dlg, IDC_SWATCH, s.color);
    Mirror(dlg);
    EnableControls(dlg);
}

Text3DSettings Collect(HWND dlg) {
    Text3DSettings s = g_current;   // keeps the font and colour from the pickers
    s.text = dlg::GetText(dlg, IDC_TEXT);
    s.showTime = dlg::GetCheck(dlg, IDC_SHOW_TIME);
    s.hours24 = dlg::GetRadio(dlg, IDC_H12, IDC_H24) == IDC_H24;
    s.size = dlg::GetTrackbar(dlg, IDC_SIZE_TB);
    s.depth = dlg::GetTrackbar(dlg, IDC_DEPTH);
    s.resolution = dlg::GetTrackbar(dlg, IDC_RESOLUTION);
    s.speed = dlg::GetTrackbar(dlg, IDC_SPEED);
    s.rotation = dlg::GetComboIndex(dlg, IDC_ROTATION);
    s.surface = dlg::GetComboIndex(dlg, IDC_SURFACE);
    s.imagePath = dlg::GetText(dlg, IDC_IMAGE_PATH);
    s.cycleColors = dlg::GetCheck(dlg, IDC_CYCLE);
    return s;
}

} // namespace

BOOL Text3DConfigDialog(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_INITDIALOG: {
        Settings settings(L"Text3D");
        Apply(dlg, Text3DSettings::Load(settings));
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
        if (LOWORD(wParam) == IDC_SURFACE && HIWORD(wParam) == CBN_SELCHANGE) { EnableControls(dlg); return TRUE; }
        switch (LOWORD(wParam)) {
        case IDC_SHOW_TIME:
            EnableControls(dlg);
            return TRUE;
        case IDC_FONT:
            if (dlg::PickFont(dlg, g_current.font)) dlg::SetText(dlg, IDC_FONT_LABEL, dlg::DescribeFont(g_current.font));
            return TRUE;
        case IDC_COLOR:
            if (dlg::PickColor(dlg, g_current.color)) g_swatch.Set(dlg, IDC_SWATCH, g_current.color);
            return TRUE;
        case IDC_IMAGE_BROWSE: {
            std::wstring path = dlg::GetText(dlg, IDC_IMAGE_PATH);
            if (dlg::PickImageFile(dlg, path)) dlg::SetText(dlg, IDC_IMAGE_PATH, path);
            return TRUE;
        }
        case IDOK: {
            Settings settings(L"Text3D");
            Collect(dlg).Save(settings);
            EndDialog(dlg, IDOK);
            return TRUE;
        }
        case IDCANCEL:
            EndDialog(dlg, IDCANCEL);
            return TRUE;
        case IDC_DEFAULTS:
            Apply(dlg, Text3DSettings{});
            return TRUE;
        }
        break;
    case WM_DESTROY:
        g_swatch.Reset();
        break;
    }
    return FALSE;
}
