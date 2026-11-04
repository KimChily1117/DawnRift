#include "pch.h"
#include "BlendState.h"

BlendState::BlendState()
{
    Alpha();
}

BlendState::~BlendState() {}

void BlendState::SetState()
{
    DC->OMSetBlendState(_blendState.Get(), _blendFactor, _sampleMask);
}

void BlendState::SetState(float* blendFactor, UINT mask)
{
    DC->OMSetBlendState(_blendState.Get(), blendFactor, mask);
}

void BlendState::Alpha(bool enable)
{
    D3D11_BLEND_DESC desc = {};
    desc.RenderTarget[0].BlendEnable = enable;
    desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    DEVICE->CreateBlendState(&desc, _blendState.GetAddressOf());
}

void BlendState::Additive()
{
    D3D11_BLEND_DESC desc = {};
    desc.RenderTarget[0].BlendEnable = true;
    desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    desc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
    desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    DEVICE->CreateBlendState(&desc, _blendState.GetAddressOf());
}
