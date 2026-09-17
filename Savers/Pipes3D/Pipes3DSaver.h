#pragma once
#include "Saver.h"
#include "Gfx/Camera.h"
#include "Gfx/ConstantBuffer.h"
#include "Gfx/Forward.h"
#include "Gfx/Mesh.h"
#include "Gfx/Texture.h"
#include <string>
#include <vector>

// 3D Pipes (Windows NT 4 / 95 Plus!): pipes grow through a grid, turning with elbow or
// ball joints, occasionally sprouting the Utah teapot.
struct Pipes3DSettings {
    enum JointType { Elbow = 0, Ball = 1, Mixed = 2, Cycle = 3 };
    enum Surface { Solid = 0, Textured = 1 };

    bool multiple = true;         // Single = 1 pipe, Multiple = 4
    int jointType = Mixed;
    int surface = Solid;
    std::wstring texturePath;     // empty = procedural
    int resolution = 16;          // cylinder slices 6..32
    int speed = 5;                // 1..10
    bool smoothGrowth = false;

    static Pipes3DSettings Load(const rs::Settings& s);
    void Save(rs::Settings& s) const;
};

class Pipes3DSaver : public rs::Saver {
public:
    void Initialize(rs::Device& device, const rs::SaverContext& ctx) override;
    void Update(float dt, double time) override;
    void Render(rs::Device& device, rs::SwapChain& swap) override;
    void Resize(int width, int height) override;

private:
    static constexpr int kW = 24, kH = 18, kD = 24;
    static constexpr float kPipeRadius = 0.28f;
    static constexpr float kBallRadius = 0.42f;
    static constexpr float kBendRadius = 0.5f;      // elbow centreline radius: face to face
    static constexpr float kTeapotChance = 0.001f;  // per turn
    static constexpr int kMaxPipes = 4;

    struct Pipe {
        DirectX::XMINT3 cell{};
        int dir = 0;                    // 0..5 = +x -x +y -y +z -z
        DirectX::XMFLOAT4 color{};
        int joint = 0;                  // resolved Elbow(0) / Ball(1) for this pipe
        bool alive = false;
        int lastCylinder = -1;          // index of the last committed link instance
        DirectX::XMFLOAT3 lastLinkStart{};
        bool hasHead = false;
        DirectX::XMFLOAT3 headStart{}, headEnd{};
    };

    struct InstanceList {
        std::vector<rs::InstanceData> items;
        rs::DynamicVertexBuffer<rs::InstanceData> buffer;
        bool dirty = false;
        void Clear() { items.clear(); dirty = true; }
    };

    void ResetScene();
    void SetupCamera();
    bool SpawnPipe(Pipe& p);
    void Advance(Pipe& p);
    int ChooseDirection(const Pipe& p);
    bool Occupied(const DirectX::XMINT3& c) const;
    void Occupy(const DirectX::XMINT3& c);
    bool InGrid(const DirectX::XMINT3& c) const;
    static DirectX::XMINT3 Step(const DirectX::XMINT3& c, int dir);
    static DirectX::XMFLOAT3 Center(const DirectX::XMINT3& c);
    static DirectX::XMFLOAT3 DirVec(int dir);
    DirectX::XMFLOAT4 RandomColor();

    int AddCylinder(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b, const DirectX::XMFLOAT4& color);
    void SetCylinder(int index, const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b);
    void AddSphere(const DirectX::XMFLOAT3& c, float radius, const DirectX::XMFLOAT4& color);
    void AddElbow(const DirectX::XMFLOAT3& cellCenter, int inDir, int outDir, const DirectX::XMFLOAT4& color);
    void AddTeapot(const DirectX::XMFLOAT3& c, const DirectX::XMFLOAT4& color);
    static DirectX::XMMATRIX CylinderWorld(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b);
    void UploadInstances(rs::Device& device, InstanceList& list);

    Pipes3DSettings m_settings;
    rs::SaverContext m_ctx;
    rs::Device* m_device = nullptr;

    std::vector<uint8_t> m_grid;
    Pipe m_pipes[kMaxPipes];
    int m_pipeCount = 1;
    int m_occupied = 0;
    int m_spawnFailures = 0;
    int m_cycleIndex = 0;
    float m_stepAccumulator = 0.0f;
    float m_stepInterval = 0.1f;
    float m_resetFlash = 0.0f;

    rs::Forward m_forward;
    rs::Camera m_camera;
    rs::Mesh m_cylinder, m_sphere, m_elbow, m_teapot;
    InstanceList m_cylinders, m_spheres, m_elbows, m_teapots;
    rs::Texture m_texture;
    rs::Material m_material;
};
