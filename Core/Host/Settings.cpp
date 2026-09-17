#include "Settings.h"
#include "Util/RegKey.h"
#include <windows.h>
#include <cstdio>
#include <cwchar>

namespace rs {

Settings::Settings(const std::wstring& saverName) : m_path(L"Software\\RetroSavers\\" + saverName) {}

int Settings::GetInt(const wchar_t* name, int def) const {
    DWORD value = 0, size = sizeof(value), type = 0;
    if (RegGetValueW(HKEY_CURRENT_USER, m_path.c_str(), name, RRF_RT_REG_DWORD, &type, &value, &size) == ERROR_SUCCESS)
        return static_cast<int>(value);
    return def;
}

bool Settings::GetBool(const wchar_t* name, bool def) const { return GetInt(name, def ? 1 : 0) != 0; }

float Settings::GetFloat(const wchar_t* name, float def) const {
    std::wstring s = GetString(name, L"");
    if (s.empty()) return def;
    wchar_t* end = nullptr;
    float v = std::wcstof(s.c_str(), &end);
    return (end && end != s.c_str()) ? v : def;
}

std::wstring Settings::GetString(const wchar_t* name, const std::wstring& def) const {
    DWORD size = 0;
    if (RegGetValueW(HKEY_CURRENT_USER, m_path.c_str(), name, RRF_RT_REG_SZ, nullptr, nullptr, &size) != ERROR_SUCCESS || size == 0)
        return def;
    std::wstring buf(size / sizeof(wchar_t), L'\0');
    if (RegGetValueW(HKEY_CURRENT_USER, m_path.c_str(), name, RRF_RT_REG_SZ, nullptr, buf.data(), &size) != ERROR_SUCCESS)
        return def;
    buf.resize(wcsnlen(buf.c_str(), buf.size()));
    return buf;
}

void Settings::SetInt(const wchar_t* name, int value) {
    RegKey key(HKEY_CURRENT_USER, m_path.c_str(), true);
    if (!key.Valid()) return;
    DWORD v = static_cast<DWORD>(value);
    RegSetValueExW(key.Get(), name, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&v), sizeof(v));
}

void Settings::SetBool(const wchar_t* name, bool value) { SetInt(name, value ? 1 : 0); }

void Settings::SetFloat(const wchar_t* name, float value) {
    wchar_t buf[64];
    swprintf_s(buf, L"%.6g", value);
    SetString(name, buf);
}

void Settings::SetString(const wchar_t* name, const std::wstring& value) {
    RegKey key(HKEY_CURRENT_USER, m_path.c_str(), true);
    if (!key.Valid()) return;
    RegSetValueExW(key.Get(), name, 0, REG_SZ, reinterpret_cast<const BYTE*>(value.c_str()),
                   static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)));
}

} // namespace rs
