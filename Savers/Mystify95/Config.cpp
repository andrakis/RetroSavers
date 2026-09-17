#include <windows.h>
#include <commctrl.h>
#include "Host/DialogUtil.h"
#include "Host/Settings.h"
#include "Mystify95Saver.h"
#include "resource.h"

using namespace rs;

namespace {

struct ShapeIds { int active, lines, linesLabel, two, random, color1, swatch1, color2, swatch2; };
const ShapeIds kIds[2] = {
    { IDC_S1_ACTIVE, IDC_S1_LINES, IDC_S1_LINES_LABEL, IDC_S1_TWO, IDC_S1_RANDOM, IDC_S1_COLOR1, IDC_S1_SWATCH1, IDC_S1_COLOR2, IDC_S1_SWATCH2 },
    { IDC_S2_ACTIVE, IDC_S2_LINES, IDC_S2_LINES_LABEL, IDC_S2_TWO, IDC_S2_RANDOM, IDC_S2_COLOR1, IDC_S2_SWATCH1, IDC_S2_COLOR2, IDC_S2_SWATCH2 },
};

Mystify95Settings g_current;
HBRUSH g_swatchBrush[2][2]{};

void RefreshSwatches(HWND dlg) {
    for (int i = 0; i < 2; ++i) {
        COLORREF c[2] = { g_current.shape[i].color1, g_current.shape[i].color2 };
        for (int k = 0; k < 2; ++k) {
            if (g_swatchBrush[i][k]) DeleteObject(g_swatchBrush[i][k]);
            g_swatchBrush[i][k] = CreateSolidBrush(c[k]);
        }
        InvalidateRect(GetDlgItem(dlg, kIds[i].swatch1), nullptr, TRUE);
        InvalidateRect(GetDlgItem(dlg, kIds[i].swatch2), nullptr, TRUE);
    }
}

void EnableColorButtons(HWND dlg) {
    for (int i = 0; i < 2; ++i) {
        bool two = dlg::GetCheck(dlg, kIds[i].two);
        EnableWindow(GetDlgItem(dlg, kIds[i].color1), two);
        EnableWindow(GetDlgItem(dlg, kIds[i].color2), two);
    }
}

void Apply(HWND dlg, const Mystify95Settings& s) {
    g_current = s;
    for (int i = 0; i < 2; ++i) {
        const auto& sh = s.shape[i];
        dlg::SetCheck(dlg, kIds[i].active, sh.active);
        dlg::SetTrackbar(dlg, kIds[i].lines, 1, 15, sh.lines, 1);
        dlg::MirrorTrackbar(dlg, kIds[i].lines, kIds[i].linesLabel);
        dlg::SetRadio(dlg, kIds[i].two, kIds[i].random, sh.randomColors ? kIds[i].random : kIds[i].two);
    }
    dlg::SetCheck(dlg, IDC_CLEAR, s.clearScreen);
    RefreshSwatches(dlg);
    EnableColorButtons(dlg);
}

Mystify95Settings Collect(HWND dlg) {
    Mystify95Settings s = g_current;   // keeps the colours chosen via the pickers
    for (int i = 0; i < 2; ++i) {
        auto& sh = s.shape[i];
        sh.active = dlg::GetCheck(dlg, kIds[i].active);
        sh.lines = dlg::GetTrackbar(dlg, kIds[i].lines);
        sh.randomColors = dlg::GetCheck(dlg, kIds[i].random);
    }
    s.clearScreen = dlg::GetCheck(dlg, IDC_CLEAR);
    return s;
}

} // namespace

BOOL Mystify95ConfigDialog(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_INITDIALOG: {
        Settings settings(L"Mystify95");
        Apply(dlg, Mystify95Settings::Load(settings));
        return TRUE;
    }
    case WM_HSCROLL:
        for (int i = 0; i < 2; ++i) dlg::MirrorTrackbar(dlg, kIds[i].lines, kIds[i].linesLabel);
        return TRUE;
    case WM_CTLCOLORSTATIC: {
        // scrnsave.lib declares the dialog proc as returning BOOL; GDI handles are 32-bit
        // significant and zero-extend correctly through EAX, so the truncation is safe.
        HWND ctl = reinterpret_cast<HWND>(lParam);
        for (int i = 0; i < 2; ++i) {
            if (ctl == GetDlgItem(dlg, kIds[i].swatch1)) return static_cast<BOOL>(reinterpret_cast<INT_PTR>(g_swatchBrush[i][0]));
            if (ctl == GetDlgItem(dlg, kIds[i].swatch2)) return static_cast<BOOL>(reinterpret_cast<INT_PTR>(g_swatchBrush[i][1]));
        }
        return FALSE;
    }
    case WM_COMMAND: {
        int id = LOWORD(wParam);
        for (int i = 0; i < 2; ++i) {
            if (id == kIds[i].color1 || id == kIds[i].color2) {
                COLORREF& c = (id == kIds[i].color1) ? g_current.shape[i].color1 : g_current.shape[i].color2;
                if (dlg::PickColor(dlg, c)) RefreshSwatches(dlg);
                return TRUE;
            }
            if (id == kIds[i].two || id == kIds[i].random) { EnableColorButtons(dlg); return TRUE; }
        }
        switch (id) {
        case IDOK: {
            Settings settings(L"Mystify95");
            Collect(dlg).Save(settings);
            EndDialog(dlg, IDOK);
            return TRUE;
        }
        case IDCANCEL:
            EndDialog(dlg, IDCANCEL);
            return TRUE;
        case IDC_DEFAULTS:
            Apply(dlg, Mystify95Settings::Defaults());
            return TRUE;
        }
        break;
    }
    case WM_DESTROY:
        for (auto& row : g_swatchBrush)
            for (auto& b : row) { if (b) DeleteObject(b); b = nullptr; }
        break;
    }
    return FALSE;
}
