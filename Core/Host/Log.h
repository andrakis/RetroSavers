#pragma once
#include <string>

namespace rs {

// Appends a timestamped line to %LOCALAPPDATA%\RetroSavers\log.txt. Never throws.
void LogLine(const std::wstring& line);
void LogLine(const std::string& line);

} // namespace rs
