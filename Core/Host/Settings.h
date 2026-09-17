#pragma once
#include <string>

namespace rs {

// Per-saver values under HKCU\Software\RetroSavers\<SaverName>.
class Settings {
public:
    explicit Settings(const std::wstring& saverName);

    int GetInt(const wchar_t* name, int def) const;
    bool GetBool(const wchar_t* name, bool def) const;
    float GetFloat(const wchar_t* name, float def) const;
    std::wstring GetString(const wchar_t* name, const std::wstring& def) const;

    void SetInt(const wchar_t* name, int value);
    void SetBool(const wchar_t* name, bool value);
    void SetFloat(const wchar_t* name, float value);
    void SetString(const wchar_t* name, const std::wstring& value);

    const std::wstring& KeyPath() const { return m_path; }

private:
    std::wstring m_path;
};

} // namespace rs
