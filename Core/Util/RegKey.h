#pragma once
#include <windows.h>

namespace rs {

// RAII HKEY. Opens (or creates) the key on construction; closes on destruction.
class RegKey {
public:
    RegKey() = default;
    RegKey(HKEY root, const wchar_t* subKey, bool create) {
        if (create)
            RegCreateKeyExW(root, subKey, 0, nullptr, 0, KEY_READ | KEY_WRITE, nullptr, &m_key, nullptr);
        else
            RegOpenKeyExW(root, subKey, 0, KEY_READ, &m_key);
    }
    ~RegKey() { if (m_key) RegCloseKey(m_key); }
    RegKey(const RegKey&) = delete;
    RegKey& operator=(const RegKey&) = delete;

    bool Valid() const { return m_key != nullptr; }
    HKEY Get() const { return m_key; }

private:
    HKEY m_key = nullptr;
};

} // namespace rs
