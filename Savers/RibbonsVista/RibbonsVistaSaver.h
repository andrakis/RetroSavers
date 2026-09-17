#pragma once
#include "Saver.h"
#include "Gfx/LineRenderer2D.h"
#include "Gfx/TrailBuffer.h"
#include <vector>

// Ribbons (Windows Vista / 7): wide glossy ribbons twist their way across the screen, leaving
// their trails behind until the picture slowly fades and starts over.
struct RibbonsVistaSettings {
    enum ColorMode { Rainbow = 0, Pastel = 1, Single = 2 };

    int count = 4;            // 1..10
    int width = 5;            // 1..10
    int persistence = 7;      // 1..10
    int speed = 5;            // 1..10
    int colorMode = Rainbow;

    static RibbonsVistaSettings Load(const rs::Settings& s);
    void Save(rs::Settings& s) const;
};

class RibbonsVistaSaver : public rs::Saver {
public:
    void Initialize(rs::Device& device, const rs::SaverContext& ctx) override;
    void Update(float dt, double time) override;
    void Render(rs::Device& device, rs::SwapChain& swap) override;
    void Resize(int width, int height) override;
    std::optional<DirectX::XMFLOAT4> ClearColor() const override { return std::nullopt; }

private:
    struct Ribbon {
        DirectX::XMFLOAT2 pos, prevPos;
        float heading;            // radians
        float twist, twistRate;   // width modulation phase
        float hue, hueRate;
        float seed;               // noise offset for steering
        float prevHalfWidth;
        DirectX::XMFLOAT2 prevNormal;
        bool hasPrev;
    };

    void SpawnRibbon(Ribbon& r, bool anywhere);
    DirectX::XMFLOAT4 Shade(const Ribbon& r, float across, float facing) const;

    RibbonsVistaSettings m_settings;
    rs::SaverContext m_ctx;
    std::vector<Ribbon> m_ribbons;
    double m_time = 0.0;
    float m_fadeOut = 0.0f;     // > 0 while the periodic wipe is running
    float m_wipeTimer = 0.0f;

    rs::LineRenderer2D m_lines;
    rs::TrailBuffer m_trail;
};
