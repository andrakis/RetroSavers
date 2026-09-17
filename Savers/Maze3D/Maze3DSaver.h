#pragma once
#include "Saver.h"
#include "Gfx/Billboard.h"
#include "Gfx/Camera.h"
#include "Gfx/Forward.h"
#include "Gfx/Mesh.h"
#include "Gfx/Texture.h"
#include <string>
#include <vector>

// 3D Maze (Windows 95 Plus! / NT 4): a first-person walk through a brick maze using the
// right-hand rule, with the smiley at the exit, the spinning OpenGL logo, the rock that
// turns the world upside down, rats, and globes.
struct Maze3DSettings {
    std::wstring wallTexture;      // empty = procedural brick
    std::wstring floorTexture;     // empty = procedural planks
    std::wstring ceilingTexture;   // empty = procedural tiles
    bool showRat = true;
    bool showLogo = true;
    int mazeSize = 15;             // odd, 9..31
    int speed = 5;                 // 1..10

    static Maze3DSettings Load(const rs::Settings& s);
    void Save(rs::Settings& s) const;
};

class Maze3DSaver : public rs::Saver {
public:
    void Initialize(rs::Device& device, const rs::SaverContext& ctx) override;
    void Update(float dt, double time) override;
    void Render(rs::Device& device, rs::SwapChain& swap) override;
    void Resize(int width, int height) override;

private:
    enum class State { Deciding, Turning, Moving, FadeOut, FadeIn };

    struct Rat {
        DirectX::XMINT2 cell{}, next{};
        float progress = 0.0f;
        int heading = 0;
    };
    struct Globe { DirectX::XMINT2 cell{}; float phase = 0.0f; };

    // maze
    void Generate();
    bool Open(int x, int z) const;
    void BuildGeometry(rs::Device& device);
    void LoadTextures(rs::Device& device);
    static DirectX::XMINT2 StepCell(const DirectX::XMINT2& c, int heading);
    static float HeadingYaw(int heading) { return heading * DirectX::XM_PIDIV2; }
    DirectX::XMFLOAT3 CellCenter(const DirectX::XMINT2& c, float y) const;
    DirectX::XMINT2 RandomCorridorCell(bool deadEndPreferred);

    // navigation
    void Decide();
    void BeginTurn(int newHeading);
    void BeginMove();
    void OnEnterCell(const DirectX::XMINT2& cell);
    void UpdateRats(float dt);
    int RandomOpenHeading(const DirectX::XMINT2& cell, int avoidHeading);

    // draw
    void DrawObjects(rs::Device& device, float fade);

    Maze3DSettings m_settings;
    rs::SaverContext m_ctx;
    rs::Device* m_device = nullptr;
    int m_size = 15;
    std::vector<uint8_t> m_cells;          // 1 = wall
    DirectX::XMINT2 m_start{}, m_exit{}, m_rockCell{ -1, -1 }, m_logoCell{ -1, -1 };
    int m_startHeading = 0;
    std::vector<Rat> m_rats;
    std::vector<Globe> m_globes;

    // walker
    State m_state = State::FadeIn;
    DirectX::XMINT2 m_cell{}, m_nextCell{};
    int m_heading = 0;
    float m_yaw = 0.0f, m_yawFrom = 0.0f, m_yawTo = 0.0f;
    float m_roll = 0.0f, m_rollTarget = 0.0f;
    float m_t = 0.0f, m_duration = 1.0f;
    float m_fade = 0.0f;
    float m_speedScale = 1.0f;
    double m_time = 0.0;
    bool m_flipped = false;

    // gfx
    rs::Forward m_forward;
    rs::Billboard m_billboard;
    rs::Camera m_camera;
    rs::Mesh m_walls, m_floor, m_ceiling, m_cube, m_rock, m_globe;
    rs::Texture m_wallTex, m_floorTex, m_ceilingTex, m_logoTex, m_globeTex, m_smileyTex, m_doorTex, m_ratTex;
};
