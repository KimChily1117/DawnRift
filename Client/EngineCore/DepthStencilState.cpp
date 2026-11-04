#include "pch.h"
#include "DepthStencilState.h"

DepthStencilState::DepthStencilState()
{
    DepthEnable(true);
}

DepthStencilState::~DepthStencilState() {}

void DepthStencilState::SetState()
{
    DC->OMSetDepthStencilState(_depthStencilState.Get(), 0);
}

void DepthStencilState::DepthEnable(bool enable)
{
    _depthEnable = enable;
    DepthWriteMask(_writeMask);
}

void DepthStencilState::DepthWriteMask(D3D11_DEPTH_WRITE_MASK mask)
{
    _writeMask = mask;

    D3D11_DEPTH_STENCIL_DESC desc = {};
    desc.DepthEnable = _depthEnable;
    desc.DepthWriteMask = _writeMask;
    desc.DepthFunc = D3D11_COMPARISON_LESS;

    DEVICE->CreateDepthStencilState(&desc, _depthStencilState.GetAddressOf());
}
