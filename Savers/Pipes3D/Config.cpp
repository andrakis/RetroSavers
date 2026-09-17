#include <windows.h>
#include <commctrl.h>
#include "Host/DialogUtil.h"
#include "Host/Settings.h"
#include "Pipes3DSaver.h"
#include "resource.h"

using namespace rs;

namespace {

const wchar_t* const kJoints[] = { L"Elbow", L"Ball", L"Mixed", L"Cycle" };
const wchar_t* const kSurfaces[] = { L"Solid", L"Textured" };

void Mirror(HWND dlg) {
    dlg::MirrorTrackbar(dlg, IDC_RESOLUTION, IDC_RESOLUTION_LABEL);
    dlg::MirrorTrackbar(dlg, IDC_SPEED, IDC_SPEED_LABEL);
}

void EnableTexture(HWND dlg) {
    bool textured = dlg::GetComboIndex(dlg, IDC_SURFACE) == Pipes3DSettings::Textured;
    EnableWindow(GetDlgItem(dlg, IDC_TEXTURE_PATH), textured);
    EnableWindow(GetDlgItem(dlg, IDC_TEXTURE_BROWSE), textured);
}

void Apply(HWND dlg, const Pipes3DSettings& s) {
    dlg::SetRadio(dlg, IDC_PIPES_SINGLE, IDC_PIPES_MULTIPLE, s.multiple ? IDC_PIPES_MULTIPLE : IDC_PIPES_SINGLE);
    dlg::FillCombo(dlg, IDC_JOINT, kJoints, 4, s.jointType);
    dlg::FillCombo(dlg, IDC_SURFACE, kSurfaces, 2, s.surface);
    dlg::SetText(dlg, IDC_TEXTURE_PATH, s.texturePath);
    dlg::SetTrackbar(dlg, IDC_RESOLUTION, 6, 32, s.resolution, 2);
    dlg::SetTrackbar(dlg, IDC_SPEED, 1, 10, s.speed, 1);
    dlg::SetCheck(dlg, IDC_SMOOTH, s.smoothGrowth);
    Mirror(dlg);
    EnableTexture(dlg);
}

Pipes3DSettings Collect(HWND dlg) {
    Pipes3DSettings s;
    s.multiple = dlg::GetRadio(dlg, IDC_PIPES_SINGLE, IDC_PIPES_MULTIPLE) == IDC_PIPES_MULTIPLE;
    s.jointType = dlg::GetComboIndex(dlg, IDC_JOINT);
    s.surface = dlg::GetComboIndex(dlg, IDC_SURFACE);
    s.texturePath = dlg::GetText(dlg, IDC_TEXTURE_PATH);
    s.resolution = dlg::GetTrackbar(dlg, IDC_RESOLUTION);
    s.speed = dlg::GetTrackbar(dlg, IDC_SPEED);
    s.smoothGrowth = dlg::GetCheck(dlg, IDC_SMOOTH);
    return s;
}

} // namespace

BOOL Pipes3DConfigDialog(HWND dlg, UINT msg, WPARAM wParam, LPARAM) {
    switch (msg) {
    case WM_INITDIALOG: {
        Settings settings(L"Pipes3D");
        Apply(dlg, Pipes3DSettings::Load(settings));
        return TRUE;
    }
    case WM_HSCROLL:
        Mirror(dlg);
        return TRUE;
    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_SURFACE && HIWORD(wParam) == CBN_SELCHANGE) { EnableTexture(dlg); return TRUE; }
        switch (LOWORD(wParam)) {
        case IDC_TEXTURE_BROWSE: {
            std::wstring path = dlg::GetText(dlg, IDC_TEXTURE_PATH);
            if (dlg::PickBmpFile(dlg, path)) dlg::SetText(dlg, IDC_TEXTURE_PATH, path);
            return TRUE;
        }
        case IDOK: {
            Settings settings(L"Pipes3D");
            Collect(dlg).Save(settings);
            EndDialog(dlg, IDOK);
            return TRUE;
        }
        case IDCANCEL:
            EndDialog(dlg, IDCANCEL);
            return TRUE;
        case IDC_DEFAULTS:
            Apply(dlg, Pipes3DSettings{});
            return TRUE;
        }
        break;
    }
    return FALSE;
}
