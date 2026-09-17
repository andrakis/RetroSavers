#pragma once
#include "Saver.h"
#include "Gfx/SpriteBatch2D.h"
#include "Gfx/Texture.h"
#include <string>
#include <vector>

// Flying Windows (Windows 3.1 / 95): emblems stream out of the vanishing point towards the
// viewer. The emblem is a generic four-pane flag; any image can be substituted.
struct FlyingWindowsSettings {
    int density = 40;             // 10..200 emblems
    int warpSpeed = 5;            // 1..10
    std::wstring imagePath;       // empty = procedural emblem
    COLORREF pane[4] = { RGB(235, 50, 35), RGB(60, 180, 50), RGB(30, 110, 230), RGB(250, 200, 30) };
    bool spin = false;
    bool stars = true;

    static FlyingWindowsSettings Load(const rs::Settings& s);
    void Save(rs::Settings& s) const;
};

class FlyingWindowsSaver : public rs::Saver {
public:
    void Initialize(rs::Device& device, const rs::SaverContext& ctx) override;
    void Update(float dt, double time) override;
    void Render(rs::Device& device, rs::SwapChain& swap) override;
    void Resize(int width, int height) override;

private:
    struct Item { float x, y, z, angle, spin; };
    // Emblems spawn in a narrow cone about the vanishing point so they grow large before they
    // leave the edges; stars use the whole plane.
    static constexpr float kEmblemSpread = 0.3f;

    void LoadEmblem(rs::Device& device);
    void Respawn(Item& it, bool anywhere, float spread);

    FlyingWindowsSettings m_settings;
    rs::SaverContext m_ctx;
    std::vector<Item> m_items;
    std::vector<Item> m_stars;
    std::vector<const Item*> m_order;   // far to near for this frame

    rs::SpriteBatch2D m_sprites;
    rs::Texture m_emblem;
    float m_aspect = 1.0f;              // emblem width / height
};
