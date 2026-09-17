#include <windows.h>
#include <commctrl.h>
#include "Host/DialogUtil.h"
#include "Host/Settings.h"
#include "FlyingObjectsSaver.h"
#include "resource.h"

using namespace rs;

namespace {

const wchar_t* const kStyles[] = { L"Ribbon", L"Two ribbons", L"Twist", L"Splash", L"Explode", L"Textured flag", L"Logo", L"Cycle through all" };
const wchar_t* const kColors[] = { L"Rainbow", L"Solid colour", L"Checker" };

void Mirror(HWND dlg) {
    dlg::MirrorTrackbar(dlg, IDC_RESOLUTION, IDC_RESOLUTION_LABEL);
    dlg::MirrorTrackbar(dlg, IDC_SPEED, IDC_SPEED_LABEL);
    dlg::MirrorTrackbar(dlg, IDC_SIZE_TB, IDC_SIZE_LABEL);
}

void Apply(HWND dlg, const FlyingObjectsSettings& s) {
    dlg::FillCombo(dlg, IDC_STYLE, kStyles, 8, s.style);
    dlg::FillCombo(dlg, IDC_COLOR_MODE, kColors, 3, s.colorMode);
    dlg::SetText(dlg, IDC_TEXTURE_PATH, s.texturePath);
    dlg::SetTrackbar(dlg, IDC_RESOLUTION, 1, 10, s.resolution, 1);
    dlg::SetTrackbar(dlg, IDC_SPEED, 1, 10, s.speed, 1);
    dlg::SetTrackbar(dlg, IDC_SIZE_TB, 1, 10, s.size, 1);
    dlg::SetCheck(dlg, IDC_WIREFRAME, s.wireframe);
    Mirror(dlg);
}

FlyingObjectsSettings Collect(HWND dlg) {
    FlyingObjectsSettings s;
    s.style = dlg::GetComboIndex(dlg, IDC_STYLE);
    s.colorMode = dlg::GetComboIndex(dlg, IDC_COLOR_MODE);
    s.texturePath = dlg::GetText(dlg, IDC_TEXTURE_PATH);
    s.resolution = dlg::GetTrackbar(dlg, IDC_RESOLUTION);
    s.speed = dlg::GetTrackbar(dlg, IDC_SPEED);
    s.size = dlg::GetTrackbar(dlg, IDC_SIZE_TB);
    s.wireframe = dlg::GetCheck(dlg, IDC_WIREFRAME);
    return s;
}

} // namespace

BOOL FlyingObjectsConfigDialog(HWND dlg, UINT msg, WPARAM wParam, LPARAM) {
    switch (msg) {
    case WM_INITDIALOG: {
        Settings settings(L"FlyingObjects");
        Apply(dlg, FlyingObjectsSettings::Load(settings));
        return TRUE;
    }
    case WM_HSCROLL:
        Mirror(dlg);
        return TRUE;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_TEXTURE_BROWSE: {
            std::wstring path = dlg::GetText(dlg, IDC_TEXTURE_PATH);
            if (dlg::PickImageFile(dlg, path)) dlg::SetText(dlg, IDC_TEXTURE_PATH, path);
            return TRUE;
        }
        case IDOK: {
            Settings settings(L"FlyingObjects");
            Collect(dlg).Save(settings);
            EndDialog(dlg, IDOK);
            return TRUE;
        }
        case IDCANCEL:
            EndDialog(dlg, IDCANCEL);
            return TRUE;
        case IDC_DEFAULTS:
            Apply(dlg, FlyingObjectsSettings{});
            return TRUE;
        }
        break;
    }
    return FALSE;
}
