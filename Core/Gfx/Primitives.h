#pragma once
#include "Mesh.h"

namespace rs::Primitives {

// UV sphere centred at the origin.
MeshData Sphere(float radius, int slices = 16, int stacks = 12);

// Cylinder along +Z from z=0 to z=length, with end caps. u wraps around, v runs along the axis.
MeshData Cylinder(float radius, float length, int slices = 16, bool caps = true);

// 90 degree torus section: starts at the origin heading +Z, ends at (0, bend, bend) heading +Y.
// bend is the bend radius of the centreline; radius is the tube radius.
MeshData ElbowTube(float radius, float bend, int slices = 16, int segments = 8);

// Axis-aligned box centred at the origin with per-face UVs.
MeshData Box(float sx, float sy, float sz);

// Quad in the XY plane facing -Z (towards a camera at -Z looking +Z), centred, size w x h. UV (0,0) top-left.
MeshData Quad(float w, float h);

// Flat grid in the XZ plane facing +Y, centred, with uvRepeat tiling.
MeshData Grid(float w, float d, int divisionsX, int divisionsZ, float uvRepeat = 1.0f);

// The glxgears gear (a port of gears.c): centred on the origin, axis along Z, `width` thick,
// with a bore of `innerRadius`, `teeth` teeth of `toothDepth` around `outerRadius`. Flat shaded.
MeshData Gear(float innerRadius, float outerRadius, float width, int teeth, float toothDepth);

// Lumpy boulder: a sphere displaced along its normals by 3D value noise (`roughness` = fraction
// of the radius), squashed to `squash` of its height and with the bottom flattened so it sits
// on a floor at y = 0 (the top is at about y = radius * squash). Smooth normals.
MeshData Rock(float radius, uint32_t seed, float roughness = 0.35f, float squash = 0.75f, int slices = 20, int stacks = 14);

} // namespace rs::Primitives
