#pragma once
#include "Mesh.h"
#include <windows.h>
#include <string>

namespace rs::FontMesh {

// Extruded 3D text. Glyph outlines come from DirectWrite for the given LOGFONTW (face, weight,
// italic), are unioned and flattened by Direct2D, tessellated for the front/back caps and
// walked as contours for the sides. Units: the font's em height is `emSize`; the extrusion is
// `depth` along +Z with the front cap at z = -depth/2 facing -Z (towards a camera on -Z). The
// mesh is centred on its bounding box. `tolerance` is the D2D flattening tolerance in em units
// (smaller = smoother curves, more triangles). Side normals are smoothed across shallow corners.
// Returns an empty mesh if the text has no outline (blank) or the font could not be resolved.
MeshData Build(const std::wstring& text, const LOGFONTW& font, float emSize = 1.0f, float depth = 0.25f, float tolerance = 0.01f);

} // namespace rs::FontMesh
