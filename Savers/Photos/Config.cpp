#include <windows.h>
#include <commctrl.h>
#include "Host/DialogUtil.h"
#include "Host/Settings.h"
#include "PhotosSaver.h"
#include "resource.h"

using namespace rs;

namespace {

const wchar_t* const kTransitions[] = { L"Crossfade", L"Cut", L"Fade through black" };
const wchar_t* const kFits[] = { L"Fit whole picture", L"Fill the screen" };

void Mirror(HWND dlg) {
    dlg::SetText(dlg, IDC_INTERVAL_LABEL, std::to_wstring(dlg::GetTrackbar(dlg, IDC_INTERVAL)) + L" s");
}

void Apply(HWND dlg, const PhotosSettings& s) {
    dlg::SetText(dlg, IDC_FOLDER, s.folder);
    dlg::SetTrackbar(dlg, IDC_INTERVAL, 3, 60, s.interval, 5);
    dlg::FillCombo(dlg, IDC_TRANSITION, kTransitions, 3, s.transition);
    dlg::FillCombo(dlg, IDC_FIT, kFits, 2, s.fit);
    dlg::SetCheck(dlg, IDC_SHUFFLE, s.shuffle);
    dlg::SetCheck(dlg, IDC_SUBFOLDERS, s.subfolders);
    dlg::SetCheck(dlg, IDC_FILENAME, s.showFileName);
    dlg::SetCheck(dlg, IDC_KENBURNS, s.kenBurns);
    Mirror(dlg);
}

PhotosSettings Collect(HWND dlg) {
    PhotosSettings s;
    s.folder = dlg::GetText(dlg, IDC_FOLDER);
    s.interval = dlg::GetTrackbar(dlg, IDC_INTERVAL);
    s.transition = dlg::GetComboIndex(dlg, IDC_TRANSITION);
    s.fit = dlg::GetComboIndex(dlg, IDC_FIT);
    s.shuffle = dlg::GetCheck(dlg, IDC_SHUFFLE);
    s.subfolders = dlg::GetCheck(dlg, IDC_SUBFOLDERS);
    s.showFileName = dlg::GetCheck(dlg, IDC_FILENAME);
    s.kenBurns = dlg::GetCheck(dlg, IDC_KENBURNS);
    return s;
}

} // namespace

BOOL PhotosConfigDialog(HWND dlg, UINT msg, WPARAM wParam, LPARAM) {
    switch (msg) {
    case WM_INITDIALOG: {
        Settings settings(L"Photos");
        PhotosSettings s = PhotosSettings::Load(settings);
        if (s.folder.empty()) s.folder = PhotosSettings::DefaultFolder();
        Apply(dlg, s);
        return TRUE;
    }
    case WM_HSCROLL:
        Mirror(dlg);
        return TRUE;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_BROWSE: {
            std::wstring path = dlg::GetText(dlg, IDC_FOLDER);
            if (dlg::PickFolder(dlg, path)) dlg::SetText(dlg, IDC_FOLDER, path);
            return TRUE;
        }
        case IDOK: {
            Settings settings(L"Photos");
            Collect(dlg).Save(settings);
            EndDialog(dlg, IDOK);
            return TRUE;
        }
        case IDCANCEL:
            EndDialog(dlg, IDCANCEL);
            return TRUE;
        case IDC_DEFAULTS: {
            PhotosSettings s;
            s.folder = PhotosSettings::DefaultFolder();
            Apply(dlg, s);
            return TRUE;
        }
        }
        break;
    }
    return FALSE;
}
