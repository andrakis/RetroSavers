#include <windows.h>
#include <commctrl.h>
#include "Host/DialogUtil.h"
#include "Host/Settings.h"
#include "FireworksSaver.h"
#include "resource.h"

using namespace rs;

namespace {

void Mirror(HWND dlg) {
    dlg::MirrorTrackbar(dlg, IDC_RATE, IDC_RATE_LABEL);
    dlg::MirrorTrackbar(dlg, IDC_GRAVITY, IDC_GRAVITY_LABEL);
    dlg::MirrorTrackbar(dlg, IDC_TRAIL, IDC_TRAIL_LABEL);
    dlg::MirrorTrackbar(dlg, IDC_BLOOM, IDC_BLOOM_LABEL);
    dlg::MirrorTrackbar(dlg, IDC_BURST, IDC_BURST_LABEL);
    int finale = dlg::GetTrackbar(dlg, IDC_FINALE);
    dlg::SetText(dlg, IDC_FINALE_LABEL, finale ? std::to_wstring(finale) + L" s" : L"never");
}

void Apply(HWND dlg, const FireworksSettings& s) {
    dlg::SetTrackbar(dlg, IDC_RATE, 1, 10, s.launchRate, 1);
    dlg::SetTrackbar(dlg, IDC_GRAVITY, 1, 10, s.gravity, 1);
    dlg::SetTrackbar(dlg, IDC_TRAIL, 1, 10, s.trail, 1);
    dlg::SetTrackbar(dlg, IDC_BLOOM, 0, 10, s.bloom, 1);
    dlg::SetTrackbar(dlg, IDC_BURST, 1, 10, s.burstSize, 1);
    dlg::SetTrackbar(dlg, IDC_FINALE, 0, 120, s.finaleEvery, 15);
    Mirror(dlg);
}

FireworksSettings Collect(HWND dlg) {
    FireworksSettings s;
    s.launchRate = dlg::GetTrackbar(dlg, IDC_RATE);
    s.gravity = dlg::GetTrackbar(dlg, IDC_GRAVITY);
    s.trail = dlg::GetTrackbar(dlg, IDC_TRAIL);
    s.bloom = dlg::GetTrackbar(dlg, IDC_BLOOM);
    s.burstSize = dlg::GetTrackbar(dlg, IDC_BURST);
    s.finaleEvery = dlg::GetTrackbar(dlg, IDC_FINALE);
    return s;
}

} // namespace

BOOL FireworksConfigDialog(HWND dlg, UINT msg, WPARAM wParam, LPARAM) {
    switch (msg) {
    case WM_INITDIALOG: {
        Settings settings(L"Fireworks");
        Apply(dlg, FireworksSettings::Load(settings));
        return TRUE;
    }
    case WM_HSCROLL:
        Mirror(dlg);
        return TRUE;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDOK: {
            Settings settings(L"Fireworks");
            Collect(dlg).Save(settings);
            EndDialog(dlg, IDOK);
            return TRUE;
        }
        case IDCANCEL:
            EndDialog(dlg, IDCANCEL);
            return TRUE;
        case IDC_DEFAULTS:
            Apply(dlg, FireworksSettings{});
            return TRUE;
        }
        break;
    }
    return FALSE;
}
