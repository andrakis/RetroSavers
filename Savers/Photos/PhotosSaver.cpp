#include "PhotosSaver.h"
#include "Gfx/Device.h"
#include "Gfx/ImageLoader.h"
#include "Gfx/SwapChain.h"
#include "Gfx/TextureFactory.h"
#include "Util/MathUtil.h"
#include "Shaders/Photos_ps.h"
#include <objbase.h>
#include <shlobj.h>
#include <algorithm>
#include <cmath>
#include <random>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")

using namespace DirectX;
using namespace rs;

// ---------------------------------------------------------------- settings

std::wstring PhotosSettings::DefaultFolder() {
    PWSTR path = nullptr;
    std::wstring result;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Pictures, 0, nullptr, &path)) && path) result = path;
    if (path) CoTaskMemFree(path);
    return result;
}

PhotosSettings PhotosSettings::Load(const Settings& s) {
    PhotosSettings v;
    v.folder = s.GetString(L"Folder", L"");
    v.interval = Clamp(s.GetInt(L"Interval", v.interval), 3, 60);
    v.transition = Clamp(s.GetInt(L"Transition", v.transition), 0, 2);
    v.fit = Clamp(s.GetInt(L"Fit", v.fit), 0, 1);
    v.shuffle = s.GetBool(L"Shuffle", v.shuffle);
    v.subfolders = s.GetBool(L"Subfolders", v.subfolders);
    v.showFileName = s.GetBool(L"ShowFileName", v.showFileName);
    v.kenBurns = s.GetBool(L"KenBurns", v.kenBurns);
    return v;
}

void PhotosSettings::Save(Settings& s) const {
    s.SetString(L"Folder", folder);
    s.SetInt(L"Interval", interval);
    s.SetInt(L"Transition", transition);
    s.SetInt(L"Fit", fit);
    s.SetBool(L"Shuffle", shuffle);
    s.SetBool(L"Subfolders", subfolders);
    s.SetBool(L"ShowFileName", showFileName);
    s.SetBool(L"KenBurns", kenBurns);
}

// ---------------------------------------------------------------- enumeration (worker thread)

namespace {

bool IsImageFile(const wchar_t* name) {
    static const wchar_t* exts[] = { L".jpg", L".jpeg", L".png", L".bmp", L".gif", L".tif", L".tiff", L".webp", L".heic", L".jfif" };
    const wchar_t* dot = wcsrchr(name, L'.');
    if (!dot) return false;
    for (const wchar_t* e : exts)
        if (_wcsicmp(dot, e) == 0) return true;
    return false;
}

void Enumerate(const std::wstring& dir, bool recurse, std::vector<std::wstring>& out, int depth, const std::atomic<bool>& stop) {
    if (depth > 12 || stop) return;
    WIN32_FIND_DATAW fd{};
    HANDLE h = FindFirstFileExW((dir + L"\\*").c_str(), FindExInfoBasic, &fd, FindExSearchNameMatch, nullptr, FIND_FIRST_EX_LARGE_FETCH);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (stop) break;
        if (fd.cFileName[0] == L'.' && (fd.cFileName[1] == 0 || (fd.cFileName[1] == L'.' && fd.cFileName[2] == 0))) continue;
        if (fd.dwFileAttributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)) continue;
        std::wstring full = dir + L"\\" + fd.cFileName;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (recurse && !(fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) Enumerate(full, true, out, depth + 1, stop);
        } else if (IsImageFile(fd.cFileName)) {
            out.push_back(full);
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

} // namespace

// ---------------------------------------------------------------- lifecycle

PhotosSaver::~PhotosSaver() {
    m_stop = true;
    m_cv.notify_all();
    if (m_worker.joinable()) m_worker.join();
}

void PhotosSaver::Initialize(Device& device, const SaverContext& ctx) {
    m_ctx = ctx;
    m_device = &device;
    m_settings = PhotosSettings::Load(*ctx.settings);
    if (m_settings.folder.empty()) m_settings.folder = PhotosSettings::DefaultFolder();
    m_holdTime = static_cast<float>(m_settings.interval);
    m_maxDim = std::max(2 * std::max(m_ctx.width, m_ctx.height), 1024);

    m_post.Create(device);
    ThrowIfFailed(device.Get()->CreatePixelShader(g_Photos_ps, sizeof(g_Photos_ps), nullptr, &m_ps), "CreatePixelShader(Photos)");
    m_cb.Create(device);
    m_sprites.Create(device);

    m_requested = true;   // first image
    m_worker = std::thread([this] { WorkerMain(); });
}

void PhotosSaver::WorkerMain() {
    const HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED | COINIT_DISABLE_OLE1DDE);
    std::mt19937 rng{ std::random_device{}() };

    std::vector<std::wstring> files;
    Enumerate(m_settings.folder, m_settings.subfolders, files, 0, m_stop);
    if (m_settings.shuffle) std::shuffle(files.begin(), files.end(), rng);
    else std::sort(files.begin(), files.end(), [](const std::wstring& a, const std::wstring& b) { return _wcsicmp(a.c_str(), b.c_str()) < 0; });
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_files = std::move(files);
        m_scanned = true;
    }
    m_cv.notify_all();

    while (!m_stop) {
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait(lock, [&] { return m_stop || (m_requested && !m_decoded); });
            if (m_stop) break;
            m_requested = false;
        }
        Decoded d;
        bool ok = false;
        for (int attempt = 0; attempt < 25 && !m_stop; ++attempt) {
            std::wstring path;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                if (m_files.empty()) break;
                path = m_files[m_nextFile];
                if (++m_nextFile >= m_files.size()) {
                    m_nextFile = 0;
                    if (m_settings.shuffle) std::shuffle(m_files.begin(), m_files.end(), rng);
                }
            }
            if (auto img = ImageLoader::Load(path, m_maxDim)) {
                d.image = std::move(*img);
                size_t slash = path.find_last_of(L"\\/");
                d.name = slash == std::wstring::npos ? path : path.substr(slash + 1);
                ok = true;
                break;
            }
        }
        std::lock_guard<std::mutex> lock(m_mutex);
        if (ok) m_decoded = std::move(d);
    }
    if (SUCCEEDED(com)) CoUninitialize();
}

void PhotosSaver::RequestNext() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_requested = true;
    }
    m_cv.notify_all();
}

bool PhotosSaver::TakeDecoded(Decoded& out) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_decoded) return false;
    out = std::move(*m_decoded);
    m_decoded.reset();
    return true;
}

void PhotosSaver::ShowNext(Device& device, Decoded&& d) {
    Rng& rng = *m_ctx.rng;
    int slot = (m_current + 1) & 1;
    Slide& s = m_slides[slot];
    s.texture.FromImage(device, d.image, true);
    s.aspect = static_cast<float>(d.image.width) / std::max(d.image.height, 1);
    s.name = d.name;
    s.valid = true;
    // Ken Burns: a gentle zoom in or out with a small drift.
    bool zoomIn = rng.Chance(0.5f);
    s.zoomFrom = zoomIn ? 1.0f : 1.12f;
    s.zoomTo = zoomIn ? 1.12f : 1.0f;
    s.panFrom = { rng.Range(-0.02f, 0.02f), rng.Range(-0.02f, 0.02f) };
    s.panTo = { rng.Range(-0.02f, 0.02f), rng.Range(-0.02f, 0.02f) };
    m_current = slot;
    m_transition = (m_settings.transition == PhotosSettings::Cut || !m_slides[slot ^ 1].valid) ? 1.0f : 0.0f;
    m_slideTime = 0.0f;
    if (m_settings.showFileName) UpdateCaption(device, s.name);
}

void PhotosSaver::UpdateCaption(Device& device, const std::wstring& text) {
    if (text == m_captionText && m_caption.Valid()) return;
    int px = static_cast<int>(std::max(12.0f, m_ctx.height * 0.02f) * m_ctx.dpiScale);
    m_caption.FromImage(device, TextureFactory::TextImage(text, TextureFactory::MakeLogFont(L"Segoe UI", px)), false);
    m_captionText = text;
}

void PhotosSaver::Update(float dt, double) {
    Decoded d;
    if (m_waiting) {
        if (TakeDecoded(d) && m_device) {
            ShowNext(*m_device, std::move(d));
            m_waiting = false;
            RequestNext();   // prefetch
        }
        m_noticeTimer += dt;
        return;
    }
    float fadeSeconds = m_settings.transition == PhotosSettings::ThroughBlack ? 2.0f : 1.5f;
    m_transition = std::min(1.0f, m_transition + dt / fadeSeconds);
    m_slideTime += dt;
    if (m_slideTime >= m_holdTime && m_transition >= 1.0f) {
        if (TakeDecoded(d) && m_device) {
            ShowNext(*m_device, std::move(d));
            RequestNext();
        }
    }
}

XMFLOAT4 PhotosSaver::Transform(const Slide& s, float t) const {
    float vp = static_cast<float>(m_ctx.width) / std::max(m_ctx.height, 1);
    float sx, sy;
    bool wider = s.aspect > vp;
    if (m_settings.fit == PhotosSettings::FitInside) { sx = wider ? 1.0f : vp / s.aspect; sy = wider ? s.aspect / vp : 1.0f; }
    else { sx = wider ? vp / s.aspect : 1.0f; sy = wider ? 1.0f : s.aspect / vp; }
    float ox = 0, oy = 0;
    if (m_settings.kenBurns) {
        float k = Smoothstep(0.0f, 1.0f, t);
        float z = Lerp(s.zoomFrom, s.zoomTo, k);
        sx /= z;
        sy /= z;
        if (m_settings.fit == PhotosSettings::Fill) {   // panning would expose the edges when fitting
            ox = Lerp(s.panFrom.x, s.panTo.x, k);
            oy = Lerp(s.panFrom.y, s.panTo.y, k);
        }
    }
    return { sx, sy, ox, oy };
}

void PhotosSaver::Render(Device& device, SwapChain&) {
    ID3D11DeviceContext* ctx = device.Ctx();
    const States& states = m_post.GetStates();

    if (m_current < 0) {
        // Nothing decoded yet: black, and a notice once the scan came back empty (or is slow).
        m_post.Fill(ctx, { 0, 0, 0, 1 });
        bool empty;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            empty = m_scanned && m_files.empty();
        }
        if (empty || m_noticeTimer > 4.0f) {
            std::wstring text = empty ? L"No pictures found in " + m_settings.folder : L"Loading pictures\x2026";
            UpdateCaption(device, text);
            float w = static_cast<float>(m_caption.Width()), h = static_cast<float>(m_caption.Height());
            m_sprites.Begin(m_ctx.width, m_ctx.height);
            m_sprites.Push(m_ctx.width * 0.5f, m_ctx.height * 0.5f, w, h, { 0.6f, 0.6f, 0.6f, 1 });
            m_sprites.End(device, &m_caption, states.AlphaBlend());
        }
        return;
    }

    const Slide& cur = m_slides[m_current];
    const Slide& prev = m_slides[m_current ^ 1];
    float total = m_holdTime + 1.5f;
    PhotoCB cb{};
    cb.xformA = prev.valid ? Transform(prev, std::min(1.0f, (m_holdTime + m_transition * 1.5f) / total)) : XMFLOAT4{ 1, 1, 0, 0 };
    cb.xformB = Transform(cur, std::min(1.0f, m_slideTime / total));
    cb.mix = { m_transition, m_settings.transition == PhotosSettings::ThroughBlack ? 1.0f : 0.0f, prev.valid ? 1.0f : 0.0f, 1.0f };
    m_cb.Update(ctx, cb);
    m_cb.BindPS(ctx, 0);
    states.Set2D(ctx, states.Opaque());
    ID3D11ShaderResourceView* srvs[2] = { prev.valid ? prev.texture.SRV() : cur.texture.SRV(), cur.texture.SRV() };
    ctx->PSSetShaderResources(0, 2, srvs);
    ID3D11SamplerState* samp = states.LinearClamp();
    ctx->PSSetSamplers(0, 1, &samp);
    m_post.Draw(ctx, m_ps.Get());
    ID3D11ShaderResourceView* none[2] = { nullptr, nullptr };
    ctx->PSSetShaderResources(0, 2, none);

    if (m_settings.showFileName && m_caption.Valid()) {
        float margin = 14.0f * m_ctx.dpiScale;
        float w = static_cast<float>(m_caption.Width()), h = static_cast<float>(m_caption.Height());
        float a = std::min(1.0f, m_transition * 2.0f);
        m_sprites.Begin(m_ctx.width, m_ctx.height);
        m_sprites.Push(margin + w * 0.5f + 1, m_ctx.height - margin - h * 0.5f + 1, w, h, { 0, 0, 0, 0.8f * a });   // shadow
        m_sprites.Push(margin + w * 0.5f, m_ctx.height - margin - h * 0.5f, w, h, { 1, 1, 1, a });
        m_sprites.End(device, &m_caption, states.AlphaBlend());
    }
}

void PhotosSaver::Resize(int width, int height) {
    m_ctx.width = width;
    m_ctx.height = height;
    m_ctx.viewport = { 0, 0, width, height };
}
