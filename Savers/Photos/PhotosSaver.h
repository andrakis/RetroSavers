#pragma once
#include "Saver.h"
#include "Gfx/ConstantBuffer.h"
#include "Gfx/PostProcess.h"
#include "Gfx/SpriteBatch2D.h"
#include "Gfx/Texture.h"
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

// Photos: a slideshow of a folder. A worker thread enumerates and decodes the next image
// (WIC, downscaled to about twice the viewport) while the render thread shows the current one.
struct PhotosSettings {
    enum Transition { Crossfade = 0, Cut = 1, ThroughBlack = 2 };
    enum Fit { FitInside = 0, Fill = 1 };

    std::wstring folder;          // empty = the user's Pictures folder
    int interval = 8;             // 3..60 seconds
    int transition = Crossfade;
    int fit = FitInside;
    bool shuffle = true;
    bool subfolders = true;
    bool showFileName = false;
    bool kenBurns = true;

    static PhotosSettings Load(const rs::Settings& s);
    void Save(rs::Settings& s) const;
    static std::wstring DefaultFolder();
};

class PhotosSaver : public rs::Saver {
public:
    ~PhotosSaver() override;
    void Initialize(rs::Device& device, const rs::SaverContext& ctx) override;
    void Update(float dt, double time) override;
    void Render(rs::Device& device, rs::SwapChain& swap) override;
    void Resize(int width, int height) override;
    std::optional<DirectX::XMFLOAT4> ClearColor() const override { return std::nullopt; }

private:
    struct Slide {
        rs::Texture texture;
        float aspect = 1.0f;
        std::wstring name;
        DirectX::XMFLOAT2 panFrom{ 0, 0 }, panTo{ 0, 0 };   // Ken Burns
        float zoomFrom = 1.0f, zoomTo = 1.0f;
        bool valid = false;
    };
    struct PhotoCB { DirectX::XMFLOAT4 xformA, xformB, mix; };
    struct Decoded { rs::Image image; std::wstring name; };

    void WorkerMain();
    void RequestNext();
    bool TakeDecoded(Decoded& out);
    void ShowNext(rs::Device& device, Decoded&& d);
    DirectX::XMFLOAT4 Transform(const Slide& s, float t) const;
    void UpdateCaption(rs::Device& device, const std::wstring& text);

    PhotosSettings m_settings;
    rs::SaverContext m_ctx;
    rs::Device* m_device = nullptr;

    // Worker state.
    std::thread m_worker;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::atomic<bool> m_stop{ false };
    bool m_requested = false;
    std::optional<Decoded> m_decoded;
    bool m_scanned = false;       // enumeration finished (possibly with no files)
    std::vector<std::wstring> m_files;
    size_t m_nextFile = 0;
    int m_maxDim = 2048;

    // Display state.
    Slide m_slides[2];
    int m_current = -1;           // index into m_slides of the slide being shown
    float m_transition = 1.0f;    // 0..1 progress from the previous slide to the current one
    float m_slideTime = 0.0f;     // seconds the current slide has been up
    float m_holdTime = 8.0f;
    bool m_waiting = true;        // showing nothing yet / waiting for the first decode
    float m_noticeTimer = 0.0f;

    rs::PostProcess m_post;
    rs::ComPtr<ID3D11PixelShader> m_ps;
    rs::ConstantBuffer<PhotoCB> m_cb;
    rs::SpriteBatch2D m_sprites;
    rs::Texture m_caption;
    std::wstring m_captionText;
};
