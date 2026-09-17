#pragma once
#include <DirectXMath.h>
#include <algorithm>
#include <cmath>

namespace rs {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 2.0f * kPi;

template <typename T> T Lerp(T a, T b, float t) { return a + (b - a) * t; }
template <typename T> T Clamp(T v, T lo, T hi) { return std::min(std::max(v, lo), hi); }
inline float Saturate(float v) { return Clamp(v, 0.0f, 1.0f); }
inline float Smoothstep(float e0, float e1, float x) { float t = Saturate((x - e0) / (e1 - e0)); return t * t * (3.0f - 2.0f * t); }
inline float Wrap01(float v) { return v - std::floor(v); }
inline float ToRadians(float deg) { return deg * (kPi / 180.0f); }

inline DirectX::XMFLOAT4 Lerp(const DirectX::XMFLOAT4& a, const DirectX::XMFLOAT4& b, float t) {
    return { Lerp(a.x, b.x, t), Lerp(a.y, b.y, t), Lerp(a.z, b.z, t), Lerp(a.w, b.w, t) };
}

} // namespace rs
