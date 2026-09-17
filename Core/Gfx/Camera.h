#pragma once
#include <DirectXMath.h>

namespace rs {

// Simple look-at perspective camera. Left-handed, row-major (DirectXMath conventions);
// transpose matrices when uploading to constant buffers.
struct Camera {
    DirectX::XMFLOAT3 eye{ 0, 0, -10 };
    DirectX::XMFLOAT3 target{ 0, 0, 0 };
    DirectX::XMFLOAT3 up{ 0, 1, 0 };
    float fovY = DirectX::XM_PIDIV4;
    float aspect = 16.0f / 9.0f;
    float nearZ = 0.1f;
    float farZ = 200.0f;

    DirectX::XMMATRIX View() const {
        return DirectX::XMMatrixLookAtLH(DirectX::XMLoadFloat3(&eye), DirectX::XMLoadFloat3(&target), DirectX::XMLoadFloat3(&up));
    }
    DirectX::XMMATRIX Proj() const {
        return DirectX::XMMatrixPerspectiveFovLH(fovY, aspect, nearZ, farZ);
    }
    DirectX::XMMATRIX ViewProj() const { return View() * Proj(); }

    DirectX::XMVECTOR Forward() const {
        return DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&target), DirectX::XMLoadFloat3(&eye)));
    }
    DirectX::XMVECTOR Right() const {
        return DirectX::XMVector3Normalize(DirectX::XMVector3Cross(DirectX::XMLoadFloat3(&up), Forward()));
    }
    DirectX::XMVECTOR TrueUp() const {
        return DirectX::XMVector3Normalize(DirectX::XMVector3Cross(Forward(), Right()));
    }
};

} // namespace rs
