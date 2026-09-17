#include <windows.h>
#include <commctrl.h>
#include "Host/DialogUtil.h"
#include "Host/Settings.h"
#include "FlyingWindowsSaver.h"
#include "resource.h"

using namespace rs;

namespace {

const int kPaneButton[4] = { IDC_PANE1, IDC_PANE2, IDC_PANE3, IDC_PANE4 };
const int kPaneSwatch[4] = { IDC_SWATCH1, IDC_SWATCH2, IDC_SWATCH3, IDC_SWATCH4 };

FlyingWindowsSettings g_current;
dlg::Swatch g_swatch[4];

void Mirror(HWND dlg) {
    dlg::MirrorTrackbar(dlg, IDC_DENSITY, IDC_DENSITY_LABEL);
    dlg::MirrorTrackbar(dlg, IDC_SPEED, IDC_SPEED_LABEL);
}

void EnablePanes(HWND dlg) {
    bool procedural = dlg::GetText(dlg, IDC_IMAGE_PATH).empty();
    for (int i = 0; i < 4; ++i) EnableWindow(GetDlgItem(dlg, kPaneButton[i]), procedural);
}

void Apply(HWND dlg, const FlyingWindowsSettings& s) {
    g_current = s;
    dlg::SetTrackbar(dlg, IDC_DENSITY, 10, 200, s.density, 10);
    dlg::SetTrackbar(dlg, IDC_SPEED, 1, 10, s.warpSpeed, 1);
    dlg::SetText(dlg, IDC_IMAGE_PATH, s.imagePath);
    for (int i = 0; i < 4; ++i) g_swatch[i].Set(dlg, kPaneSwatch[i], s.pane[i]);
    dlg::SetCheck(dlg, IDC_SPIN, s.spin);
    dlg::SetCheck(dlg, IDC_STARS, s.stars);
    Mirror(dlg);
    EnablePanes(dlg);
}

FlyingWindowsSettings Collect(HWND dlg) {
    FlyingWindowsSettings s = g_current;   // keeps the pane colours from the pickers
    s.density = dlg::GetTrackbar(dlg, IDC_DENSITY);
    s.warpSpeed = dlg::GetTrackbar(dlg, IDC_SPEED);
    s.imagePath = dlg::GetText(dlg, IDC_IMAGE_PATH);
    s.spin = dlg::GetCheck(dlg, IDC_SPIN);
    s.stars = dlg::GetCheck(dlg, IDC_STARS);
    return s;
}

} // namespace

BOOL FlyingWindowsConfigDialog(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_INITDIALOG: {
        Settings settings(L"FlyingWindows");
        Apply(dlg, FlyingWindowsSettings::Load(settings));
        return TRUE;
    }
    case WM_HSCROLL:
        Mirror(dlg);
        return TRUE;
    case WM_CTLCOLORSTATIC: {
        HWND ctl = reinterpret_cast<HWND>(lParam);
        for (int i = 0; i < 4; ++i)
            if (ctl == GetDlgItem(dlg, kPaneSwatch[i])) return static_cast<BOOL>(reinterpret_cast<INT_PTR>(g_swatch[i].Brush()));
        return FALSE;
    }
    case WM_COMMAND: {
        int id = LOWORD(wParam);
        if (id == IDC_IMAGE_PATH && HIWORD(wParam) == EN_CHANGE) { EnablePanes(dlg); return TRUE; }
        for (int i = 0; i < 4; ++i) {
            if (id == kPaneButton[i]) {
                if (dlg::PickColor(dlg, g_current.pane[i])) g_swatch[i].Set(dlg, kPaneSwatch[i], g_current.pane[i]);
                return TRUE;
            }
        }
        switch (id) {
        case IDC_IMAGE_BROWSE: {
            std::wstring path = dlg::GetText(dlg, IDC_IMAGE_PATH);
            if (dlg::PickImageFile(dlg, path)) dlg::SetText(dlg, IDC_IMAGE_PATH, path);
            return TRUE;
        }
        case IDC_IMAGE_CLEAR:
            dlg::SetText(dlg, IDC_IMAGE_PATH, L"");
            return TRUE;
        case IDOK: {
            Settings settings(L"FlyingWindows");
            Collect(dlg).Save(settings);
            EndDialog(dlg, IDOK);
            return TRUE;
        }
        case IDCANCEL:
            EndDialog(dlg, IDCANCEL);
            return TRUE;
        case IDC_DEFAULTS:
            Apply(dlg, FlyingWindowsSettings{});
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
