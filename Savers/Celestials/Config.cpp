#include <windows.h>
#include <commctrl.h>
#include "Host/DialogUtil.h"
#include "Host/Settings.h"
#include "CelestialsSaver.h"
#include "resource.h"

using namespace rs;

namespace {

// Checkbox per Kind, in Kind order.
const int kObjectIds[CelestialsSettings::kKinds] = {
    IDC_OBJ_BLACKHOLE, IDC_OBJ_BOSON, IDC_OBJ_WHITEHOLE, IDC_OBJ_TZO, IDC_OBJ_STRANGE,
    IDC_OBJ_EMBER, IDC_OBJ_REDDWARF, IDC_OBJ_SUNLIKE, IDC_OBJ_BLUEGIANT, IDC_OBJ_REDGIANT,
};

void Mirror(HWND dlg) {
    dlg::SetText(dlg, IDC_SECONDS_LABEL, std::to_wstring(dlg::GetTrackbar(dlg, IDC_SECONDS) * 5) + L" s");
    dlg::MirrorTrackbar(dlg, IDC_ORBIT, IDC_ORBIT_LABEL);
}

void Apply(HWND dlg, const CelestialsSettings& s) {
    for (int k = 0; k < CelestialsSettings::kKinds; ++k) dlg::SetCheck(dlg, kObjectIds[k], s.enabled[k]);
    dlg::SetTrackbar(dlg, IDC_SECONDS, 1, 24, s.seconds / 5, 2);
    dlg::SetTrackbar(dlg, IDC_ORBIT, 1, 10, s.orbit, 1);
    dlg::SetCheck(dlg, IDC_DISK, s.disk);
    dlg::SetCheck(dlg, IDC_WARP, s.warp);
    dlg::SetCheck(dlg, IDC_GLARE, s.reduceGlare);
    dlg::SetCheck(dlg, IDC_INFO, s.info);
    const wchar_t* const quality[] = { L"Low (50% resolution)", L"Medium (75% resolution)", L"High (full resolution)" };
    dlg::FillCombo(dlg, IDC_QUALITY, quality, 3, s.quality);
    Mirror(dlg);
}

CelestialsSettings Collect(HWND dlg) {
    CelestialsSettings s;
    for (int k = 0; k < CelestialsSettings::kKinds; ++k) s.enabled[k] = dlg::GetCheck(dlg, kObjectIds[k]);
    s.seconds = dlg::GetTrackbar(dlg, IDC_SECONDS) * 5;
    s.orbit = dlg::GetTrackbar(dlg, IDC_ORBIT);
    s.disk = dlg::GetCheck(dlg, IDC_DISK);
    s.warp = dlg::GetCheck(dlg, IDC_WARP);
    s.reduceGlare = dlg::GetCheck(dlg, IDC_GLARE);
    s.info = dlg::GetCheck(dlg, IDC_INFO);
    s.quality = dlg::GetComboIndex(dlg, IDC_QUALITY);
    return s;
}

} // namespace

BOOL CelestialsConfigDialog(HWND dlg, UINT msg, WPARAM wParam, LPARAM) {
    switch (msg) {
    case WM_INITDIALOG: {
        Settings settings(L"Celestials");
        Apply(dlg, CelestialsSettings::Load(settings));
        return TRUE;
    }
    case WM_HSCROLL:
        Mirror(dlg);
        return TRUE;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDOK: {
            CelestialsSettings s = Collect(dlg);
            if (!s.AnyEnabled()) {
                MessageBoxW(dlg, L"Pick at least one object to show.", L"Celestials", MB_OK | MB_ICONINFORMATION);
                return TRUE;
            }
            Settings settings(L"Celestials");
            s.Save(settings);
            EndDialog(dlg, IDOK);
            return TRUE;
        }
        case IDCANCEL:
            EndDialog(dlg, IDCANCEL);
            return TRUE;
        case IDC_DEFAULTS:
            Apply(dlg, CelestialsSettings{});
            return TRUE;
        }
        break;
    }
    return FALSE;
}
