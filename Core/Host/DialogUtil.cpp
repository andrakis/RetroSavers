#include "DialogUtil.h"
#include <commctrl.h>
#include <commdlg.h>
#include <cstdio>

#pragma comment(lib, "comdlg32.lib")

namespace rs::dlg {

void SetTrackbar(HWND dlg, int id, int lo, int hi, int pos, int tickFreq) {
    HWND h = GetDlgItem(dlg, id);
    SendMessageW(h, TBM_SETRANGE, TRUE, MAKELPARAM(lo, hi));
    SendMessageW(h, TBM_SETTICFREQ, tickFreq, 0);
    SendMessageW(h, TBM_SETPOS, TRUE, pos);
}

int GetTrackbar(HWND dlg, int id) {
    return static_cast<int>(SendMessageW(GetDlgItem(dlg, id), TBM_GETPOS, 0, 0));
}

void SetTrackbarPos(HWND dlg, int id, int pos) {
    SendMessageW(GetDlgItem(dlg, id), TBM_SETPOS, TRUE, pos);
}

void SetCheck(HWND dlg, int id, bool checked) { CheckDlgButton(dlg, id, checked ? BST_CHECKED : BST_UNCHECKED); }
bool GetCheck(HWND dlg, int id) { return IsDlgButtonChecked(dlg, id) == BST_CHECKED; }

void SetRadio(HWND dlg, int firstId, int lastId, int selectedId) { CheckRadioButton(dlg, firstId, lastId, selectedId); }

int GetRadio(HWND dlg, int firstId, int lastId) {
    for (int id = firstId; id <= lastId; ++id)
        if (IsDlgButtonChecked(dlg, id) == BST_CHECKED) return id;
    return firstId;
}

void FillCombo(HWND dlg, int id, const wchar_t* const* items, int count, int selected) {
    HWND h = GetDlgItem(dlg, id);
    SendMessageW(h, CB_RESETCONTENT, 0, 0);
    for (int i = 0; i < count; ++i) SendMessageW(h, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(items[i]));
    SendMessageW(h, CB_SETCURSEL, selected, 0);
}

int GetComboIndex(HWND dlg, int id) {
    int i = static_cast<int>(SendMessageW(GetDlgItem(dlg, id), CB_GETCURSEL, 0, 0));
    return i < 0 ? 0 : i;
}

void SetText(HWND dlg, int id, const std::wstring& text) { SetDlgItemTextW(dlg, id, text.c_str()); }

std::wstring GetText(HWND dlg, int id) {
    wchar_t buf[MAX_PATH * 2]{};
    GetDlgItemTextW(dlg, id, buf, static_cast<int>(std::size(buf)));
    return buf;
}

void SetIntText(HWND dlg, int id, int value) { SetDlgItemInt(dlg, id, static_cast<UINT>(value), TRUE); }

void MirrorTrackbar(HWND dlg, int trackbarId, int labelId, const wchar_t* prefix) {
    wchar_t buf[64];
    swprintf_s(buf, L"%s%d", prefix, GetTrackbar(dlg, trackbarId));
    SetDlgItemTextW(dlg, labelId, buf);
}

bool PickBmpFile(HWND owner, std::wstring& path) {
    wchar_t buf[MAX_PATH]{};
    wcsncpy_s(buf, path.c_str(), _TRUNCATE);
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFilter = L"Bitmap files (*.bmp)\0*.bmp\0All files (*.*)\0*.*\0";
    ofn.lpstrFile = buf;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = L"Choose a texture";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
    ofn.lpstrDefExt = L"bmp";
    if (!GetOpenFileNameW(&ofn)) return false;
    path = buf;
    return true;
}

bool PickColor(HWND owner, COLORREF& io) {
    static COLORREF custom[16] = {};
    CHOOSECOLORW cc{};
    cc.lStructSize = sizeof(cc);
    cc.hwndOwner = owner;
    cc.rgbResult = io;
    cc.lpCustColors = custom;
    cc.Flags = CC_RGBINIT | CC_FULLOPEN;
    if (!ChooseColorW(&cc)) return false;
    io = cc.rgbResult;
    return true;
}

} // namespace rs::dlg
