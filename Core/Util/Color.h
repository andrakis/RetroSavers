#pragma once
#include <DirectXMath.h>
#include <windows.h>
#include <cstdint>
#include "MathUtil.h"

namespace rs {

// h in [0,1) around the wheel, s/v in [0,1].
inline DirectX::XMFLOAT4 HsvToRgb(float h, float s, float v, float a = 1.0f) {
    h = Wrap01(h) * 6.0f;
    int i = static_cast<int>(h);
    float f = h - static_cast<float>(i);
    float p = v * (1.0f - s);
    float q = v * (1.0f - s * f);
    float t = v * (1.0f - s * (1.0f - f));
    switch (i % 6) {
    case 0: return { v, t, p, a };
    case 1: return { q, v, p, a };
    case 2: return { p, v, t, a };
    case 3: return { p, q, v, a };
    case 4: return { t, p, v, a };
    default: return { v, p, q, a };
    }
}

inline DirectX::XMFLOAT4 FromColorRef(COLORREF c, float a = 1.0f) {
    return { GetRValue(c) / 255.0f, GetGValue(c) / 255.0f, GetBValue(c) / 255.0f, a };
}

inline COLORREF ToColorRef(const DirectX::XMFLOAT4& c) {
    return RGB(static_cast<int>(Saturate(c.x) * 255.0f + 0.5f),
               static_cast<int>(Saturate(c.y) * 255.0f + 0.5f),
               static_cast<int>(Saturate(c.z) * 255.0f + 0.5f));
}

// Packs into the byte order of DXGI_FORMAT_R8G8B8A8_UNORM (r in the low byte).
inline uint32_t PackRgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
    return static_cast<uint32_t>(r) | (static_cast<uint32_t>(g) << 8) | (static_cast<uint32_t>(b) << 16) | (static_cast<uint32_t>(a) << 24);
}

inline uint32_t PackRgbaF(float r, float g, float b, float a = 1.0f) {
    return PackRgba(static_cast<uint8_t>(Saturate(r) * 255.0f + 0.5f), static_cast<uint8_t>(Saturate(g) * 255.0f + 0.5f),
                    static_cast<uint8_t>(Saturate(b) * 255.0f + 0.5f), static_cast<uint8_t>(Saturate(a) * 255.0f + 0.5f));
}

} // namespace rs
