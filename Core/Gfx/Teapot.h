#pragma once
#include "Mesh.h"

namespace rs {

// Tessellates the Newell teapot on the CPU. Result is Y-up, centred at the origin,
// scaled so its height is `size`, spout pointing +X.
MeshData BuildTeapot(float size, int tessellation = 8);

} // namespace rs
