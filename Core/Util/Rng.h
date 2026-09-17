#pragma once
#include <cstdint>
#include <random>

namespace rs {

// Small wrapper around mt19937 with the handful of helpers the savers need.
class Rng {
public:
    Rng() : m_engine(std::random_device{}()) {}
    explicit Rng(uint32_t seed) : m_engine(seed) {}

    float Float() { return std::uniform_real_distribution<float>(0.0f, 1.0f)(m_engine); }
    float Range(float lo, float hi) { return lo + (hi - lo) * Float(); }
    int Int(int lo, int hi) { return std::uniform_int_distribution<int>(lo, hi)(m_engine); }
    bool Chance(float probability) { return Float() < probability; }

private:
    std::mt19937 m_engine;
};

} // namespace rs
