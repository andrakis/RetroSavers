#include <windows.h>
#include <commctrl.h>
#include "Host/DialogUtil.h"
#include "Host/Settings.h"
#include "RibbonsVistaSaver.h"
#include "resource.h"

using namespace rs;

namespace {

const wchar_t* const kColors[] = { L"Rainbow", L"Pastel", L"One hue per ribbon" };

void Mirror(HWND dlg) {
    dlg::MirrorTrackbar(dlg, IDC_COUNT, IDC_COUNT_LABEL);
    dlg::MirrorTrackbar(dlg, IDC_WIDTH, IDC_WIDTH_LABEL);
    dlg::MirrorTrackbar(dlg, IDC_PERSIST, IDC_PERSIST_LABEL);
    dlg::MirrorTrackbar(dlg, IDC_SPEED, IDC_SPEED_LABEL);
}

void Apply(HWND dlg, const RibbonsVistaSettings& s) {
    dlg::SetTrackbar(dlg, IDC_COUNT, 1, 10, s.count, 1);
    dlg::SetTrackbar(dlg, IDC_WIDTH, 1, 10, s.width, 1);
    dlg::SetTrackbar(dlg, IDC_PERSIST, 1, 10, s.persistence, 1);
    dlg::SetTrackbar(dlg, IDC_SPEED, 1, 10, s.speed, 1);
    dlg::FillCombo(dlg, IDC_COLOR_MODE, kColors, 3, s.colorMode);
    Mirror(dlg);
}

RibbonsVistaSettings Collect(HWND dlg) {
    RibbonsVistaSettings s;
    s.count = dlg::GetTrackbar(dlg, IDC_COUNT);
    s.width = dlg::GetTrackbar(dlg, IDC_WIDTH);
    s.persistence = dlg::GetTrackbar(dlg, IDC_PERSIST);
    s.speed = dlg::GetTrackbar(dlg, IDC_SPEED);
    s.colorMode = dlg::GetComboIndex(dlg, IDC_COLOR_MODE);
    return s;
}

} // namespace

BOOL RibbonsVistaConfigDialog(HWND dlg, UINT msg, WPARAM wParam, LPARAM) {
    switch (msg) {
    case WM_INITDIALOG: {
        Settings settings(L"Ribbons");
        Apply(dlg, RibbonsVistaSettings::Load(settings));
        return TRUE;
    }
    case WM_HSCROLL:
        Mirror(dlg);
        return TRUE;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDOK: {
            Settings settings(L"Ribbons");
            Collect(dlg).Save(settings);
            EndDialog(dlg, IDOK);
            return TRUE;
        }
        case IDCANCEL:
            EndDialog(dlg, IDCANCEL);
            return TRUE;
        case IDC_DEFAULTS:
            Apply(dlg, RibbonsVistaSettings{});
            return TRUE;
        }
        break;
    }
    return FALSE;
}
