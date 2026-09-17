#pragma once
#include "Device.h"
#include "ConstantBuffer.h"
#include "Camera.h"
#include "States.h"
#include "Texture.h"

namespace rs {

// Camera-facing textured quads (sprites). Alpha-tested and alpha-blended.
class Billboard {
public:
    void Create(Device& device);

    void BeginFrame(ID3D11DeviceContext* ctx, const Camera& camera);
    // Draws one sprite centred at `center` with the given world-space width/height.
    // Set `depthWrite` false for translucent sprites drawn after opaque geometry.
    void Draw(ID3D11DeviceContext* ctx, const Texture& texture, const DirectX::XMFLOAT3& center, float width, float height,
              const DirectX::XMFLOAT4& color = { 1, 1, 1, 1 }, bool depthWrite = true, const DirectX::XMFLOAT4& uvRect = { 0, 0, 1, 1 });
    // Same but with an explicit orientation (right/up vectors) instead of camera facing.
    void DrawOriented(ID3D11DeviceContext* ctx, const Texture& texture, const DirectX::XMFLOAT3& center, const DirectX::XMFLOAT3& right,
                      const DirectX::XMFLOAT3& up, const DirectX::XMFLOAT4& color = { 1, 1, 1, 1 }, bool depthWrite = true);

private:
    struct CB {
        DirectX::XMFLOAT4X4 viewProj;
        DirectX::XMFLOAT4 center;
        DirectX::XMFLOAT4 right;
        DirectX::XMFLOAT4 up;
        DirectX::XMFLOAT4 color;
        DirectX::XMFLOAT4 uvRect;
    };

    ComPtr<ID3D11VertexShader> m_vs;
    ComPtr<ID3D11PixelShader> m_ps;
    ConstantBuffer<CB> m_cb;
    States m_states;
    DirectX::XMFLOAT4X4 m_viewProj{};
    DirectX::XMFLOAT3 m_camRight{ 1, 0, 0 };
    DirectX::XMFLOAT3 m_camUp{ 0, 1, 0 };
};

} // namespace rs
