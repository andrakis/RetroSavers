#pragma once
#include "Gfx/Texture.h"
#include <optional>

namespace rs {

// Grabs the virtual screen with GDI. Call from WM_CREATE (before the saver window is
// shown) so the capture holds the desktop the saver is about to cover. Returns an
// image in virtual-screen space; SaverContext::viewport indexes straight into it.
std::optional<Image> CaptureDesktop();

} // namespace rs
