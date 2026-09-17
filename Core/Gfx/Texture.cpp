#include "Texture.h"

namespace rs {

void Texture::FromImage(Device& device, const Image& image, bool mipmaps) {
    if (!image.Valid()) throw std::runtime_error("Texture::FromImage: invalid image");
    FromRgba(device, image.width, image.height, image.pixels.data(), mipmaps);
}

void Texture::FromRgba(Device& device, int width, int height, const uint32_t* rgba, bool mipmaps) {
    m_texture.Reset();
    m_srv.Reset();
    m_width = width;
    m_height = height;
    m_mipmaps = mipmaps;

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = mipmaps ? 0 : 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | (mipmaps ? D3D11_BIND_RENDER_TARGET : 0);
    desc.MiscFlags = mipmaps ? D3D11_RESOURCE_MISC_GENERATE_MIPS : 0;
    ThrowIfFailed(device.Get()->CreateTexture2D(&desc, nullptr, &m_texture), "CreateTexture2D");
    device.Ctx()->UpdateSubresource(m_texture.Get(), 0, nullptr, rgba, width * 4, 0);

    D3D11_SHADER_RESOURCE_VIEW_DESC sv{};
    sv.Format = desc.Format;
    sv.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    sv.Texture2D.MipLevels = mipmaps ? static_cast<UINT>(-1) : 1;
    ThrowIfFailed(device.Get()->CreateShaderResourceView(m_texture.Get(), &sv, &m_srv), "CreateShaderResourceView");
    if (mipmaps) device.Ctx()->GenerateMips(m_srv.Get());
}

void Texture::Update(Device& device, const Image& image) {
    if (!image.Valid()) throw std::runtime_error("Texture::Update: invalid image");
    if (!Valid() || image.width != m_width || image.height != m_height) {
        FromImage(device, image, m_mipmaps);
        return;
    }
    device.Ctx()->UpdateSubresource(m_texture.Get(), 0, nullptr, image.pixels.data(), image.width * 4, 0);
    if (m_mipmaps) device.Ctx()->GenerateMips(m_srv.Get());
}

} // namespace rs
