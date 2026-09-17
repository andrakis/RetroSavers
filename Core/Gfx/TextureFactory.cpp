#include "TextureFactory.h"
#include "Util/Color.h"
#include "Util/MathUtil.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cwchar>

#pragma comment(lib, "gdi32.lib")

namespace rs::TextureFactory {

namespace {

uint32_t Hash(uint32_t x, uint32_t y, uint32_t seed) {
    uint32_t h = x * 374761393u + y * 668265263u + seed * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

float Hash01(int x, int y, uint32_t seed) {
    return (Hash(static_cast<uint32_t>(x), static_cast<uint32_t>(y), seed) & 0xFFFFFFu) / 16777215.0f;
}

uint32_t Shade(float r, float g, float b, float k) { return PackRgbaF(r * k, g * k, b * k, 1.0f); }

} // namespace

float ValueNoise(float x, float y, uint32_t seed) {
    int xi = static_cast<int>(std::floor(x)), yi = static_cast<int>(std::floor(y));
    float fx = x - xi, fy = y - yi;
    fx = fx * fx * (3 - 2 * fx);
    fy = fy * fy * (3 - 2 * fy);
    float a = Hash01(xi, yi, seed), b = Hash01(xi + 1, yi, seed);
    float c = Hash01(xi, yi + 1, seed), d = Hash01(xi + 1, yi + 1, seed);
    return Lerp(Lerp(a, b, fx), Lerp(c, d, fx), fy);
}

float Fbm(float x, float y, uint32_t seed, int octaves) {
    float sum = 0, amp = 0.5f, norm = 0;
    for (int i = 0; i < octaves; ++i) {
        sum += ValueNoise(x, y, seed + i * 17) * amp;
        norm += amp;
        x *= 2.03f;
        y *= 1.97f;
        amp *= 0.5f;
    }
    return sum / norm;
}

Image Brick(int w, int h, uint32_t seed) {
    Image img(w, h);
    const int rows = 8;
    const int cols = 4;
    float bh = static_cast<float>(h) / rows, bw = static_cast<float>(w) / cols;
    float mortar = 2.5f;
    for (int y = 0; y < h; ++y) {
        int row = static_cast<int>(y / bh);
        float offset = (row % 2) ? bw * 0.5f : 0.0f;
        float ly = std::fmod(static_cast<float>(y), bh);
        for (int x = 0; x < w; ++x) {
            float sx = std::fmod(x + offset, bw);
            int col = static_cast<int>((x + offset) / bw);
            bool isMortar = ly < mortar || sx < mortar;
            float n = Fbm(x * 0.15f, y * 0.15f, seed, 3);
            if (isMortar) {
                float k = 0.55f + 0.25f * n;
                img.At(x, y) = Shade(0.75f, 0.72f, 0.68f, k);
            } else {
                float tint = Hash01(col, row, seed + 99);
                float r = 0.62f + 0.18f * tint, g = 0.28f + 0.10f * tint, b = 0.20f + 0.06f * tint;
                float k = 0.75f + 0.35f * n;
                img.At(x, y) = Shade(r, g, b, k);
            }
        }
    }
    return img;
}

Image WoodPlanks(int w, int h, uint32_t seed) {
    Image img(w, h);
    const int planks = 4;
    float pw = static_cast<float>(w) / planks;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int plank = static_cast<int>(x / pw);
            float sx = std::fmod(static_cast<float>(x), pw);
            float tint = Hash01(plank, 0, seed);
            float grain = ValueNoise(x * 0.08f + tint * 40.0f, y * 0.9f, seed + 7);
            float rings = 0.5f + 0.5f * std::sin((y * 0.05f + grain * 6.0f + tint * 9.0f));
            float k = 0.55f + 0.35f * rings + 0.15f * grain;
            if (sx < 2.0f) k *= 0.45f;
            float r = 0.55f + 0.15f * tint, g = 0.33f + 0.10f * tint, b = 0.16f + 0.05f * tint;
            img.At(x, y) = Shade(r, g, b, k);
        }
    }
    return img;
}

Image CeilingTiles(int w, int h, uint32_t seed) {
    Image img(w, h);
    const int tiles = 2;
    float tw = static_cast<float>(w) / tiles, th = static_cast<float>(h) / tiles;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float sx = std::fmod(static_cast<float>(x), tw), sy = std::fmod(static_cast<float>(y), th);
            bool edge = sx < 3.0f || sy < 3.0f;
            float speck = Hash01(x, y, seed);
            float n = Fbm(x * 0.3f, y * 0.3f, seed, 2);
            float k = edge ? 0.55f : (0.78f + 0.12f * n + (speck > 0.94f ? -0.25f : 0.0f));
            img.At(x, y) = Shade(0.92f, 0.92f, 0.88f, k);
        }
    }
    return img;
}

Image Checker(int w, int h, int cells, uint32_t a, uint32_t b) {
    Image img(w, h);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            img.At(x, y) = (((x * cells / w) + (y * cells / h)) & 1) ? b : a;
    return img;
}

Image Noise(int w, int h, uint32_t seed, float scale) {
    Image img(w, h);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            float n = Fbm(x * scale / w, y * scale / h, seed, 4);
            img.At(x, y) = Shade(1, 1, 1, n);
        }
    return img;
}

Image Marble(int w, int h, uint32_t seed) {
    Image img(w, h);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            float n = Fbm(x * 6.0f / w, y * 6.0f / h, seed, 5);
            float v = 0.5f + 0.5f * std::sin((x * 0.04f) + n * 9.0f);
            float k = 0.55f + 0.45f * v;
            img.At(x, y) = Shade(0.95f, 0.93f, 0.9f, k);
        }
    return img;
}

Image Globe(int w, int h, uint32_t seed) {
    Image img(w, h);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            // Tile horizontally so the seam does not show when wrapped around a sphere.
            float u = x * kTwoPi / w;
            float n = Fbm(std::cos(u) * 2.5f + 5.0f, std::sin(u) * 2.5f + y * 6.0f / h, seed, 5);
            float lat = std::fabs(y / static_cast<float>(h) - 0.5f) * 2.0f;
            bool ice = lat > 0.88f + 0.06f * n;
            bool land = n > 0.52f;
            if (ice) img.At(x, y) = Shade(0.95f, 0.97f, 1.0f, 0.95f + 0.05f * n);
            else if (land) img.At(x, y) = Shade(0.25f + 0.4f * (n - 0.52f) * 4.0f, 0.55f, 0.2f, 0.8f + 0.2f * n);
            else img.At(x, y) = Shade(0.1f, 0.3f, 0.8f, 0.7f + 0.3f * n);
        }
    return img;
}

Image Gdi(int w, int h, COLORREF fill, const std::function<void(HDC)>& draw, COLORREF keyColor, bool useKey) {
    Image img(w, h);
    HDC screen = GetDC(nullptr);
    HDC dc = CreateCompatibleDC(screen);
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h; // top-down
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP bmp = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (bmp && bits) {
        HGDIOBJ old = SelectObject(dc, bmp);
        HBRUSH brush = CreateSolidBrush(fill);
        RECT rc{ 0, 0, w, h };
        FillRect(dc, &rc, brush);
        DeleteObject(brush);
        draw(dc);
        GdiFlush();
        const uint32_t* src = static_cast<const uint32_t*>(bits); // BGRX
        for (int i = 0; i < w * h; ++i) {
            uint32_t p = src[i];
            uint8_t b = p & 0xFF, g = (p >> 8) & 0xFF, r = (p >> 16) & 0xFF;
            bool transparent = useKey && RGB(r, g, b) == keyColor;
            img.pixels[i] = transparent ? PackRgba(0, 0, 0, 0) : PackRgba(r, g, b, 255);
        }
        SelectObject(dc, old);
        DeleteObject(bmp);
    }
    DeleteDC(dc);
    ReleaseDC(nullptr, screen);
    return img;
}

namespace {

struct GdiPen {
    HPEN pen; HBRUSH brush; HGDIOBJ oldPen, oldBrush; HDC dc;
    GdiPen(HDC d, COLORREF outline, COLORREF fillColor, int width = 2) : dc(d) {
        pen = CreatePen(PS_SOLID, width, outline);
        brush = CreateSolidBrush(fillColor);
        oldPen = SelectObject(dc, pen);
        oldBrush = SelectObject(dc, brush);
    }
    ~GdiPen() { SelectObject(dc, oldPen); SelectObject(dc, oldBrush); DeleteObject(pen); DeleteObject(brush); }
};

} // namespace

Image Smiley(int size) {
    return Gdi(size, size, RGB(255, 0, 255), [size](HDC dc) {
        int m = size / 16;
        {
            GdiPen p(dc, RGB(120, 90, 0), RGB(255, 220, 0), size / 40 + 1);
            Ellipse(dc, m, m, size - m, size - m);
        }
        {
            GdiPen p(dc, RGB(0, 0, 0), RGB(0, 0, 0), 1);
            int ex = size / 5, ey = size / 3, er = size / 14;
            Ellipse(dc, size / 2 - ex - er, ey - er, size / 2 - ex + er, ey + er);
            Ellipse(dc, size / 2 + ex - er, ey - er, size / 2 + ex + er, ey + er);
        }
        {
            HPEN pen = CreatePen(PS_SOLID, size / 24 + 1, RGB(0, 0, 0));
            HGDIOBJ old = SelectObject(dc, pen);
            int r = size * 3 / 10;
            Arc(dc, size / 2 - r, size / 2 - r + size / 12, size / 2 + r, size / 2 + r,
                size / 2 - r * 3 / 4, size / 2 + r / 3, size / 2 + r * 3 / 4, size / 2 + r / 3);
            SelectObject(dc, old);
            DeleteObject(pen);
        }
    });
}

Image Door(int size) {
    return Gdi(size, size, RGB(255, 0, 255), [size](HDC dc) {
        {
            GdiPen p(dc, RGB(40, 20, 5), RGB(110, 60, 20), size / 32 + 1);
            Rectangle(dc, size / 8, 0, size * 7 / 8, size);
        }
        {
            GdiPen p(dc, RGB(60, 30, 10), RGB(140, 80, 30), 1);
            Rectangle(dc, size / 5, size / 12, size * 4 / 5, size * 11 / 24);
            Rectangle(dc, size / 5, size / 2, size * 4 / 5, size * 11 / 12);
        }
        {
            GdiPen p(dc, RGB(120, 100, 0), RGB(230, 200, 60), 1);
            int r = size / 20;
            Ellipse(dc, size * 3 / 4 - r, size / 2 - r, size * 3 / 4 + r, size / 2 + r);
        }
    });
}

Image Rock(int size) {
    return Gdi(size, size, RGB(255, 0, 255), [size](HDC dc) {
        GdiPen p(dc, RGB(60, 60, 60), RGB(140, 140, 140), size / 32 + 1);
        POINT pts[] = {
            { size / 6, size / 2 }, { size / 4, size / 5 }, { size / 2, size / 8 }, { size * 3 / 4, size / 4 },
            { size * 7 / 8, size / 2 }, { size * 3 / 4, size * 5 / 6 }, { size / 2, size * 7 / 8 }, { size / 4, size * 3 / 4 },
        };
        Polygon(dc, pts, 8);
        GdiPen p2(dc, RGB(90, 90, 90), RGB(170, 170, 170), 1);
        POINT hi[] = { { size / 3, size / 3 }, { size / 2, size / 4 }, { size * 5 / 8, size * 3 / 8 }, { size * 3 / 8, size / 2 } };
        Polygon(dc, hi, 4);
    });
}

Image Rat(int size) {
    return Gdi(size, size, RGB(255, 0, 255), [size](HDC dc) {
        int cy = size * 3 / 5;
        {
            HPEN pen = CreatePen(PS_SOLID, size / 40 + 1, RGB(200, 130, 130));
            HGDIOBJ old = SelectObject(dc, pen);
            MoveToEx(dc, size / 10, cy, nullptr);
            LineTo(dc, size / 3, cy + size / 12);
            SelectObject(dc, old);
            DeleteObject(pen);
        }
        {
            GdiPen p(dc, RGB(60, 60, 60), RGB(120, 120, 125), size / 40 + 1);
            Ellipse(dc, size / 4, cy - size / 6, size * 3 / 4, cy + size / 6);   // body
            Ellipse(dc, size * 5 / 8, cy - size / 8, size * 15 / 16, cy + size / 12); // head
        }
        {
            GdiPen p(dc, RGB(150, 90, 100), RGB(230, 170, 180), 1);
            int r = size / 14;
            Ellipse(dc, size * 11 / 16 - r, cy - size / 6 - r / 2, size * 11 / 16 + r, cy - size / 6 + r * 3 / 2); // ear
        }
        {
            GdiPen p(dc, RGB(0, 0, 0), RGB(0, 0, 0), 1);
            int r = size / 40 + 1;
            Ellipse(dc, size * 13 / 16 - r, cy - size / 20 - r, size * 13 / 16 + r, cy - size / 20 + r); // eye
        }
    });
}

Image Checker(int w, int h, int cellsX, int cellsY, uint32_t a, uint32_t b) {
    Image img(w, h);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            img.At(x, y) = (((x * cellsX / w) + (y * cellsY / h)) & 1) ? b : a;
    return img;
}

Image GdiMask(int w, int h, const std::function<void(HDC)>& draw, COLORREF color) {
    Image img = Gdi(w, h, RGB(0, 0, 0), draw, RGB(255, 0, 255), false);
    uint8_t cr = GetRValue(color), cg = GetGValue(color), cb = GetBValue(color);
    for (auto& p : img.pixels) {
        uint32_t r = p & 0xFF, g = (p >> 8) & 0xFF, b = (p >> 16) & 0xFF;
        uint8_t luma = static_cast<uint8_t>((r * 299 + g * 587 + b * 114) / 1000);
        p = PackRgba(cr, cg, cb, luma);
    }
    return img;
}

LOGFONTW MakeLogFont(const wchar_t* face, int pixelHeight, int weight, bool italic) {
    LOGFONTW lf{};
    lf.lfHeight = -std::max(pixelHeight, 1);
    lf.lfWeight = weight;
    lf.lfItalic = italic ? TRUE : FALSE;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfOutPrecision = OUT_TT_PRECIS;
    lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    lf.lfQuality = ANTIALIASED_QUALITY;
    lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
    wcsncpy_s(lf.lfFaceName, face, _TRUNCATE);
    return lf;
}

Image TextImage(const std::wstring& text, const LOGFONTW& fontIn, COLORREF color, int padding) {
    LOGFONTW lf = fontIn;
    lf.lfQuality = ANTIALIASED_QUALITY;   // never ClearType: colour fringes have no meaning in a mask
    if (text.empty()) return Image(1, 1, 0);

    // Measure with a throwaway DC; italics overhang their advance so pad by a third of the height.
    HDC screen = GetDC(nullptr);
    HDC dc = CreateCompatibleDC(screen);
    HFONT font = CreateFontIndirectW(&lf);
    HGDIOBJ old = SelectObject(dc, font);
    RECT rc{ 0, 0, 0, 0 };
    DrawTextW(dc, text.c_str(), -1, &rc, DT_CALCRECT | DT_NOPREFIX | DT_LEFT | DT_NOCLIP);
    SelectObject(dc, old);
    DeleteDC(dc);
    ReleaseDC(nullptr, screen);
    int overhang = lf.lfItalic ? std::abs(lf.lfHeight) / 3 : 0;
    int w = std::max<int>(rc.right - rc.left, 1) + 2 * padding + overhang;
    int h = std::max<int>(rc.bottom - rc.top, 1) + 2 * padding;

    Image img = GdiMask(w, h, [&](HDC d) {
        HGDIOBJ prev = SelectObject(d, font);
        SetBkMode(d, TRANSPARENT);
        SetTextColor(d, RGB(255, 255, 255));
        RECT r{ padding, padding, w - padding, h - padding };
        DrawTextW(d, text.c_str(), -1, &r, DT_NOPREFIX | DT_LEFT | DT_NOCLIP);
        SelectObject(d, prev);
    }, color);
    DeleteObject(font);
    return img;
}

Image Emblem(int size, const uint32_t* paneColors) {
    static const uint32_t kDefault[4] = { PackRgba(235, 50, 35), PackRgba(60, 180, 50), PackRgba(30, 110, 230), PackRgba(250, 200, 30) };
    const uint32_t* pal = paneColors ? paneColors : kDefault;
    Image img(size, size, 0);
    const int ss = 3;                       // supersampling per axis
    const float gap = 0.035f;               // pane separation as a fraction of the size
    const float x0 = 0.06f, x1 = 0.94f;     // horizontal extent of the flag
    auto wave = [](float u) { return 0.075f * std::sin(u * 4.2f - 0.9f); };
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            float acc[4] = { 0, 0, 0, 0 };
            int cover = 0;
            for (int sy = 0; sy < ss; ++sy) {
                for (int sx = 0; sx < ss; ++sx) {
                    float px = (x + (sx + 0.5f) / ss) / size;
                    float py = (y + (sy + 0.5f) / ss) / size;
                    if (px < x0 || px > x1) continue;
                    float u = (px - x0) / (x1 - x0);            // 0..1 across the flag
                    float top = 0.16f + wave(u), bottom = 0.84f + wave(u);
                    if (py < top || py > bottom) continue;
                    float v = (py - top) / (bottom - top);      // 0..1 down the flag
                    // Pane boundaries: a vertical curve through the middle and the horizontal wave.
                    float mid = 0.5f + 0.02f * std::sin(v * 3.0f);
                    if (std::fabs(u - mid) < gap * 0.6f) continue;
                    if (std::fabs(v - 0.5f) < gap * 0.6f) continue;
                    if (std::fabs(px - x0) < gap * 0.5f || std::fabs(px - x1) < gap * 0.5f) continue;
                    if (py - top < gap * 0.5f || bottom - py < gap * 0.5f) continue;
                    int pane = (u < mid ? 0 : 1) + (v < 0.5f ? 0 : 2);
                    // Shade by the wave slope so the cloth reads as lit from the upper left.
                    float slope = 0.075f * 4.2f * std::cos(u * 4.2f - 0.9f);
                    float k = Clamp(0.82f + 1.6f * slope, 0.55f, 1.15f);
                    uint32_t c = pal[pane];
                    acc[0] += (c & 0xFF) * k;
                    acc[1] += ((c >> 8) & 0xFF) * k;
                    acc[2] += ((c >> 16) & 0xFF) * k;
                    acc[3] += static_cast<float>((c >> 24) & 0xFF);
                    ++cover;
                }
            }
            if (!cover) continue;
            float n = static_cast<float>(ss * ss);
            uint8_t a = static_cast<uint8_t>(Clamp(acc[3] / n, 0.0f, 255.0f) + 0.5f);
            // Straight alpha: colour is the average over covered samples only.
            uint8_t r = static_cast<uint8_t>(Clamp(acc[0] / cover, 0.0f, 255.0f) + 0.5f);
            uint8_t g = static_cast<uint8_t>(Clamp(acc[1] / cover, 0.0f, 255.0f) + 0.5f);
            uint8_t b = static_cast<uint8_t>(Clamp(acc[2] / cover, 0.0f, 255.0f) + 0.5f);
            img.At(x, y) = PackRgba(r, g, b, a);
        }
    }
    return img;
}

Image SoftDot(int size, float hardness) {
    Image img(size, size, 0);
    float half = size * 0.5f;
    float soft = std::max(1.0f - Saturate(hardness), 0.02f);
    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x) {
            float dx = (x + 0.5f - half) / half, dy = (y + 0.5f - half) / half;
            float r = std::sqrt(dx * dx + dy * dy);
            float t = Saturate((1.0f - r) / soft);
            float a = t * t * (3.0f - 2.0f * t);
            img.At(x, y) = PackRgbaF(a, a, a, a);
        }
    return img;
}

Image DiscLogo(const std::wstring& text, int w, int h) {
    return GdiMask(w, h, [&](HDC dc) {
        int textH = h * 58 / 100;
        LOGFONTW lf = MakeLogFont(L"Segoe UI", textH, FW_BLACK, true);
        HFONT font = CreateFontIndirectW(&lf);
        HGDIOBJ old = SelectObject(dc, font);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(255, 255, 255));
        RECT rc{ 0, 0, w, h * 62 / 100 };
        DrawTextW(dc, text.c_str(), -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        SelectObject(dc, old);
        DeleteObject(font);
        // Flattened disc: white ring with a dark hole, under the text.
        int cy = h * 80 / 100, ry = h * 13 / 100, rx = w * 46 / 100;
        {
            GdiPen p(dc, RGB(255, 255, 255), RGB(255, 255, 255), 1);
            Ellipse(dc, w / 2 - rx, cy - ry, w / 2 + rx, cy + ry);
        }
        {
            GdiPen p(dc, RGB(0, 0, 0), RGB(0, 0, 0), 1);
            Ellipse(dc, w / 2 - rx / 3, cy - ry / 3, w / 2 + rx / 3, cy + ry / 3);
        }
    });
}

Image Toaster(int frameW, int frameH, int frames) {
    return Gdi(frameW * frames, frameH, RGB(255, 0, 255), [&](HDC dc) {
        for (int f = 0; f < frames; ++f) {
            const int ox = f * frameW;
            const int bodyL = ox + frameW / 8, bodyR = ox + frameW * 7 / 8, bodyT = frameH * 6 / 12, bodyB = frameH * 11 / 12;
            // Body: chrome box with a darker base band, two slots on top, a lever and a highlight.
            {
                GdiPen p(dc, RGB(70, 70, 80), RGB(200, 204, 212), 2);
                RoundRect(dc, bodyL, bodyT, bodyR, bodyB, frameW / 8, frameH / 8);
            }
            {
                GdiPen p(dc, RGB(70, 70, 80), RGB(150, 152, 160), 1);
                Rectangle(dc, bodyL + 3, bodyB - frameH / 9, bodyR - 3, bodyB - 3);
            }
            {
                GdiPen p(dc, RGB(40, 40, 45), RGB(40, 40, 45), 1);
                int slotY = bodyT + 3, slotH = frameH / 20 + 2;
                Rectangle(dc, bodyL + frameW / 8, slotY, ox + frameW / 2 - 3, slotY + slotH);
                Rectangle(dc, ox + frameW / 2 + 3, slotY, bodyR - frameW / 8, slotY + slotH);
            }
            {
                GdiPen p(dc, RGB(60, 60, 60), RGB(30, 30, 30), 1);
                Rectangle(dc, bodyL + frameW / 12, bodyT + frameH / 5, bodyL + frameW / 12 + 6, bodyT + frameH / 5 + frameH / 10);
            }
            {
                GdiPen p(dc, RGB(255, 255, 255), RGB(255, 255, 255), 1);
                Rectangle(dc, bodyL + 6, bodyT + 6, bodyL + 10, bodyB - frameH / 8);
            }
            // Wing on top of the body: a feathered white wing that flaps between raised and swept.
            float flap = std::sin(static_cast<float>(f) / frames * kTwoPi);   // -1..1
            {
                GdiPen p(dc, RGB(90, 90, 100), RGB(245, 245, 250), 2);
                int rx = ox + frameW / 2, ry = bodyT + 2;                        // wing root on the lid
                int lift = static_cast<int>(frameH * 0.22f * (0.6f + 0.4f * flap));
                int sweep = static_cast<int>(frameW * 0.12f * flap);
                POINT wing[] = {
                    { rx - frameW / 10, ry }, { rx - frameW / 8 + sweep, ry - lift },
                    { rx + frameW / 6 + sweep, ry - lift - frameH / 8 }, { rx + frameW * 2 / 5 + sweep, ry - lift - frameH / 14 },
                    { rx + frameW * 2 / 5 + sweep - frameW / 16, ry - lift + frameH / 12 },
                    { rx + frameW / 5 + sweep, ry - lift / 2 }, { rx + frameW / 8, ry },
                };
                Polygon(dc, wing, 7);
                // Feather lines.
                HPEN pen = CreatePen(PS_SOLID, 1, RGB(160, 160, 170));
                HGDIOBJ old = SelectObject(dc, pen);
                for (int k = 1; k <= 3; ++k) {
                    MoveToEx(dc, rx + frameW / 24 * k, ry - 2, nullptr);
                    LineTo(dc, rx + frameW / 6 + sweep + k * frameW / 14, ry - lift - frameH / 12 + k * frameH / 20);
                }
                SelectObject(dc, old);
                DeleteObject(pen);
            }
        }
    });
}

Image Toast(int size) {
    return Gdi(size, size, RGB(255, 0, 255), [size](HDC dc) {
        int m = size / 10;
        {
            GdiPen p(dc, RGB(110, 60, 20), RGB(170, 110, 50), 2);   // crust
            POINT slice[] = { { m, size / 3 }, { m + size / 8, m }, { size / 2, m + size / 12 }, { size - m - size / 8, m },
                              { size - m, size / 3 }, { size - m, size - m }, { m, size - m } };
            Polygon(dc, slice, 7);
        }
        {
            GdiPen p(dc, RGB(220, 180, 110), RGB(235, 200, 130), 1);   // soft centre
            POINT inner[] = { { m * 2, size * 2 / 5 }, { m * 2 + size / 10, m * 2 }, { size / 2, m * 2 + size / 16 }, { size - m * 2 - size / 10, m * 2 },
                              { size - m * 2, size * 2 / 5 }, { size - m * 2, size - m * 2 }, { m * 2, size - m * 2 } };
            Polygon(dc, inner, 7);
        }
    });
}

Image LogoText(const std::wstring& text, int w, int h, COLORREF fg, COLORREF bg) {
    return Gdi(w, h, bg, [&](HDC dc) {
        HFONT font = CreateFontW(-h / 2, 0, 0, 0, FW_BOLD, TRUE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                 CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        HGDIOBJ old = SelectObject(dc, font);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, fg);
        RECT rc{ 0, 0, w, h };
        DrawTextW(dc, text.c_str(), -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(dc, old);
        DeleteObject(font);
    }, RGB(255, 0, 255), false);
}

} // namespace rs::TextureFactory
