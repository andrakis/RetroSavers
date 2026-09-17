#pragma once
#include "Device.h"
#include <vector>
#include <cstdint>

namespace rs {

// CPU-side RGBA8 image (r in the low byte, top row first).
struct Image {
    int width = 0;
    int height = 0;
    std::vector<uint32_t> pixels;

    Image() = default;
    Image(int w, int h, uint32_t fill = 0xFF000000u) : width(w), height(h), pixels(static_cast<size_t>(w) * h, fill) {}
    bool Valid() const { return width > 0 && height > 0 && pixels.size() == static_cast<size_t>(width) * height; }
    uint32_t& At(int x, int y) { return pixels[static_cast<size_t>(y) * width + x]; }
    uint32_t At(int x, int y) const { return pixels[static_cast<size_t>(y) * width + x]; }
};

class Texture {
public:
    void FromImage(Device& device, const Image& image, bool mipmaps = true);
    void FromRgba(Device& device, int width, int height, const uint32_t* rgba, bool mipmaps = true);
    // Re-uploads pixels into the existing texture when the size matches (regenerating mips if
    // it has them); otherwise recreates it. For images that change every frame.
    void Update(Device& device, const Image& image);
    void BindPS(ID3D11DeviceContext* ctx, UINT slot) const { ID3D11ShaderResourceView* s = m_srv.Get(); ctx->PSSetShaderResources(slot, 1, &s); }
    ID3D11ShaderResourceView* SRV() const { return m_srv.Get(); }
    bool Valid() const { return m_srv != nullptr; }
    int Width() const { return m_width; }
    int Height() const { return m_height; }

private:
    ComPtr<ID3D11Texture2D> m_texture;
    ComPtr<ID3D11ShaderResourceView> m_srv;
    int m_width = 0;
    int m_height = 0;
    bool m_mipmaps = false;
};

} // namespace rs
