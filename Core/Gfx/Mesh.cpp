#include "Mesh.h"

using namespace DirectX;

namespace rs {

void MeshData::Append(const MeshData& other) {
    uint32_t base = static_cast<uint32_t>(vertices.size());
    vertices.insert(vertices.end(), other.vertices.begin(), other.vertices.end());
    indices.reserve(indices.size() + other.indices.size());
    for (uint32_t i : other.indices) indices.push_back(base + i);
}

void MeshData::Transform(const XMMATRIX& m) {
    XMMATRIX n = XMMatrixTranspose(XMMatrixInverse(nullptr, m));
    for (auto& v : vertices) {
        XMStoreFloat3(&v.position, XMVector3TransformCoord(XMLoadFloat3(&v.position), m));
        XMStoreFloat3(&v.normal, XMVector3Normalize(XMVector3TransformNormal(XMLoadFloat3(&v.normal), n)));
    }
}

void MeshData::ComputeNormals() {
    std::vector<XMFLOAT3> acc(vertices.size(), XMFLOAT3{ 0, 0, 0 });
    for (size_t i = 0; i + 2 < indices.size(); i += 3) {
        uint32_t a = indices[i], b = indices[i + 1], c = indices[i + 2];
        XMVECTOR pa = XMLoadFloat3(&vertices[a].position), pb = XMLoadFloat3(&vertices[b].position), pc = XMLoadFloat3(&vertices[c].position);
        XMVECTOR n = XMVector3Cross(XMVectorSubtract(pb, pa), XMVectorSubtract(pc, pa));
        for (uint32_t idx : { a, b, c }) {
            XMVECTOR cur = XMLoadFloat3(&acc[idx]);
            XMStoreFloat3(&acc[idx], XMVectorAdd(cur, n));
        }
    }
    for (size_t i = 0; i < vertices.size(); ++i) {
        XMVECTOR n = XMLoadFloat3(&acc[i]);
        if (XMVectorGetX(XMVector3LengthSq(n)) > 1e-12f)
            XMStoreFloat3(&vertices[i].normal, XMVector3Normalize(n));
        else
            vertices[i].normal = { 0, 1, 0 };
    }
}

void MeshData::FixWinding() {
    for (size_t i = 0; i + 2 < indices.size(); i += 3) {
        const auto& a = vertices[indices[i]];
        const auto& b = vertices[indices[i + 1]];
        const auto& c = vertices[indices[i + 2]];
        XMVECTOR pa = XMLoadFloat3(&a.position), pb = XMLoadFloat3(&b.position), pc = XMLoadFloat3(&c.position);
        XMVECTOR cross = XMVector3Cross(XMVectorSubtract(pb, pa), XMVectorSubtract(pc, pa));
        XMVECTOR n = XMVectorAdd(XMVectorAdd(XMLoadFloat3(&a.normal), XMLoadFloat3(&b.normal)), XMLoadFloat3(&c.normal));
        if (XMVectorGetX(XMVector3Dot(cross, n)) < 0.0f) std::swap(indices[i + 1], indices[i + 2]);
    }
}

void Mesh::Create(Device& device, const MeshData& data) {
    m_vb.Reset();
    m_ib.Reset();
    m_indexCount = 0;
    if (data.vertices.empty() || data.indices.empty()) return;

    D3D11_BUFFER_DESC vd{};
    vd.ByteWidth = static_cast<UINT>(data.vertices.size() * sizeof(VertexPNT));
    vd.Usage = D3D11_USAGE_IMMUTABLE;
    vd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA vs{ data.vertices.data(), 0, 0 };
    ThrowIfFailed(device.Get()->CreateBuffer(&vd, &vs, &m_vb), "CreateBuffer(vertex)");

    D3D11_BUFFER_DESC id{};
    id.ByteWidth = static_cast<UINT>(data.indices.size() * sizeof(uint32_t));
    id.Usage = D3D11_USAGE_IMMUTABLE;
    id.BindFlags = D3D11_BIND_INDEX_BUFFER;
    D3D11_SUBRESOURCE_DATA is{ data.indices.data(), 0, 0 };
    ThrowIfFailed(device.Get()->CreateBuffer(&id, &is, &m_ib), "CreateBuffer(index)");
    m_indexCount = static_cast<UINT>(data.indices.size());
}

void Mesh::Bind(ID3D11DeviceContext* ctx) const {
    ID3D11Buffer* vb = m_vb.Get();
    UINT stride = sizeof(VertexPNT), offset = 0;
    ctx->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
    ctx->IASetIndexBuffer(m_ib.Get(), DXGI_FORMAT_R32_UINT, 0);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void Mesh::Draw(ID3D11DeviceContext* ctx) const {
    if (!m_indexCount) return;
    Bind(ctx);
    ctx->DrawIndexed(m_indexCount, 0, 0);
}

void Mesh::DrawInstanced(ID3D11DeviceContext* ctx, UINT instances) const {
    if (!m_indexCount || !instances) return;
    Bind(ctx);
    ctx->DrawIndexedInstanced(m_indexCount, instances, 0, 0, 0);
}

} // namespace rs
