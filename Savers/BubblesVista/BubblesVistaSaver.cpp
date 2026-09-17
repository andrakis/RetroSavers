#include "BubblesVistaSaver.h"
#include "Gfx/Device.h"
#include "Gfx/SwapChain.h"
#include "Host/Host.h"
#include "Util/Color.h"
#include "Util/MathUtil.h"
#include "Shaders/Bubble_ps.h"
#include <algorithm>
#include <cmath>

using namespace DirectX;
using namespace rs;

// ---------------------------------------------------------------- settings

BubblesVistaSettings BubblesVistaSettings::Load(const Settings& s) {
    BubblesVistaSettings v;
    v.count = Clamp(s.GetInt(L"Count", v.count), 1, 40);
    v.size = Clamp(s.GetInt(L"Size", v.size), 1, 10);
    v.speed = Clamp(s.GetInt(L"Speed", v.speed), 1, 10);
    v.showOnDesktop = s.GetBool(L"ShowOnDesktop", v.showOnDesktop);
    v.colorMode = Clamp(s.GetInt(L"ColorMode", v.colorMode), 0, 2);
    v.color = static_cast<COLORREF>(s.GetInt(L"Color", static_cast<int>(v.color)));
    v.wobble = s.GetBool(L"Wobble", v.wobble);
    return v;
}

void BubblesVistaSettings::Save(Settings& s) const {
    s.SetInt(L"Count", count);
    s.SetInt(L"Size", size);
    s.SetInt(L"Speed", speed);
    s.SetBool(L"ShowOnDesktop", showOnDesktop);
    s.SetInt(L"ColorMode", colorMode);
    s.SetInt(L"Color", static_cast<int>(color));
    s.SetBool(L"Wobble", wobble);
}

// ---------------------------------------------------------------- lifecycle

void BubblesVistaSaver::Initialize(Device& device, const SaverContext& ctx) {
    m_ctx = ctx;
    m_settings = BubblesVistaSettings::Load(*ctx.settings);
    m_sprites.Create(device);
    ThrowIfFailed(device.Get()->CreatePixelShader(g_Bubble_ps, sizeof(g_Bubble_ps), nullptr, &m_bubblePs), "CreatePixelShader(Bubble)");
    m_cb.Create(device);
    CreateBackground(device);
    Spawn();
}

void BubblesVistaSaver::CreateBackground(Device& device) {
    // Run mode with the desktop captured: crop this viewport out of it. Otherwise a dark gradient.
    const Image* desk = m_settings.showOnDesktop ? Host::DesktopImage() : nullptr;
    if (desk) {
        Image crop(m_ctx.width, m_ctx.height, 0xFF000000u);
        for (int y = 0; y < m_ctx.height; ++y) {
            int sy = m_ctx.viewport.top + y;
            if (sy < 0 || sy >= desk->height) continue;
            for (int x = 0; x < m_ctx.width; ++x) {
                int sx = m_ctx.viewport.left + x;
                if (sx < 0 || sx >= desk->width) continue;
                crop.At(x, y) = desk->At(sx, sy);
            }
        }
        m_background.FromImage(device, crop, false);
        m_hasDesktop = true;
    } else {
        Image grad(1, 256);
        for (int y = 0; y < 256; ++y) {
            float t = y / 255.0f;
            grad.At(0, y) = PackRgbaF(0.02f + 0.06f * t, 0.03f + 0.08f * t, 0.10f + 0.18f * t);
        }
        m_background.FromImage(device, grad, false);
        m_hasDesktop = false;
    }
}

void BubblesVistaSaver::Spawn() {
    Rng& rng = *m_ctx.rng;
    const float w = static_cast<float>(m_ctx.width), h = static_cast<float>(m_ctx.height);
    const float base = h * (0.03f + 0.012f * m_settings.size);
    const float speed = h * (0.02f + 0.02f * m_settings.speed);
    m_bubbles.clear();
    for (int i = 0; i < m_settings.count; ++i) {
        Bubble b;
        b.r = base * rng.Range(0.6f, 1.4f);
        b.x = rng.Range(b.r, std::max(b.r, w - b.r));
        b.y = rng.Range(b.r, std::max(b.r, h - b.r));
        float dir = rng.Range(0.0f, kTwoPi), s = speed * rng.Range(0.6f, 1.2f);
        b.vx = std::cos(dir) * s;
        b.vy = std::sin(dir) * s;
        b.phase = rng.Float();
        b.wobblePhase = rng.Range(0.0f, kTwoPi);
        b.wobbleRate = rng.Range(1.5f, 3.0f);
        m_bubbles.push_back(b);
    }
}

void BubblesVistaSaver::Update(float dt, double) {
    m_time += dt;
    const float w = static_cast<float>(m_ctx.width), h = static_cast<float>(m_ctx.height);
    for (auto& b : m_bubbles) {
        b.x += b.vx * dt;
        b.y += b.vy * dt;
        if (b.x < b.r) { b.x = b.r; b.vx = std::fabs(b.vx); }
        if (b.x > w - b.r) { b.x = w - b.r; b.vx = -std::fabs(b.vx); }
        if (b.y < b.r) { b.y = b.r; b.vy = std::fabs(b.vy); }
        if (b.y > h - b.r) { b.y = h - b.r; b.vy = -std::fabs(b.vy); }
        b.phase = Wrap01(b.phase + dt * 0.02f);
    }
    // Elastic pairwise collisions, mass proportional to area.
    for (size_t i = 0; i < m_bubbles.size(); ++i) {
        for (size_t j = i + 1; j < m_bubbles.size(); ++j) {
            Bubble& a = m_bubbles[i];
            Bubble& b = m_bubbles[j];
            float dx = b.x - a.x, dy = b.y - a.y;
            float dist2 = dx * dx + dy * dy, minD = a.r + b.r;
            if (dist2 >= minD * minD || dist2 < 1e-6f) continue;
            float dist = std::sqrt(dist2);
            float nx = dx / dist, ny = dy / dist;
            // Separate.
            float overlap = minD - dist;
            float ma = a.r * a.r, mb = b.r * b.r, mt = ma + mb;
            a.x -= nx * overlap * (mb / mt);
            a.y -= ny * overlap * (mb / mt);
            b.x += nx * overlap * (ma / mt);
            b.y += ny * overlap * (ma / mt);
            // Exchange the normal velocity components (1D elastic collision along n).
            float va = a.vx * nx + a.vy * ny, vb = b.vx * nx + b.vy * ny;
            if (va - vb <= 0.0f) continue;   // already separating
            float va2 = (va * (ma - mb) + 2.0f * mb * vb) / mt;
            float vb2 = (vb * (mb - ma) + 2.0f * ma * va) / mt;
            a.vx += (va2 - va) * nx; a.vy += (va2 - va) * ny;
            b.vx += (vb2 - vb) * nx; b.vy += (vb2 - vb) * ny;
        }
    }
}

void BubblesVistaSaver::Render(Device& device, SwapChain&) {
    ID3D11DeviceContext* ctx = device.Ctx();
    const States& states = m_sprites.GetStates();
    const float w = static_cast<float>(m_ctx.width), h = static_cast<float>(m_ctx.height);

    // Background: the desktop crop or the gradient strip stretched over the viewport.
    m_sprites.Begin(m_ctx.width, m_ctx.height);
    m_sprites.Push(w * 0.5f, h * 0.5f, w, h, { 1, 1, 1, 1 });
    m_sprites.End(device, &m_background, states.Opaque());

    BubbleCB cb{};
    cb.rect = { static_cast<float>(m_ctx.viewport.left), static_cast<float>(m_ctx.viewport.top), w, h };
    cb.params = { m_hasDesktop ? 1.0f : 0.0f, 0.06f, static_cast<float>(m_settings.colorMode), 0 };
    cb.light = { -0.45f, 0.55f, 0.7f, 70.0f };
    cb.tint = FromColorRef(m_settings.color);
    m_cb.Update(ctx, cb);
    m_cb.BindPS(ctx, 0);
    m_background.BindPS(ctx, 1);

    m_sprites.Begin(m_ctx.width, m_ctx.height);
    for (const auto& b : m_bubbles) {
        float wob = m_settings.wobble ? 0.04f * std::sin(static_cast<float>(m_time) * b.wobbleRate + b.wobblePhase) : 0.0f;
        float rx = b.r * (1.0f + wob), ry = b.r * (1.0f - wob);
        m_sprites.Push(b.x, b.y, rx * 2.0f, ry * 2.0f, { b.phase, 0, 0, 0.9f });
    }
    m_sprites.End(device, nullptr, states.PremultipliedAlpha(), m_bubblePs.Get(), states.LinearClamp());
    ID3D11ShaderResourceView* null = nullptr;
    ctx->PSSetShaderResources(1, 1, &null);
}

void BubblesVistaSaver::Resize(int width, int height) {
    m_ctx.width = width;
    m_ctx.height = height;
    m_ctx.viewport = { 0, 0, width, height };
    Spawn();
}
