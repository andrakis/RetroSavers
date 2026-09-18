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
    DirectX::XMFLOAT4 lightMap;     // enable, world XZ -> uv scale, strength, unused
};

struct Material {
    DirectX::XMFLOAT4 color{ 1, 1, 1, 1 };
    float specPower = 32.0f;
    float specIntensity = 0.4f;
    float uvScale = 1.0f;
    const Texture* texture = nullptr;
    ID3D11SamplerState* sampler = nullptr;   // defaults to anisotropic wrap
    // Projected light map (t1, linear wrap): a top-down texture indexed by world XZ that
    // multiplies the diffuse term by 1 + strength * texel. Caustics, spotlights.
    ID3D11ShaderResourceView* lightMap = nullptr;
    float lightMapScale = 0.1f;               // world units -> uv
    float lightMapStrength = 1.0f;
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
    // Same with a custom vertex shader (+ its input layout) and/or pixel shader, keeping the
    // PerFrame / PerObject constants, the material texture in t0 and the light map in t1.
    // nullptr falls back to the Phong shader for that stage.
    void Draw(ID3D11DeviceContext* ctx, const Mesh& mesh, const DirectX::XMMATRIX& world, const Material& material,
              ID3D11VertexShader* vs, ID3D11InputLayout* layout, ID3D11PixelShader* ps);
    // Uploads PerObject and binds the material's texture, sampler and light map (t0/s0, t1/s1)
    // without drawing; for savers that issue their own draw calls.
    void BindMaterial(ID3D11DeviceContext* ctx, const Material& material, const DirectX::XMMATRIX& world);
    // Instances come from `instances` (slot 1); material colour is ignored in favour of instance colour.
    void DrawInstanced(ID3D11DeviceContext* ctx, const Mesh& mesh, const DynamicVertexBuffer<InstanceData>& instances, const Material& material);

    const States& GetStates() const { return m_states; }
    States& GetStates() { return m_states; }
    ID3D11PixelShader* PixelShader() const { return m_ps.Get(); }
    ID3D11VertexShader* VertexShader() const { return m_vs.Get(); }
    ID3D11InputLayout* Layout() const { return m_layout.Get(); }

private:

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
