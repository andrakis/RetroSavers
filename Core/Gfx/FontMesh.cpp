#include "FontMesh.h"
#include "Host/Log.h"
#include <d2d1.h>
#include <dwrite.h>
#include <wrl/client.h>
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <vector>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")

using namespace DirectX;
using Microsoft::WRL::ComPtr;

namespace rs::FontMesh {

namespace {

// Stack-allocated sinks: reference counting is a no-op because the object outlives the call
// that uses it.
template <typename Interface>
class SinkBase : public Interface {
public:
    ULONG STDMETHODCALLTYPE AddRef() override { return 1; }
    ULONG STDMETHODCALLTYPE Release() override { return 1; }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** obj) override {
        if (iid == __uuidof(Interface) || iid == __uuidof(IUnknown)) { *obj = this; return S_OK; }
        *obj = nullptr;
        return E_NOINTERFACE;
    }
};

class TriangleSink : public SinkBase<ID2D1TessellationSink> {
public:
    std::vector<D2D1_TRIANGLE> triangles;
    void STDMETHODCALLTYPE AddTriangles(const D2D1_TRIANGLE* t, UINT32 count) override { triangles.insert(triangles.end(), t, t + count); }
    HRESULT STDMETHODCALLTYPE Close() override { return S_OK; }
};

class ContourSink : public SinkBase<ID2D1SimplifiedGeometrySink> {
public:
    std::vector<std::vector<D2D1_POINT_2F>> contours;
    void STDMETHODCALLTYPE SetFillMode(D2D1_FILL_MODE) override {}
    void STDMETHODCALLTYPE SetSegmentFlags(D2D1_PATH_SEGMENT) override {}
    void STDMETHODCALLTYPE BeginFigure(D2D1_POINT_2F start, D2D1_FIGURE_BEGIN) override { contours.push_back({ start }); }
    void STDMETHODCALLTYPE AddLines(const D2D1_POINT_2F* pts, UINT32 count) override { contours.back().insert(contours.back().end(), pts, pts + count); }
    void STDMETHODCALLTYPE AddBeziers(const D2D1_BEZIER_SEGMENT* b, UINT32 count) override {
        for (UINT32 i = 0; i < count; ++i) contours.back().push_back(b[i].point3);   // not produced with OPTION_LINES
    }
    void STDMETHODCALLTYPE EndFigure(D2D1_FIGURE_END) override {}
    HRESULT STDMETHODCALLTYPE Close() override { return S_OK; }
};

std::vector<UINT32> ToCodepoints(const std::wstring& s) {
    std::vector<UINT32> cps;
    for (size_t i = 0; i < s.size(); ++i) {
        wchar_t c = s[i];
        if (c >= 0xD800 && c <= 0xDBFF && i + 1 < s.size() && s[i + 1] >= 0xDC00 && s[i + 1] <= 0xDFFF) {
            cps.push_back(0x10000 + ((static_cast<UINT32>(c) - 0xD800) << 10) + (static_cast<UINT32>(s[i + 1]) - 0xDC00));
            ++i;
        } else {
            cps.push_back(c);
        }
    }
    return cps;
}

ComPtr<IDWriteFontFace> ResolveFace(IDWriteFactory* dw, const LOGFONTW& lf) {
    ComPtr<IDWriteGdiInterop> interop;
    if (FAILED(dw->GetGdiInterop(&interop))) return nullptr;
    const wchar_t* fallbacks[] = { nullptr, L"Segoe UI", L"Arial" };
    for (const wchar_t* fb : fallbacks) {
        LOGFONTW l = lf;
        if (fb) wcscpy_s(l.lfFaceName, fb);
        ComPtr<IDWriteFont> font;
        ComPtr<IDWriteFontFace> face;
        if (SUCCEEDED(interop->CreateFontFromLOGFONT(&l, &font)) && SUCCEEDED(font->CreateFontFace(&face))) return face;
    }
    return nullptr;
}

} // namespace

MeshData Build(const std::wstring& text, const LOGFONTW& lf, float emSize, float depth, float tolerance) {
    MeshData out;
    if (text.empty()) return out;
    tolerance = std::max(tolerance, 0.0005f);

    ComPtr<IDWriteFactory> dw;
    if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(dw.GetAddressOf())))) {
        LogLine(L"FontMesh: DWriteCreateFactory failed");
        return out;
    }
    ComPtr<IDWriteFontFace> face = ResolveFace(dw.Get(), lf);
    if (!face) { LogLine(L"FontMesh: no usable font face"); return out; }

    // Glyph run in em units (font size 1): indices + design advances scaled to em.
    std::vector<UINT32> cps = ToCodepoints(text);
    const UINT32 n = static_cast<UINT32>(cps.size());
    std::vector<UINT16> glyphs(n);
    if (FAILED(face->GetGlyphIndices(cps.data(), n, glyphs.data()))) return out;
    DWRITE_FONT_METRICS fm{};
    face->GetMetrics(&fm);
    std::vector<DWRITE_GLYPH_METRICS> gm(n);
    if (FAILED(face->GetDesignGlyphMetrics(glyphs.data(), n, gm.data(), FALSE))) return out;
    std::vector<FLOAT> advances(n);
    for (UINT32 i = 0; i < n; ++i) advances[i] = static_cast<FLOAT>(gm[i].advanceWidth) / fm.designUnitsPerEm;

    ComPtr<ID2D1Factory> d2d;
    if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, d2d.GetAddressOf()))) {
        LogLine(L"FontMesh: D2D1CreateFactory failed");
        return out;
    }
    ComPtr<ID2D1PathGeometry> raw;
    ComPtr<ID2D1GeometrySink> sink;
    if (FAILED(d2d->CreatePathGeometry(&raw)) || FAILED(raw->Open(&sink))) return out;
    sink->SetFillMode(D2D1_FILL_MODE_WINDING);   // TrueType outlines are non-zero winding
    HRESULT hr = face->GetGlyphRunOutline(1.0f, glyphs.data(), advances.data(), nullptr, n, FALSE, FALSE, sink.Get());
    sink->Close();
    if (FAILED(hr)) { LogLine(L"FontMesh: GetGlyphRunOutline failed"); return out; }

    // Union overlapping glyph parts and resolve winding into plain outlines (holes reversed).
    ComPtr<ID2D1PathGeometry> outlined;
    ComPtr<ID2D1GeometrySink> osink;
    if (FAILED(d2d->CreatePathGeometry(&outlined)) || FAILED(outlined->Open(&osink))) return out;
    hr = raw->Outline(nullptr, tolerance, osink.Get());
    osink->Close();
    if (FAILED(hr)) { LogLine(L"FontMesh: Outline failed"); return out; }

    TriangleSink caps;
    if (FAILED(outlined->Tessellate(nullptr, tolerance, &caps))) { LogLine(L"FontMesh: Tessellate failed"); return out; }
    ContourSink contours;
    if (FAILED(outlined->Simplify(D2D1_GEOMETRY_SIMPLIFICATION_OPTION_LINES, nullptr, tolerance, &contours))) { LogLine(L"FontMesh: Simplify failed"); return out; }
    if (caps.triangles.empty()) return out;

    // D2D is y-down; the mesh is y-up. Positions scale by emSize, depth is symmetric about z = 0.
    const float hd = depth * 0.5f;
    auto world = [&](const D2D1_POINT_2F& p, float z) { return XMFLOAT3{ p.x * emSize, -p.y * emSize, z }; };
    auto capUv = [&](const D2D1_POINT_2F& p) { return XMFLOAT2{ p.x, p.y }; };

    // Caps.
    for (const D2D1_TRIANGLE& t : caps.triangles) {
        uint32_t base = static_cast<uint32_t>(out.vertices.size());
        for (const D2D1_POINT_2F* p : { &t.point1, &t.point2, &t.point3 }) out.vertices.push_back({ world(*p, -hd), { 0, 0, -1 }, capUv(*p) });
        for (const D2D1_POINT_2F* p : { &t.point1, &t.point2, &t.point3 }) out.vertices.push_back({ world(*p, hd), { 0, 0, 1 }, capUv(*p) });
        out.indices.insert(out.indices.end(), { base, base + 1, base + 2, base + 3, base + 5, base + 4 });
    }

    // Sides: one quad per contour edge, normals smoothed across shallow corners.
    const float cosSmooth = std::cos(40.0f * 3.14159265f / 180.0f);
    const float eps = std::max(tolerance, 0.002f) * 3.0f;
    for (auto& pts : contours.contours) {
        // Drop a duplicated closing point and degenerate edges.
        std::vector<D2D1_POINT_2F> c;
        for (const auto& p : pts) {
            if (!c.empty() && std::fabs(p.x - c.back().x) < 1e-6f && std::fabs(p.y - c.back().y) < 1e-6f) continue;
            c.push_back(p);
        }
        if (c.size() > 1 && std::fabs(c.front().x - c.back().x) < 1e-6f && std::fabs(c.front().y - c.back().y) < 1e-6f) c.pop_back();
        const size_t m = c.size();
        if (m < 3) continue;

        // Outward edge normals in D2D space; orientation from a containment test on the longest edge.
        std::vector<XMFLOAT2> en(m);
        size_t longest = 0;
        float longestLen = -1;
        for (size_t i = 0; i < m; ++i) {
            const auto& a = c[i];
            const auto& b = c[(i + 1) % m];
            float dx = b.x - a.x, dy = b.y - a.y;
            float len = std::sqrt(dx * dx + dy * dy);
            if (len > longestLen) { longestLen = len; longest = i; }
            en[i] = len > 1e-9f ? XMFLOAT2{ dy / len, -dx / len } : XMFLOAT2{ 0, 0 };
        }
        {
            const auto& a = c[longest];
            const auto& b = c[(longest + 1) % m];
            D2D1_POINT_2F probe{ (a.x + b.x) * 0.5f + en[longest].x * eps, (a.y + b.y) * 0.5f + en[longest].y * eps };
            BOOL inside = FALSE;
            outlined->FillContainsPoint(probe, nullptr, tolerance, &inside);
            if (inside)
                for (auto& nrm : en) { nrm.x = -nrm.x; nrm.y = -nrm.y; }
        }
        // Per-vertex normals: averaged with the previous edge when the corner is shallow.
        std::vector<XMFLOAT2> startN(m), endN(m);   // normal at the start / end of edge i
        for (size_t i = 0; i < m; ++i) {
            const XMFLOAT2& prev = en[(i + m - 1) % m];
            const XMFLOAT2& cur = en[i];
            float d = prev.x * cur.x + prev.y * cur.y;
            if (d > cosSmooth) {
                float sx = prev.x + cur.x, sy = prev.y + cur.y;
                float l = std::sqrt(sx * sx + sy * sy);
                XMFLOAT2 s = l > 1e-9f ? XMFLOAT2{ sx / l, sy / l } : cur;
                startN[i] = s;
                endN[(i + m - 1) % m] = s;
            } else {
                startN[i] = cur;
                endN[(i + m - 1) % m] = prev;
            }
        }
        float arc = 0.0f;
        for (size_t i = 0; i < m; ++i) {
            const auto& a = c[i];
            const auto& b = c[(i + 1) % m];
            float len = std::sqrt((b.x - a.x) * (b.x - a.x) + (b.y - a.y) * (b.y - a.y));
            XMFLOAT3 nA{ startN[i].x, -startN[i].y, 0 }, nB{ endN[i].x, -endN[i].y, 0 };
            uint32_t base = static_cast<uint32_t>(out.vertices.size());
            out.vertices.push_back({ world(a, -hd), nA, { arc, 0 } });
            out.vertices.push_back({ world(a, hd), nA, { arc, 1 } });
            out.vertices.push_back({ world(b, -hd), nB, { arc + len, 0 } });
            out.vertices.push_back({ world(b, hd), nB, { arc + len, 1 } });
            out.indices.insert(out.indices.end(), { base, base + 2, base + 1, base + 1, base + 2, base + 3 });
            arc += len;
        }
    }

    // Centre on the bounding box.
    XMFLOAT3 lo{ FLT_MAX, FLT_MAX, FLT_MAX }, hi{ -FLT_MAX, -FLT_MAX, -FLT_MAX };
    for (const auto& v : out.vertices) {
        lo.x = std::min(lo.x, v.position.x); lo.y = std::min(lo.y, v.position.y);
        hi.x = std::max(hi.x, v.position.x); hi.y = std::max(hi.y, v.position.y);
    }
    float cx = (lo.x + hi.x) * 0.5f, cy = (lo.y + hi.y) * 0.5f;
    for (auto& v : out.vertices) { v.position.x -= cx; v.position.y -= cy; }
    out.FixWinding();
    return out;
}

} // namespace rs::FontMesh
