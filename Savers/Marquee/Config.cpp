#include <windows.h>
#include <commctrl.h>
#include "Host/DialogUtil.h"
#include "Host/Settings.h"
#include "MarqueeSaver.h"
#include "resource.h"

using namespace rs;

namespace {

MarqueeSettings g_current;
dlg::Swatch g_textSwatch, g_bgSwatch;

void Mirror(HWND dlg) {
    dlg::MirrorTrackbar(dlg, IDC_SIZE_TB, IDC_SIZE_LABEL);
    dlg::MirrorTrackbar(dlg, IDC_SPEED, IDC_SPEED_LABEL);
}

void Apply(HWND dlg, const MarqueeSettings& s) {
    g_current = s;
    dlg::SetText(dlg, IDC_TEXT, s.text);
    dlg::SetText(dlg, IDC_FONT_LABEL, dlg::DescribeFont(s.font));
    dlg::SetTrackbar(dlg, IDC_SIZE_TB, 8, 200, s.size, 16);
    dlg::SetTrackbar(dlg, IDC_SPEED, 1, 10, s.speed, 1);
    dlg::SetRadio(dlg, IDC_POS_RANDOM, IDC_POS_CENTRED, s.position == MarqueeSettings::Centred ? IDC_POS_CENTRED : IDC_POS_RANDOM);
    dlg::SetCheck(dlg, IDC_MIRROR, s.mirror);
    g_textSwatch.Set(dlg, IDC_TEXT_SWATCH, s.textColor);
    g_bgSwatch.Set(dlg, IDC_BG_SWATCH, s.background);
    Mirror(dlg);
}

MarqueeSettings Collect(HWND dlg) {
    MarqueeSettings s = g_current;   // keeps the font and colours from the pickers
    s.text = dlg::GetText(dlg, IDC_TEXT);
    s.size = dlg::GetTrackbar(dlg, IDC_SIZE_TB);
    s.speed = dlg::GetTrackbar(dlg, IDC_SPEED);
    s.position = dlg::GetRadio(dlg, IDC_POS_RANDOM, IDC_POS_CENTRED) == IDC_POS_CENTRED ? MarqueeSettings::Centred : MarqueeSettings::RandomPos;
    s.mirror = dlg::GetCheck(dlg, IDC_MIRROR);
    return s;
}

} // namespace

BOOL MarqueeConfigDialog(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_INITDIALOG: {
        Settings settings(L"Marquee");
        Apply(dlg, MarqueeSettings::Load(settings));
        return TRUE;
    }
    case WM_HSCROLL:
        Mirror(dlg);
        return TRUE;
    case WM_CTLCOLORSTATIC: {
        HWND ctl = reinterpret_cast<HWND>(lParam);
        if (ctl == GetDlgItem(dlg, IDC_TEXT_SWATCH)) return static_cast<BOOL>(reinterpret_cast<INT_PTR>(g_textSwatch.Brush()));
        if (ctl == GetDlgItem(dlg, IDC_BG_SWATCH)) return static_cast<BOOL>(reinterpret_cast<INT_PTR>(g_bgSwatch.Brush()));
        return FALSE;
    }
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_FONT:
            if (dlg::PickFont(dlg, g_current.font)) dlg::SetText(dlg, IDC_FONT_LABEL, dlg::DescribeFont(g_current.font));
            return TRUE;
        case IDC_TEXT_COLOR:
            if (dlg::PickColor(dlg, g_current.textColor)) g_textSwatch.Set(dlg, IDC_TEXT_SWATCH, g_current.textColor);
            return TRUE;
        case IDC_BG_COLOR:
            if (dlg::PickColor(dlg, g_current.background)) g_bgSwatch.Set(dlg, IDC_BG_SWATCH, g_current.background);
            return TRUE;
        case IDOK: {
            Settings settings(L"Marquee");
            Collect(dlg).Save(settings);
            EndDialog(dlg, IDOK);
            return TRUE;
        }
        case IDCANCEL:
            EndDialog(dlg, IDCANCEL);
            return TRUE;
        case IDC_DEFAULTS:
            Apply(dlg, MarqueeSettings{});
            return TRUE;
        }
        break;
    case WM_DESTROY:
        g_textSwatch.Reset();
        g_bgSwatch.Reset();
        break;
    }
    return FALSE;
}
