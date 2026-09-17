#pragma once
#include "Device.h"

namespace rs {

// Dynamic constant buffer holding one T (padded to 16 bytes).
template <typename T>
class ConstantBuffer {
public:
    void Create(Device& device) {
        D3D11_BUFFER_DESC desc{};
        desc.ByteWidth = (sizeof(T) + 15) & ~15u;
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        ThrowIfFailed(device.Get()->CreateBuffer(&desc, nullptr, &m_buffer), "CreateBuffer(constant)");
    }

    void Update(ID3D11DeviceContext* ctx, const T& data) {
        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (SUCCEEDED(ctx->Map(m_buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
            memcpy(mapped.pData, &data, sizeof(T));
            ctx->Unmap(m_buffer.Get(), 0);
        }
    }

    void BindVS(ID3D11DeviceContext* ctx, UINT slot) const { ID3D11Buffer* b = m_buffer.Get(); ctx->VSSetConstantBuffers(slot, 1, &b); }
    void BindPS(ID3D11DeviceContext* ctx, UINT slot) const { ID3D11Buffer* b = m_buffer.Get(); ctx->PSSetConstantBuffers(slot, 1, &b); }
    ID3D11Buffer* Get() const { return m_buffer.Get(); }

private:
    ComPtr<ID3D11Buffer> m_buffer;
};

// Growable dynamic vertex buffer of POD elements (instance streams, line batches).
template <typename T>
class DynamicVertexBuffer {
public:
    void Reserve(Device& device, size_t count) {
        if (count <= m_capacity) return;
        size_t cap = m_capacity ? m_capacity : 256;
        while (cap < count) cap *= 2;
        D3D11_BUFFER_DESC desc{};
        desc.ByteWidth = static_cast<UINT>(cap * sizeof(T));
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        m_buffer.Reset();
        ThrowIfFailed(device.Get()->CreateBuffer(&desc, nullptr, &m_buffer), "CreateBuffer(dynamic vertex)");
        m_capacity = cap;
    }

    void Update(Device& device, const T* data, size_t count) {
        m_count = count;
        if (count == 0) return;
        Reserve(device, count);
        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (SUCCEEDED(device.Ctx()->Map(m_buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
            memcpy(mapped.pData, data, count * sizeof(T));
            device.Ctx()->Unmap(m_buffer.Get(), 0);
        }
    }

    void Bind(ID3D11DeviceContext* ctx, UINT slot) const {
        ID3D11Buffer* b = m_buffer.Get();
        UINT stride = sizeof(T), offset = 0;
        ctx->IASetVertexBuffers(slot, 1, &b, &stride, &offset);
    }

    size_t Count() const { return m_count; }
    ID3D11Buffer* Get() const { return m_buffer.Get(); }

private:
    ComPtr<ID3D11Buffer> m_buffer;
    size_t m_capacity = 0;
    size_t m_count = 0;
};

} // namespace rs
