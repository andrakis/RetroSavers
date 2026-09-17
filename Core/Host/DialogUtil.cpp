#include "DialogUtil.h"
#include <commctrl.h>
#include <commdlg.h>
#include <shobjidl.h>
#include <wrl/client.h>
#include <cstdio>

#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")

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

namespace {

// IFileDialog needs COM on the calling thread; scrnsave.lib's WinMain does not initialise it.
struct ComScope {
    HRESULT hr;
    ComScope() : hr(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE)) {}
    ~ComScope() { if (SUCCEEDED(hr)) CoUninitialize(); }
    bool Usable() const { return SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE; }
};

bool RunFileDialog(HWND owner, std::wstring& path, bool folder, const COMDLG_FILTERSPEC* filters, UINT filterCount, const wchar_t* title) {
    ComScope com;
    if (!com.Usable()) return false;
    Microsoft::WRL::ComPtr<IFileOpenDialog> dlg;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg)))) return false;
    DWORD opts = 0;
    dlg->GetOptions(&opts);
    opts |= FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST | FOS_NOCHANGEDIR;
    if (folder) opts |= FOS_PICKFOLDERS;
    else opts |= FOS_FILEMUSTEXIST;
    dlg->SetOptions(opts);
    dlg->SetTitle(title);
    if (filters && filterCount) {
        dlg->SetFileTypes(filterCount, filters);
        dlg->SetFileTypeIndex(1);
    }
    if (!path.empty()) {
        // Start in the current selection's folder when it exists.
        std::wstring dir = path;
        if (!folder) {
            size_t slash = dir.find_last_of(L"\\/");
            dir = slash == std::wstring::npos ? L"" : dir.substr(0, slash);
        }
        Microsoft::WRL::ComPtr<IShellItem> item;
        if (!dir.empty() && SUCCEEDED(SHCreateItemFromParsingName(dir.c_str(), nullptr, IID_PPV_ARGS(&item))))
            dlg->SetFolder(item.Get());
    }
    if (FAILED(dlg->Show(owner))) return false;
    Microsoft::WRL::ComPtr<IShellItem> result;
    if (FAILED(dlg->GetResult(&result))) return false;
    PWSTR name = nullptr;
    if (FAILED(result->GetDisplayName(SIGDN_FILESYSPATH, &name)) || !name) return false;
    path = name;
    CoTaskMemFree(name);
    return true;
}

} // namespace

bool PickImageFile(HWND owner, std::wstring& path) {
    static const COMDLG_FILTERSPEC filters[] = {
        { L"Image files", L"*.png;*.jpg;*.jpeg;*.bmp;*.gif;*.tif;*.tiff;*.webp;*.ico;*.heic" },
        { L"All files", L"*.*" },
    };
    return RunFileDialog(owner, path, false, filters, 2, L"Choose an image");
}

bool PickFolder(HWND owner, std::wstring& path) {
    return RunFileDialog(owner, path, true, nullptr, 0, L"Choose a folder");
}

bool PickFont(HWND owner, LOGFONTW& io) {
    LOGFONTW lf = io;
    CHOOSEFONTW cf{};
    cf.lStructSize = sizeof(cf);
    cf.hwndOwner = owner;
    cf.lpLogFont = &lf;
    cf.Flags = CF_SCREENFONTS | CF_INITTOLOGFONTSTRUCT | CF_NOSCRIPTSEL | CF_FORCEFONTEXIST;
    if (!ChooseFontW(&cf)) return false;
    // The dialog returns lfHeight in device units for the screen DPI; callers keep their own size
    // slider, so only carry over the face and style.
    wcsncpy_s(io.lfFaceName, lf.lfFaceName, _TRUNCATE);
    io.lfWeight = lf.lfWeight;
    io.lfItalic = lf.lfItalic;
    io.lfUnderline = lf.lfUnderline;
    io.lfStrikeOut = lf.lfStrikeOut;
    io.lfCharSet = lf.lfCharSet;
    io.lfPitchAndFamily = lf.lfPitchAndFamily;
    return true;
}

std::wstring DescribeFont(const LOGFONTW& lf) {
    std::wstring s = lf.lfFaceName;
    if (lf.lfWeight >= FW_BOLD) s += L" Bold";
    if (lf.lfItalic) s += L" Italic";
    return s;
}

Swatch::~Swatch() { Reset(); }

void Swatch::Reset() {
    if (m_brush) DeleteObject(m_brush);
    m_brush = nullptr;
}

void Swatch::Set(HWND dlg, int id, COLORREF color) {
    Reset();
    m_brush = CreateSolidBrush(color);
    InvalidateRect(GetDlgItem(dlg, id), nullptr, TRUE);
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
