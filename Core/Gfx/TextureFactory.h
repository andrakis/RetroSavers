#pragma once
#include "Texture.h"
#include <windows.h>
#include <functional>
#include <string>

namespace rs {

// Procedural images so the savers need no texture files on disk.
namespace TextureFactory {

Image Brick(int w = 256, int h = 256, uint32_t seed = 1);
Image WoodPlanks(int w = 256, int h = 256, uint32_t seed = 2);
Image CeilingTiles(int w = 256, int h = 256, uint32_t seed = 3);
Image Checker(int w = 256, int h = 256, int cells = 8, uint32_t a = 0xFFFFFFFFu, uint32_t b = 0xFF000000u);
Image Noise(int w = 256, int h = 256, uint32_t seed = 4, float scale = 8.0f);
Image Marble(int w = 256, int h = 256, uint32_t seed = 5);   // pipes "textured" default
Image Globe(int w = 256, int h = 128, uint32_t seed = 6);    // blue/green planet map

// GDI-drawn sprites. Magenta (255,0,255) is the transparency key.
Image Smiley(int size = 128);
Image Door(int size = 128);
Image Rock(int size = 128);
Image Rat(int size = 128);
Image LogoText(const std::wstring& text, int w = 256, int h = 128, COLORREF fg = RGB(255, 255, 255), COLORREF bg = RGB(0, 80, 160));

// Draw arbitrary GDI content into an image; the callback receives an HDC of w x h.
// Pixels equal to keyColor become fully transparent.
Image Gdi(int w, int h, COLORREF fill, const std::function<void(HDC)>& draw, COLORREF keyColor = RGB(255, 0, 255), bool useKey = true);

// Value noise in [0,1] (deterministic).
float ValueNoise(float x, float y, uint32_t seed);
float Fbm(float x, float y, uint32_t seed, int octaves = 4);

} // namespace TextureFactory

} // namespace rs
