#pragma once
#include "Saver.h"
#include "Gfx/LineRenderer2D.h"
#include <deque>
#include <vector>

// Beziers (Windows 2000/XP): a closed loop of cubic Bezier segments whose control
// points bounce around the screen, leaving a colour-cycling trail.
struct BeziersSettings {
    int curves = 4;   // 1..10 segments in the loop
    int trail = 30;   // 1..100 snapshots kept
    int speed = 5;    // 1..10

    static BeziersSettings Load(const rs::Settings& s);
    void Save(rs::Settings& s) const;
};

class BeziersSaver : public rs::Saver {
public:
    void Initialize(rs::Device& device, const rs::SaverContext& ctx) override;
    void Update(float dt, double time) override;
    void Render(rs::Device& device, rs::SwapChain& swap) override;
    void Resize(int width, int height) override;

private:
    static constexpr int kSubdivisions = 24;
    static constexpr float kTickRate = 30.0f;

    struct Point { DirectX::XMFLOAT2 pos, vel; };
    struct Snapshot {
        std::vector<DirectX::XMFLOAT2> points;
        DirectX::XMFLOAT4 color;
    };

    void Reset();
    void Tick();

    BeziersSettings m_settings;
    rs::SaverContext m_ctx;
    std::vector<Point> m_control;     // 3 per segment: anchor, handle out, handle in (of next anchor)
    std::deque<Snapshot> m_trail;
    float m_accumulator = 0.0f;
    float m_hue = 0.0f;
    rs::LineRenderer2D m_lines;
};
