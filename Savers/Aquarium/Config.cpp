#include <windows.h>
#include <commctrl.h>
#include "Host/DialogUtil.h"
#include "Host/Settings.h"
#include "AquariumSaver.h"
#include "resource.h"

using namespace rs;

namespace {

const wchar_t* const kQuality[] = { L"Low", L"Medium", L"High" };

AquariumSettings g_current;   // keeps the tint chosen via the picker
dlg::Swatch g_swatch;

void Mirror(HWND dlg) {
    dlg::MirrorTrackbar(dlg, IDC_FISH, IDC_FISH_LABEL);
    dlg::MirrorTrackbar(dlg, IDC_PLANTS, IDC_PLANTS_LABEL);
    dlg::MirrorTrackbar(dlg, IDC_ROCKS, IDC_ROCKS_LABEL);
    dlg::MirrorTrackbar(dlg, IDC_CAUSTICS, IDC_CAUSTICS_LABEL);
    dlg::MirrorTrackbar(dlg, IDC_SPEED, IDC_SPEED_LABEL);
}

void Apply(HWND dlg, const AquariumSettings& s) {
    g_current = s;
    dlg::SetTrackbar(dlg, IDC_FISH, 5, 30, s.fishCount, 5);
    for (int i = 0; i < kSpeciesCount; ++i) dlg::SetCheck(dlg, IDC_SPECIES0 + i, (s.species >> i) & 1);
    dlg::SetTrackbar(dlg, IDC_PLANTS, 0, 10, s.plants, 1);
    dlg::SetTrackbar(dlg, IDC_ROCKS, 0, 8, s.rocks, 1);
    dlg::SetTrackbar(dlg, IDC_CAUSTICS, 0, 10, s.caustics, 1);
    dlg::SetTrackbar(dlg, IDC_SPEED, 1, 10, s.speed, 1);
    dlg::SetCheck(dlg, IDC_BUBBLES, s.bubbles);
    dlg::SetCheck(dlg, IDC_SHAFTS, s.shafts);
    dlg::SetCheck(dlg, IDC_ORNAMENT, s.ornament);
    dlg::FillCombo(dlg, IDC_QUALITY, kQuality, 3, s.quality);
    g_swatch.Set(dlg, IDC_TINT_SWATCH, s.waterTint);
    Mirror(dlg);
}

AquariumSettings Collect(HWND dlg) {
    AquariumSettings s = g_current;
    s.fishCount = dlg::GetTrackbar(dlg, IDC_FISH);
    s.species = 0;
    for (int i = 0; i < kSpeciesCount; ++i)
        if (dlg::GetCheck(dlg, IDC_SPECIES0 + i)) s.species |= 1 << i;
    s.plants = dlg::GetTrackbar(dlg, IDC_PLANTS);
    s.rocks = dlg::GetTrackbar(dlg, IDC_ROCKS);
    s.caustics = dlg::GetTrackbar(dlg, IDC_CAUSTICS);
    s.speed = dlg::GetTrackbar(dlg, IDC_SPEED);
    s.bubbles = dlg::GetCheck(dlg, IDC_BUBBLES);
    s.shafts = dlg::GetCheck(dlg, IDC_SHAFTS);
    s.ornament = dlg::GetCheck(dlg, IDC_ORNAMENT);
    s.quality = dlg::GetComboIndex(dlg, IDC_QUALITY);
    return s;
}

} // namespace

BOOL AquariumConfigDialog(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_INITDIALOG: {
        Settings settings(L"Aquarium");
        Apply(dlg, AquariumSettings::Load(settings));
        return TRUE;
    }
    case WM_HSCROLL:
        Mirror(dlg);
        return TRUE;
    case WM_CTLCOLORSTATIC:
        // scrnsave.lib declares the dialog proc as returning BOOL; GDI handles are 32-bit
        // significant and zero-extend correctly through EAX, so the truncation is safe.
        if (reinterpret_cast<HWND>(lParam) == GetDlgItem(dlg, IDC_TINT_SWATCH))
            return static_cast<BOOL>(reinterpret_cast<INT_PTR>(g_swatch.Brush()));
        return FALSE;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_TINT:
            if (dlg::PickColor(dlg, g_current.waterTint)) g_swatch.Set(dlg, IDC_TINT_SWATCH, g_current.waterTint);
            return TRUE;
        case IDC_SPECIES_ALL:
        case IDC_SPECIES_NONE:
            for (int i = 0; i < kSpeciesCount; ++i) dlg::SetCheck(dlg, IDC_SPECIES0 + i, LOWORD(wParam) == IDC_SPECIES_ALL);
            return TRUE;
        case IDOK: {
            Settings settings(L"Aquarium");
            Collect(dlg).Save(settings);
            EndDialog(dlg, IDOK);
            return TRUE;
        }
        case IDCANCEL:
            EndDialog(dlg, IDCANCEL);
            return TRUE;
        case IDC_DEFAULTS:
            Apply(dlg, AquariumSettings{});
            return TRUE;
        }
        break;
    case WM_DESTROY:
        g_swatch.Reset();
        break;
    }
    return FALSE;
}
