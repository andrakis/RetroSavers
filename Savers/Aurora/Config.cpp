#include <windows.h>
#include <commctrl.h>
#include "Host/DialogUtil.h"
#include "Host/Settings.h"
#include "AuroraSaver.h"
#include "resource.h"

using namespace rs;

static void Mirror(HWND dlg) {
    dlg::MirrorTrackbar(dlg, IDC_SPEED, IDC_SPEED_LABEL);
    dlg::MirrorTrackbar(dlg, IDC_BRIGHTNESS, IDC_BRIGHTNESS_LABEL);
    dlg::MirrorTrackbar(dlg, IDC_AMPLITUDE, IDC_AMPLITUDE_LABEL);
    dlg::MirrorTrackbar(dlg, IDC_LAYERS, IDC_LAYERS_LABEL);
}

static void Apply(HWND dlg, const AuroraSettings& s) {
    dlg::SetTrackbar(dlg, IDC_SPEED, 1, 10, s.speed, 1);
    dlg::SetTrackbar(dlg, IDC_BRIGHTNESS, 1, 10, s.brightness, 1);
    dlg::SetTrackbar(dlg, IDC_AMPLITUDE, 1, 10, s.amplitude, 1);
    dlg::SetTrackbar(dlg, IDC_LAYERS, 1, 10, s.layers, 1);
    Mirror(dlg);
}

BOOL AuroraConfigDialog(HWND dlg, UINT msg, WPARAM wParam, LPARAM) {
    switch (msg) {
    case WM_INITDIALOG: {
        Settings settings(L"Aurora");
        Apply(dlg, AuroraSettings::Load(settings));
        return TRUE;
    }
    case WM_HSCROLL:
        Mirror(dlg);
        return TRUE;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDOK: {
            AuroraSettings s;
            s.speed = dlg::GetTrackbar(dlg, IDC_SPEED);
            s.brightness = dlg::GetTrackbar(dlg, IDC_BRIGHTNESS);
            s.amplitude = dlg::GetTrackbar(dlg, IDC_AMPLITUDE);
            s.layers = dlg::GetTrackbar(dlg, IDC_LAYERS);
            Settings settings(L"Aurora");
            s.Save(settings);
            EndDialog(dlg, IDOK);
            return TRUE;
        }
        case IDCANCEL:
            EndDialog(dlg, IDCANCEL);
            return TRUE;
        case IDC_DEFAULTS:
            Apply(dlg, AuroraSettings{});
            return TRUE;
        }
        break;
    }
    return FALSE;
}
