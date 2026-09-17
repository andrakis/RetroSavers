#pragma once
#include "Texture.h"
#include <optional>
#include <string>

namespace rs {

// Decodes any WIC-supported image (jpg/png/gif/tif/bmp/webp/heic...) into an RGBA8 Image with
// straight alpha, honouring the EXIF orientation tag. maxDim > 0 downscales so that neither
// side exceeds it (decode-time scaling keeps large photos cheap). Falls back to BmpReader when
// WIC is unavailable. The calling thread must have COM initialised (the render thread does).
namespace ImageLoader {
std::optional<Image> Load(const std::wstring& path, int maxDim = 0);
}

} // namespace rs
