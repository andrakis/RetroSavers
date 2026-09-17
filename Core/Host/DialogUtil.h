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

// Windows colour picker; io is updated on OK.
bool PickColor(HWND owner, COLORREF& io);

} // namespace rs::dlg
