#pragma once
#include "Settings.h"
#include <windows.h>
#include <cwchar>
#include <string>

namespace rs {

// Persists the parts of a LOGFONTW a saver cares about under "<prefix>Face", "<prefix>Weight",
// "<prefix>Italic" (height is a separate saver setting, in pixels, so DPI stays the saver's call).
inline LOGFONTW LoadFont(const Settings& s, const wchar_t* prefix, const LOGFONTW& def) {
    std::wstring p = prefix;
    LOGFONTW lf = def;
    std::wstring face = s.GetString((p + L"Face").c_str(), def.lfFaceName);
    wcsncpy_s(lf.lfFaceName, face.c_str(), _TRUNCATE);
    lf.lfWeight = s.GetInt((p + L"Weight").c_str(), def.lfWeight);
    lf.lfItalic = s.GetBool((p + L"Italic").c_str(), def.lfItalic != 0) ? TRUE : FALSE;
    return lf;
}

inline void SaveFont(Settings& s, const wchar_t* prefix, const LOGFONTW& lf) {
    std::wstring p = prefix;
    s.SetString((p + L"Face").c_str(), lf.lfFaceName);
    s.SetInt((p + L"Weight").c_str(), lf.lfWeight);
    s.SetBool((p + L"Italic").c_str(), lf.lfItalic != 0);
}

} // namespace rs
