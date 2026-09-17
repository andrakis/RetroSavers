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

// Indexed triangle list. Static by default; CreateDynamic + Update for CPU-deformed geometry.
class Mesh {
public:
    void Create(Device& device, const MeshData& data);
    // Dynamic vertex buffer sized to data.vertices (indices stay immutable): call Update each
    // frame with the same vertex count and index layout.
    void CreateDynamic(Device& device, const MeshData& data);
    void Update(Device& device, const MeshData& data);
    void Bind(ID3D11DeviceContext* ctx) const;       // slot 0 + index buffer + topology
    void Draw(ID3D11DeviceContext* ctx) const;
    void DrawInstanced(ID3D11DeviceContext* ctx, UINT instances) const;
    bool Valid() const { return m_indexCount > 0; }
    UINT IndexCount() const { return m_indexCount; }

private:
    void CreateIndexBuffer(Device& device, const MeshData& data);

    ComPtr<ID3D11Buffer> m_vb;
    ComPtr<ID3D11Buffer> m_ib;
    UINT m_indexCount = 0;
    UINT m_vertexCapacity = 0;   // > 0 when the vertex buffer is dynamic
};

} // namespace rs
