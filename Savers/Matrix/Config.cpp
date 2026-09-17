#include <windows.h>
#include <commctrl.h>
#include "Host/DialogUtil.h"
#include "Host/Settings.h"
#include "MatrixSaver.h"
#include "resource.h"

using namespace rs;

namespace {

const wchar_t* const kCharsets[] = { L"Katakana + digits", L"Latin + digits", L"Binary" };

MatrixSettings g_current;
dlg::Swatch g_swatch;

void Mirror(HWND dlg) {
    dlg::MirrorTrackbar(dlg, IDC_SPEED, IDC_SPEED_LABEL);
    dlg::MirrorTrackbar(dlg, IDC_DENSITY, IDC_DENSITY_LABEL);
    dlg::MirrorTrackbar(dlg, IDC_GLYPH, IDC_GLYPH_LABEL);
}

void Apply(HWND dlg, const MatrixSettings& s) {
    g_current = s;
    dlg::SetTrackbar(dlg, IDC_SPEED, 1, 10, s.speed, 1);
    dlg::SetTrackbar(dlg, IDC_DENSITY, 1, 10, s.density, 1);
    dlg::SetTrackbar(dlg, IDC_GLYPH, 8, 48, s.glyphSize, 4);
    dlg::FillCombo(dlg, IDC_CHARSET, kCharsets, 3, s.charset);
    dlg::SetCheck(dlg, IDC_BOLD, s.boldHeads);
    dlg::SetCheck(dlg, IDC_AFTERGLOW, s.afterglow);
    g_swatch.Set(dlg, IDC_SWATCH, s.color);
    Mirror(dlg);
}

MatrixSettings Collect(HWND dlg) {
    MatrixSettings s = g_current;   // keeps the colour chosen via the picker
    s.speed = dlg::GetTrackbar(dlg, IDC_SPEED);
    s.density = dlg::GetTrackbar(dlg, IDC_DENSITY);
    s.glyphSize = dlg::GetTrackbar(dlg, IDC_GLYPH);
    s.charset = dlg::GetComboIndex(dlg, IDC_CHARSET);
    s.boldHeads = dlg::GetCheck(dlg, IDC_BOLD);
    s.afterglow = dlg::GetCheck(dlg, IDC_AFTERGLOW);
    return s;
}

} // namespace

BOOL MatrixConfigDialog(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_INITDIALOG: {
        Settings settings(L"Matrix");
        Apply(dlg, MatrixSettings::Load(settings));
        return TRUE;
    }
    case WM_HSCROLL:
        Mirror(dlg);
        return TRUE;
    case WM_CTLCOLORSTATIC:
        // scrnsave.lib declares the dialog proc as returning BOOL; GDI handles are 32-bit
        // significant and zero-extend correctly through EAX, so the truncation is safe.
        if (reinterpret_cast<HWND>(lParam) == GetDlgItem(dlg, IDC_SWATCH))
            return static_cast<BOOL>(reinterpret_cast<INT_PTR>(g_swatch.Brush()));
        return FALSE;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_COLOR:
            if (dlg::PickColor(dlg, g_current.color)) g_swatch.Set(dlg, IDC_SWATCH, g_current.color);
            return TRUE;
        case IDOK: {
            Settings settings(L"Matrix");
            Collect(dlg).Save(settings);
            EndDialog(dlg, IDOK);
            return TRUE;
        }
        case IDCANCEL:
            EndDialog(dlg, IDCANCEL);
            return TRUE;
        case IDC_DEFAULTS:
            Apply(dlg, MatrixSettings{});
            return TRUE;
        }
        break;
    case WM_DESTROY:
        g_swatch.Reset();
        break;
    }
    return FALSE;
}
