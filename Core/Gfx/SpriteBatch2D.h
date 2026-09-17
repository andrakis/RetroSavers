#pragma once
#include "Device.h"
#include "ConstantBuffer.h"
#include "States.h"
#include "Texture.h"
#include <DirectXMath.h>
#include <vector>

namespace rs {

// One textured quad in pixel space (origin top-left of the current viewport).
struct Sprite {
    DirectX::XMFLOAT2 pos;      // centre
    DirectX::XMFLOAT2 size;     // width, height in pixels
    DirectX::XMFLOAT4 uvRect;   // u0, v0, u1, v1 (swap u0/u1 to mirror)
    DirectX::XMFLOAT4 color;    // multiplies the texel
    float rotation;             // radians, clockwise on screen
};

// Batches instanced quads (SV_VertexID corners, one Sprite per instance). Draw order is push order.
class SpriteBatch2D {
public:
    void Create(Device& device);

    void Begin(int viewportWidth, int viewportHeight);
    void Push(const Sprite& s) { m_sprites.push_back(s); }
    void Push(float cx, float cy, float w, float h, const DirectX::XMFLOAT4& color,
              const DirectX::XMFLOAT4& uvRect = { 0, 0, 1, 1 }, float rotation = 0.0f) {
        m_sprites.push_back({ { cx, cy }, { w, h }, uvRect, color, rotation });
    }
    // Uploads and draws everything since Begin. texture == nullptr draws with a 1x1 white texture.
    // Sets its own 2D states (no depth, no cull, given blend). psOverride replaces Sprite2D_ps and
    // receives the same PSIn (pos, uv, color); sampler defaults to linear clamp.
    void End(Device& device, const Texture* texture = nullptr, ID3D11BlendState* blend = nullptr,
             ID3D11PixelShader* psOverride = nullptr, ID3D11SamplerState* sampler = nullptr);
    // Same, sampling an arbitrary SRV (e.g. a RenderTexture) in slot 0.
    void End(Device& device, ID3D11ShaderResourceView* srv, ID3D11BlendState* blend,
             ID3D11PixelShader* psOverride = nullptr, ID3D11SamplerState* sampler = nullptr);

    size_t Count() const { return m_sprites.size(); }
    const States& GetStates() const { return m_states; }
    const Texture& White() const { return m_white; }

private:
    struct ViewportCB { DirectX::XMFLOAT4 viewport; };

    ComPtr<ID3D11VertexShader> m_vs;
    ComPtr<ID3D11PixelShader> m_ps;
    ComPtr<ID3D11InputLayout> m_layout;
    ConstantBuffer<ViewportCB> m_cb;
    DynamicVertexBuffer<Sprite> m_vb;
    States m_states;
    Texture m_white;
    std::vector<Sprite> m_sprites;
    int m_width = 1, m_height = 1;
};

} // namespace rs
