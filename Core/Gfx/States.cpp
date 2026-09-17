#include "States.h"

namespace rs {

void States::Create(Device& device) {
    ID3D11Device* d = device.Get();

    D3D11_RASTERIZER_DESC rd{};
    rd.FillMode = D3D11_FILL_SOLID;
    rd.CullMode = D3D11_CULL_BACK;
    rd.FrontCounterClockwise = FALSE;
    rd.DepthClipEnable = TRUE;
    rd.ScissorEnable = TRUE;
    ThrowIfFailed(d->CreateRasterizerState(&rd, &m_cullBack), "CreateRasterizerState");
    rd.CullMode = D3D11_CULL_NONE;
    ThrowIfFailed(d->CreateRasterizerState(&rd, &m_cullNone), "CreateRasterizerState");
    rd.FillMode = D3D11_FILL_WIREFRAME;
    ThrowIfFailed(d->CreateRasterizerState(&rd, &m_wireframe), "CreateRasterizerState");

    D3D11_DEPTH_STENCIL_DESC dd{};
    dd.DepthEnable = TRUE;
    dd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    dd.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    ThrowIfFailed(d->CreateDepthStencilState(&dd, &m_depthDefault), "CreateDepthStencilState");
    dd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    ThrowIfFailed(d->CreateDepthStencilState(&dd, &m_depthReadOnly), "CreateDepthStencilState");
    dd.DepthEnable = FALSE;
    ThrowIfFailed(d->CreateDepthStencilState(&dd, &m_depthDisabled), "CreateDepthStencilState");

    D3D11_BLEND_DESC bd{};
    bd.RenderTarget[0].BlendEnable = FALSE;
    bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    ThrowIfFailed(d->CreateBlendState(&bd, &m_opaque), "CreateBlendState");
    bd.RenderTarget[0].BlendEnable = TRUE;
    bd.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    bd.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    bd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    ThrowIfFailed(d->CreateBlendState(&bd, &m_alpha), "CreateBlendState");
    bd.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
    bd.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
    ThrowIfFailed(d->CreateBlendState(&bd, &m_additive), "CreateBlendState");

    D3D11_SAMPLER_DESC sd{};
    sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sd.MaxLOD = D3D11_FLOAT32_MAX;
    sd.ComparisonFunc = D3D11_COMPARISON_NEVER;
    ThrowIfFailed(d->CreateSamplerState(&sd, &m_linearWrap), "CreateSamplerState");
    sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    ThrowIfFailed(d->CreateSamplerState(&sd, &m_linearClamp), "CreateSamplerState");
    sd.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    ThrowIfFailed(d->CreateSamplerState(&sd, &m_pointClamp), "CreateSamplerState");
    sd.Filter = D3D11_FILTER_ANISOTROPIC;
    sd.MaxAnisotropy = 8;
    sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    ThrowIfFailed(d->CreateSamplerState(&sd, &m_anisoWrap), "CreateSamplerState");
}

void States::SetOpaque3D(ID3D11DeviceContext* ctx) const {
    ctx->RSSetState(m_cullBack.Get());
    ctx->OMSetDepthStencilState(m_depthDefault.Get(), 0);
    ctx->OMSetBlendState(m_opaque.Get(), nullptr, 0xFFFFFFFF);
}

void States::Set2D(ID3D11DeviceContext* ctx, ID3D11BlendState* blend) const {
    ctx->RSSetState(m_cullNone.Get());
    ctx->OMSetDepthStencilState(m_depthDisabled.Get(), 0);
    ctx->OMSetBlendState(blend, nullptr, 0xFFFFFFFF);
}

} // namespace rs
