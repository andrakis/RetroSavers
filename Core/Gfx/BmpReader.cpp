#include "BmpReader.h"
#include "Util/Color.h"
#include <windows.h>
#include <cstdio>
#include <vector>

namespace rs::BmpReader {

std::optional<Image> Load(const std::wstring& path) {
    if (path.empty()) return std::nullopt;
    FILE* f = nullptr;
    if (_wfopen_s(&f, path.c_str(), L"rb") != 0 || !f) return std::nullopt;
    std::vector<uint8_t> data;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len <= 0 || len > 256 * 1024 * 1024) { fclose(f); return std::nullopt; }
    data.resize(static_cast<size_t>(len));
    size_t got = fread(data.data(), 1, data.size(), f);
    fclose(f);
    if (got != data.size() || data.size() < sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER)) return std::nullopt;

    BITMAPFILEHEADER fh{};
    BITMAPINFOHEADER ih{};
    memcpy(&fh, data.data(), sizeof(fh));
    memcpy(&ih, data.data() + sizeof(fh), sizeof(ih));
    if (fh.bfType != 0x4D42 || ih.biSize < sizeof(BITMAPINFOHEADER)) return std::nullopt;
    if (ih.biCompression != BI_RGB && ih.biCompression != BI_BITFIELDS) return std::nullopt;
    if (ih.biBitCount != 24 && ih.biBitCount != 32) return std::nullopt;
    if (ih.biWidth <= 0 || ih.biWidth > 16384 || ih.biHeight == 0 || ih.biHeight > 16384 || ih.biHeight < -16384) return std::nullopt;

    int w = ih.biWidth;
    int h = ih.biHeight > 0 ? ih.biHeight : -ih.biHeight;
    bool bottomUp = ih.biHeight > 0;
    int bpp = ih.biBitCount / 8;
    size_t stride = (static_cast<size_t>(w) * bpp + 3) & ~static_cast<size_t>(3);
    size_t offset = fh.bfOffBits;
    if (offset + stride * h > data.size()) return std::nullopt;

    Image img(w, h);
    for (int y = 0; y < h; ++y) {
        const uint8_t* row = data.data() + offset + stride * (bottomUp ? (h - 1 - y) : y);
        for (int x = 0; x < w; ++x) {
            const uint8_t* p = row + static_cast<size_t>(x) * bpp;
            img.At(x, y) = PackRgba(p[2], p[1], p[0], 255);
        }
    }
    return img;
}

} // namespace rs::BmpReader
