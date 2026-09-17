#pragma once
#include "Device.h"
#include "ConstantBuffer.h"
#include "Vertex.h"
#include "States.h"
#include <vector>

namespace rs {

// Batches 1-pixel aliased lines and flat-coloured triangles in pixel space (origin top-left of
// the current viewport). Triangles are drawn first, then lines, in push order within each.
class LineRenderer2D {
public:
    void Create(Device& device);

    void Begin(int viewportWidth, int viewportHeight);
    void Line(float x0, float y0, float x1, float y1, const DirectX::XMFLOAT4& color);
    void Polyline(const DirectX::XMFLOAT2* points, size_t count, bool closed, const DirectX::XMFLOAT4& color);
    // Filled primitives with per-vertex colours (Gouraud across the triangle).
    void Triangle(const DirectX::XMFLOAT2& a, const DirectX::XMFLOAT2& b, const DirectX::XMFLOAT2& c,
                  const DirectX::XMFLOAT4& ca, const DirectX::XMFLOAT4& cb, const DirectX::XMFLOAT4& cc);
    void Quad(const DirectX::XMFLOAT2& a, const DirectX::XMFLOAT2& b, const DirectX::XMFLOAT2& c, const DirectX::XMFLOAT2& d,
              const DirectX::XMFLOAT4& ca, const DirectX::XMFLOAT4& cb, const DirectX::XMFLOAT4& cc, const DirectX::XMFLOAT4& cd);
    // Uploads and draws everything since Begin. Sets its own 2D states (no depth, no cull, given blend).
    void End(Device& device, ID3D11BlendState* blend = nullptr);

    const States& GetStates() const { return m_states; }

private:
    struct ViewportCB { DirectX::XMFLOAT4 viewport; };

    ComPtr<ID3D11VertexShader> m_vs;
    ComPtr<ID3D11PixelShader> m_ps;
    ComPtr<ID3D11InputLayout> m_layout;
    ConstantBuffer<ViewportCB> m_cb;
    DynamicVertexBuffer<VertexPC> m_vb;
    DynamicVertexBuffer<VertexPC> m_triVb;
    States m_states;
    std::vector<VertexPC> m_verts;
    std::vector<VertexPC> m_tris;
    int m_width = 1, m_height = 1;
};

} // namespace rs
