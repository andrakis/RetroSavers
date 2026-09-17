#pragma once
#include "Device.h"
#include "Vertex.h"
#include <vector>
#include <cstdint>

namespace rs {

struct MeshData {
    std::vector<VertexPNT> vertices;
    std::vector<uint32_t> indices;

    void Append(const MeshData& other);
    void Transform(const DirectX::XMMATRIX& m);   // positions + normals
    void ComputeNormals();                          // area-weighted, replaces existing
    void FixWinding();                              // make (b-a)x(c-a) agree with the vertex normals
};

// Static indexed triangle list.
class Mesh {
public:
    void Create(Device& device, const MeshData& data);
    void Bind(ID3D11DeviceContext* ctx) const;       // slot 0 + index buffer + topology
    void Draw(ID3D11DeviceContext* ctx) const;
    void DrawInstanced(ID3D11DeviceContext* ctx, UINT instances) const;
    bool Valid() const { return m_indexCount > 0; }
    UINT IndexCount() const { return m_indexCount; }

private:
    ComPtr<ID3D11Buffer> m_vb;
    ComPtr<ID3D11Buffer> m_ib;
    UINT m_indexCount = 0;
};

} // namespace rs
