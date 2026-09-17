#include "RenderTexture.h"
#include <algorithm>

namespace rs {

void RenderTexture::Create(Device& device, int width, int height, DXGI_FORMAT format) {
    m_format = format;
    m_width = std::max(width, 1);
    m_height = std::max(height, 1);
    m_texture.Reset();
    m_rtv.Reset();
    m_srv.Reset();

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = m_width;
    desc.Height = m_height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = format;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    ThrowIfFailed(device.Get()->CreateTexture2D(&desc, nullptr, &m_texture), "CreateTexture2D(render texture)");
    ThrowIfFailed(device.Get()->CreateRenderTargetView(m_texture.Get(), nullptr, &m_rtv), "CreateRenderTargetView(render texture)");
    ThrowIfFailed(device.Get()->CreateShaderResourceView(m_texture.Get(), nullptr, &m_srv), "CreateShaderResourceView(render texture)");
}

void RenderTexture::Resize(Device& device, int width, int height) {
    width = std::max(width, 1);
    height = std::max(height, 1);
    if (width == m_width && height == m_height && Valid()) return;
    Create(device, width, height, m_format);
}

void RenderTexture::Bind(ID3D11DeviceContext* ctx, bool setViewport) const {
    ID3D11RenderTargetView* rtv = m_rtv.Get();
    ctx->OMSetRenderTargets(1, &rtv, nullptr);
    if (setViewport) {
        D3D11_VIEWPORT vp{ 0, 0, static_cast<float>(m_width), static_cast<float>(m_height), 0, 1 };
        ctx->RSSetViewports(1, &vp);
        D3D11_RECT sc{ 0, 0, m_width, m_height };
        ctx->RSSetScissorRects(1, &sc);
    }
}

void RenderTexture::Upload(ID3D11DeviceContext* ctx, const Image& image) const {
    if (!Valid() || image.width != m_width || image.height != m_height) return;
    ctx->UpdateSubresource(m_texture.Get(), 0, nullptr, image.pixels.data(), image.width * 4, 0);
}

Image RenderTexture::Readback(Device& device) const {
    Image img(m_width, m_height, 0);
    if (!Valid()) return img;
    D3D11_TEXTURE2D_DESC desc{};
    m_texture->GetDesc(&desc);
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.MiscFlags = 0;
    ComPtr<ID3D11Texture2D> staging;
    if (FAILED(device.Get()->CreateTexture2D(&desc, nullptr, &staging))) return img;
    device.Ctx()->CopyResource(staging.Get(), m_texture.Get());
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (SUCCEEDED(device.Ctx()->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped))) {
        for (int y = 0; y < m_height; ++y)
            memcpy(&img.pixels[static_cast<size_t>(y) * m_width], static_cast<const uint8_t*>(mapped.pData) + static_cast<size_t>(y) * mapped.RowPitch, static_cast<size_t>(m_width) * 4);
        device.Ctx()->Unmap(staging.Get(), 0);
    }
    return img;
}

void RenderTexture::Clear(ID3D11DeviceContext* ctx, const DirectX::XMFLOAT4& color) const {
    ctx->ClearRenderTargetView(m_rtv.Get(), &color.x);
}

} // namespace rs
