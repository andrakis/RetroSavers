#pragma once
#include <windows.h>
#include <string>

namespace rs::dlg {

// Trackbar (msctls_trackbar32)
void SetTrackbar(HWND dlg, int id, int lo, int hi, int pos, int tickFreq = 1);
int GetTrackbar(HWND dlg, int id);
void SetTrackbarPos(HWND dlg, int id, int pos);

// Checkbox / radio
void SetCheck(HWND dlg, int id, bool checked);
bool GetCheck(HWND dlg, int id);
// Selects one radio in a group and clears the rest (ids are consecutive from firstId to lastId).
void SetRadio(HWND dlg, int firstId, int lastId, int selectedId);
int GetRadio(HWND dlg, int firstId, int lastId);  // returns selected id or firstId

// Combo box
void FillCombo(HWND dlg, int id, const wchar_t* const* items, int count, int selected);
int GetComboIndex(HWND dlg, int id);

// Edit / static text
void SetText(HWND dlg, int id, const std::wstring& text);
std::wstring GetText(HWND dlg, int id);
void SetIntText(HWND dlg, int id, int value);

// Shows a label that mirrors a trackbar value ("Lines: 5"). Call from WM_HSCROLL too.
void MirrorTrackbar(HWND dlg, int trackbarId, int labelId, const wchar_t* prefix = L"");

// Modal *.bmp picker. Returns false if cancelled.
bool PickBmpFile(HWND owner, std::wstring& path);

// Modal picker for any WIC-decodable image (jpg/png/bmp/gif/tif/webp...). Returns false if cancelled.
bool PickImageFile(HWND owner, std::wstring& path);

// Modal folder picker (IFileDialog). Returns false if cancelled.
bool PickFolder(HWND owner, std::wstring& path);

// Windows colour picker; io is updated on OK.
bool PickColor(HWND owner, COLORREF& io);

// Windows font picker; io is updated on OK. lfHeight stays in the caller's units (pixels).
bool PickFont(HWND owner, LOGFONTW& io);

// "Segoe UI, 24 px, Bold Italic" for a label beside a font button.
std::wstring DescribeFont(const LOGFONTW& lf);

// Colour swatch helpers for LTEXT controls with SS_SUNKEN: keep one brush per swatch, refresh
// after PickColor, return it from WM_CTLCOLORSTATIC and free them on WM_DESTROY.
class Swatch {
public:
    ~Swatch();
    void Set(HWND dlg, int id, COLORREF color);          // (re)creates the brush and repaints
    HBRUSH Brush() const { return m_brush; }
    void Reset();
private:
    HBRUSH m_brush = nullptr;
};

} // namespace rs::dlg
