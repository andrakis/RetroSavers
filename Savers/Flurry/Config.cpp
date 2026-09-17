#include <windows.h>
#include <commctrl.h>
#include "Host/DialogUtil.h"
#include "Host/Settings.h"
#include "FlurrySaver.h"
#include "resource.h"

using namespace rs;

namespace {

const wchar_t* const kPresets[] = { L"Classic", L"RGB", L"Fire", L"Water", L"Psychedelic", L"Binary" };

void Mirror(HWND dlg) {
    dlg::MirrorTrackbar(dlg, IDC_STREAMS, IDC_STREAMS_LABEL);
    dlg::MirrorTrackbar(dlg, IDC_SPEED, IDC_SPEED_LABEL);
    dlg::MirrorTrackbar(dlg, IDC_BRIGHTNESS, IDC_BRIGHTNESS_LABEL);
    dlg::MirrorTrackbar(dlg, IDC_TRAIL, IDC_TRAIL_LABEL);
}

void Apply(HWND dlg, const FlurrySettings& s) {
    dlg::SetTrackbar(dlg, IDC_STREAMS, 1, 12, s.streams, 1);
    dlg::SetTrackbar(dlg, IDC_SPEED, 1, 10, s.speed, 1);
    dlg::SetTrackbar(dlg, IDC_BRIGHTNESS, 1, 10, s.brightness, 1);
    dlg::SetTrackbar(dlg, IDC_TRAIL, 1, 10, s.trail, 1);
    dlg::FillCombo(dlg, IDC_PRESET, kPresets, 6, s.preset);
    dlg::SetCheck(dlg, IDC_BLOOM, s.bloom);
    Mirror(dlg);
}

FlurrySettings Collect(HWND dlg) {
    FlurrySettings s;
    s.streams = dlg::GetTrackbar(dlg, IDC_STREAMS);
    s.speed = dlg::GetTrackbar(dlg, IDC_SPEED);
    s.brightness = dlg::GetTrackbar(dlg, IDC_BRIGHTNESS);
    s.trail = dlg::GetTrackbar(dlg, IDC_TRAIL);
    s.preset = dlg::GetComboIndex(dlg, IDC_PRESET);
    s.bloom = dlg::GetCheck(dlg, IDC_BLOOM);
    return s;
}

} // namespace

BOOL FlurryConfigDialog(HWND dlg, UINT msg, WPARAM wParam, LPARAM) {
    switch (msg) {
    case WM_INITDIALOG: {
        Settings settings(L"Flurry");
        Apply(dlg, FlurrySettings::Load(settings));
        return TRUE;
    }
    case WM_HSCROLL:
        Mirror(dlg);
        return TRUE;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDOK: {
            Settings settings(L"Flurry");
            Collect(dlg).Save(settings);
            EndDialog(dlg, IDOK);
            return TRUE;
        }
        case IDCANCEL:
            EndDialog(dlg, IDCANCEL);
            return TRUE;
        case IDC_DEFAULTS:
            Apply(dlg, FlurrySettings{});
            return TRUE;
        }
        break;
    }
    return FALSE;
}
