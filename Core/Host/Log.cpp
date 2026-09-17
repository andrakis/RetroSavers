#include "Log.h"
#include <windows.h>
#include <shlobj.h>
#include <cstdio>
#include <ctime>

namespace rs {

static std::wstring LogPath() {
    wchar_t* base = nullptr;
    std::wstring path;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE, nullptr, &base)) && base) {
        path = std::wstring(base) + L"\\RetroSavers";
        CoTaskMemFree(base);
        CreateDirectoryW(path.c_str(), nullptr);
        path += L"\\log.txt";
    }
    return path;
}

void LogLine(const std::wstring& line) {
    std::wstring path = LogPath();
    if (path.empty()) return;
    FILE* f = nullptr;
    if (_wfopen_s(&f, path.c_str(), L"a, ccs=UTF-8") != 0 || !f) return;
    time_t now = time(nullptr);
    tm t{};
    localtime_s(&t, &now);
    wchar_t stamp[64];
    wcsftime(stamp, 64, L"%Y-%m-%d %H:%M:%S", &t);
    fwprintf(f, L"%s  %s\n", stamp, line.c_str());
    fclose(f);
}

void LogLine(const std::string& line) {
    int n = MultiByteToWideChar(CP_UTF8, 0, line.c_str(), -1, nullptr, 0);
    std::wstring w(n > 0 ? n - 1 : 0, L'\0');
    if (n > 0) MultiByteToWideChar(CP_UTF8, 0, line.c_str(), -1, w.data(), n);
    LogLine(w);
}

} // namespace rs
