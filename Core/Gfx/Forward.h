#pragma once
#include "Device.h"
#include "ConstantBuffer.h"
#include "Mesh.h"
#include "States.h"
#include "Texture.h"
#include "Camera.h"

namespace rs {

struct FrameConstants {
    DirectX::XMFLOAT4X4 viewProj;   // transposed for HLSL
    DirectX::XMFLOAT4 eyePos;
    DirectX::XMFLOAT4 lightDir;     // towards the light
    DirectX::XMFLOAT4 lightColor;
    DirectX::XMFLOAT4 ambient;
    DirectX::XMFLOAT4 fogColor;     // a > 0.5 enables
    DirectX::XMFLOAT4 fogParams;    // start, end
};

struct ObjectConstants {
    DirectX::XMFLOAT4X4 world;      // transposed for HLSL
    DirectX::XMFLOAT4 color;
    DirectX::XMFLOAT4 material;     // specPower, specIntensity, useTexture, uvScale
};

struct Material {
    DirectX::XMFLOAT4 color{ 1, 1, 1, 1 };
    float specPower = 32.0f;
    float specIntensity = 0.4f;
    float uvScale = 1.0f;
    const Texture* texture = nullptr;
    ID3D11SamplerState* sampler = nullptr;   // defaults to anisotropic wrap
};

// Blinn-Phong forward renderer with a per-object path and an instanced path.
class Forward {
public:
    void Create(Device& device);

    // Fills the per-frame constants from a camera and lighting values.
    void BeginFrame(ID3D11DeviceContext* ctx, const Camera& camera, const DirectX::XMFLOAT3& lightDir,
                    const DirectX::XMFLOAT3& lightColor, const DirectX::XMFLOAT3& ambient);
    void BeginFrame(ID3D11DeviceContext* ctx, const FrameConstants& frame);

    void Draw(ID3D11DeviceContext* ctx, const Mesh& mesh, const DirectX::XMMATRIX& world, const Material& material);
    // Instances come from `instances` (slot 1); material colour is ignored in favour of instance colour.
    void DrawInstanced(ID3D11DeviceContext* ctx, const Mesh& mesh, const DynamicVertexBuffer<InstanceData>& instances, const Material& material);

    const States& GetStates() const { return m_states; }
    States& GetStates() { return m_states; }

private:
    void SetMaterial(ID3D11DeviceContext* ctx, const Material& m, const DirectX::XMMATRIX& world);

    ComPtr<ID3D11VertexShader> m_vs;
    ComPtr<ID3D11VertexShader> m_vsInstanced;
    ComPtr<ID3D11PixelShader> m_ps;
    ComPtr<ID3D11InputLayout> m_layout;
    ComPtr<ID3D11InputLayout> m_layoutInstanced;
    ConstantBuffer<FrameConstants> m_frameCb;
    ConstantBuffer<ObjectConstants> m_objectCb;
    States m_states;
    Texture m_white;
};

} // namespace rs
