#include <windows.h>
#include <commctrl.h>
#include "Host/DialogUtil.h"
#include "Host/Settings.h"
#include "GearsSaver.h"
#include "resource.h"

using namespace rs;

namespace {

GearsSettings g_current;
dlg::Swatch g_swatch[3];
const int kColorButtons[3] = { IDC_COLOR1, IDC_COLOR2, IDC_COLOR3 };
const int kSwatches[3] = { IDC_SWATCH1, IDC_SWATCH2, IDC_SWATCH3 };

COLORREF& ColorRef(int i) { return i == 0 ? g_current.color1 : (i == 1 ? g_current.color2 : g_current.color3); }

void Mirror(HWND dlg) {
    dlg::MirrorTrackbar(dlg, IDC_SPEED, IDC_SPEED_LABEL);
}

void Apply(HWND dlg, const GearsSettings& s) {
    g_current = s;
    dlg::SetTrackbar(dlg, IDC_SPEED, 1, 10, s.speed, 1);
    for (int i = 0; i < 3; ++i) g_swatch[i].Set(dlg, kSwatches[i], ColorRef(i));
    dlg::SetCheck(dlg, IDC_WIREFRAME, s.wireframe);
    dlg::SetCheck(dlg, IDC_AUTOROTATE, s.autoRotate);
    dlg::SetCheck(dlg, IDC_FPS, s.showFps);
    Mirror(dlg);
}

GearsSettings Collect(HWND dlg) {
    GearsSettings s = g_current;   // keeps the colours from the pickers
    s.speed = dlg::GetTrackbar(dlg, IDC_SPEED);
    s.wireframe = dlg::GetCheck(dlg, IDC_WIREFRAME);
    s.autoRotate = dlg::GetCheck(dlg, IDC_AUTOROTATE);
    s.showFps = dlg::GetCheck(dlg, IDC_FPS);
    return s;
}

} // namespace

BOOL GearsConfigDialog(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_INITDIALOG: {
        Settings settings(L"Gears");
        Apply(dlg, GearsSettings::Load(settings));
        return TRUE;
    }
    case WM_HSCROLL:
        Mirror(dlg);
        return TRUE;
    case WM_CTLCOLORSTATIC: {
        HWND ctl = reinterpret_cast<HWND>(lParam);
        for (int i = 0; i < 3; ++i)
            if (ctl == GetDlgItem(dlg, kSwatches[i])) return static_cast<BOOL>(reinterpret_cast<INT_PTR>(g_swatch[i].Brush()));
        return FALSE;
    }
    case WM_COMMAND: {
        int id = LOWORD(wParam);
        for (int i = 0; i < 3; ++i) {
            if (id == kColorButtons[i]) {
                if (dlg::PickColor(dlg, ColorRef(i))) g_swatch[i].Set(dlg, kSwatches[i], ColorRef(i));
                return TRUE;
            }
        }
        switch (id) {
        case IDOK: {
            Settings settings(L"Gears");
            Collect(dlg).Save(settings);
            EndDialog(dlg, IDOK);
            return TRUE;
        }
        case IDCANCEL:
            EndDialog(dlg, IDCANCEL);
            return TRUE;
        case IDC_DEFAULTS:
            Apply(dlg, GearsSettings{});
            return TRUE;
        }
        break;
    }
    case WM_DESTROY:
        for (auto& s : g_swatch) s.Reset();
        break;
    }
    return FALSE;
}
