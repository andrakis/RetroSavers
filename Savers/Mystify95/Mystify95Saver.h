#pragma once
#include "Saver.h"
#include "Gfx/LineRenderer2D.h"
#include "Gfx/PostProcess.h"
#include "Gfx/RenderTexture.h"
#include "Gfx/Texture.h"
#include <deque>
#include <vector>

// Mystify Your Mind (Windows 3.1 / 95): two bouncing 4-vertex polygons leaving trails.
struct MystifyShapeSettings {
    bool active = true;
    int lines = 5;              // 1..15 polygons kept on screen
    bool randomColors = true;   // false = cycle between color1 and color2
    COLORREF color1 = RGB(255, 0, 0);
    COLORREF color2 = RGB(0, 0, 255);
};

struct Mystify95Settings {
    MystifyShapeSettings shape[2];
    bool clearScreen = true;    // false = draw over the desktop and never erase

    static Mystify95Settings Load(const rs::Settings& s);
    void Save(rs::Settings& s) const;
    static Mystify95Settings Defaults();
};

class Mystify95Saver : public rs::Saver {
public:
    void Initialize(rs::Device& device, const rs::SaverContext& ctx) override;
    void Update(float dt, double time) override;
    void Render(rs::Device& device, rs::SwapChain& swap) override;
    void Resize(int width, int height) override;
    std::optional<DirectX::XMFLOAT4> ClearColor() const override;

private:
    static constexpr int kVertices = 4;
    static constexpr float kTickRate = 30.0f;

    struct Snapshot {
        DirectX::XMFLOAT2 p[kVertices];
        DirectX::XMFLOAT4 color;
    };
    struct Shape {
        DirectX::XMFLOAT2 pos[kVertices];
        DirectX::XMFLOAT2 vel[kVertices];
        float hue = 0.0f;
        float hueStep = 0.0f;
        float phase = 0.0f;            // two-colour cycling phase
        std::deque<Snapshot> trail;
        std::vector<Snapshot> pending; // snapshots not yet drawn into the persistent target
    };

    void ResetShapes();
    void Tick(Shape& s, const MystifyShapeSettings& cfg);
    void DrawSnapshot(const Snapshot& s);
    void InitPersistentTarget(rs::Device& device);

    Mystify95Settings m_settings;
    rs::SaverContext m_ctx;
    Shape m_shapes[2];
    float m_accumulator = 0.0f;

    rs::Device* m_device = nullptr;
    rs::LineRenderer2D m_lines;
    rs::PostProcess m_post;
    rs::RenderTexture m_persistent;   // used when clearScreen == false
    bool m_persistentValid = false;
};
