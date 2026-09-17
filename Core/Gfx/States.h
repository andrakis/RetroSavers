#pragma once
#include "Device.h"

namespace rs {

// Common pipeline state presets. Rasterizer states have scissor enabled so the
// host's per-monitor scissor rect always applies.
class States {
public:
    void Create(Device& device);

    ID3D11RasterizerState* CullBack() const { return m_cullBack.Get(); }
    ID3D11RasterizerState* CullNone() const { return m_cullNone.Get(); }
    ID3D11RasterizerState* Wireframe() const { return m_wireframe.Get(); }

    ID3D11DepthStencilState* DepthDefault() const { return m_depthDefault.Get(); }
    ID3D11DepthStencilState* DepthReadOnly() const { return m_depthReadOnly.Get(); }
    ID3D11DepthStencilState* DepthDisabled() const { return m_depthDisabled.Get(); }

    ID3D11BlendState* Opaque() const { return m_opaque.Get(); }
    ID3D11BlendState* AlphaBlend() const { return m_alpha.Get(); }
    ID3D11BlendState* Additive() const { return m_additive.Get(); }
    ID3D11BlendState* PremultipliedAlpha() const { return m_premultiplied.Get(); }   // src ONE, dst INV_SRC_ALPHA

    ID3D11SamplerState* LinearWrap() const { return m_linearWrap.Get(); }
    ID3D11SamplerState* LinearClamp() const { return m_linearClamp.Get(); }
    ID3D11SamplerState* PointClamp() const { return m_pointClamp.Get(); }
    ID3D11SamplerState* PointWrap() const { return m_pointWrap.Get(); }
    ID3D11SamplerState* AnisoWrap() const { return m_anisoWrap.Get(); }

    // Convenience: opaque, depth on, back-face culling.
    void SetOpaque3D(ID3D11DeviceContext* ctx) const;
    // Convenience: no depth, no culling, given blend.
    void Set2D(ID3D11DeviceContext* ctx, ID3D11BlendState* blend) const;

private:
    ComPtr<ID3D11RasterizerState> m_cullBack, m_cullNone, m_wireframe;
    ComPtr<ID3D11DepthStencilState> m_depthDefault, m_depthReadOnly, m_depthDisabled;
    ComPtr<ID3D11BlendState> m_opaque, m_alpha, m_additive, m_premultiplied;
    ComPtr<ID3D11SamplerState> m_linearWrap, m_linearClamp, m_pointClamp, m_pointWrap, m_anisoWrap;
};

} // namespace rs
