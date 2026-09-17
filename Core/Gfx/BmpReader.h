#pragma once
#include "Texture.h"
#include <optional>
#include <string>

namespace rs {

// Loads an uncompressed 24- or 32-bpp Windows BMP (the only format the original savers took).
namespace BmpReader {
std::optional<Image> Load(const std::wstring& path);
}

} // namespace rs
