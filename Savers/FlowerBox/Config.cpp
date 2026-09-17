#include <windows.h>
#include <commctrl.h>
#include "Host/DialogUtil.h"
#include "Host/Settings.h"
#include "FlowerBoxSaver.h"
#include "resource.h"

using namespace rs;

namespace {

const wchar_t* const kShapes[] = { L"Cube", L"Sphere", L"Star", L"Cycle" };
const wchar_t* const kColors[] = { L"Colour per face", L"Checker", L"Cycling hues" };

void Mirror(HWND dlg) {
    dlg::MirrorTrackbar(dlg, IDC_COMPLEXITY, IDC_COMPLEXITY_LABEL);
    dlg::MirrorTrackbar(dlg, IDC_SPEED, IDC_SPEED_LABEL);
    dlg::MirrorTrackbar(dlg, IDC_SIZE_TB, IDC_SIZE_LABEL);
}

void Apply(HWND dlg, const FlowerBoxSettings& s) {
    dlg::SetTrackbar(dlg, IDC_COMPLEXITY, 1, 10, s.complexity, 1);
    dlg::SetTrackbar(dlg, IDC_SPEED, 1, 10, s.speed, 1);
    dlg::SetTrackbar(dlg, IDC_SIZE_TB, 1, 10, s.size, 1);
    dlg::FillCombo(dlg, IDC_SHAPE, kShapes, 4, s.shape);
    dlg::FillCombo(dlg, IDC_COLOR_MODE, kColors, 3, s.colorMode);
    dlg::SetCheck(dlg, IDC_SPIN, s.spin);
    dlg::SetCheck(dlg, IDC_BOUNCE, s.bounce);
    Mirror(dlg);
}

FlowerBoxSettings Collect(HWND dlg) {
    FlowerBoxSettings s;
    s.complexity = dlg::GetTrackbar(dlg, IDC_COMPLEXITY);
    s.speed = dlg::GetTrackbar(dlg, IDC_SPEED);
    s.size = dlg::GetTrackbar(dlg, IDC_SIZE_TB);
    s.shape = dlg::GetComboIndex(dlg, IDC_SHAPE);
    s.colorMode = dlg::GetComboIndex(dlg, IDC_COLOR_MODE);
    s.spin = dlg::GetCheck(dlg, IDC_SPIN);
    s.bounce = dlg::GetCheck(dlg, IDC_BOUNCE);
    return s;
}

} // namespace

BOOL FlowerBoxConfigDialog(HWND dlg, UINT msg, WPARAM wParam, LPARAM) {
    switch (msg) {
    case WM_INITDIALOG: {
        Settings settings(L"FlowerBox");
        Apply(dlg, FlowerBoxSettings::Load(settings));
        return TRUE;
    }
    case WM_HSCROLL:
        Mirror(dlg);
        return TRUE;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDOK: {
            Settings settings(L"FlowerBox");
            Collect(dlg).Save(settings);
            EndDialog(dlg, IDOK);
            return TRUE;
        }
        case IDCANCEL:
            EndDialog(dlg, IDCANCEL);
            return TRUE;
        case IDC_DEFAULTS:
            Apply(dlg, FlowerBoxSettings{});
            return TRUE;
        }
        break;
    }
    return FALSE;
}
