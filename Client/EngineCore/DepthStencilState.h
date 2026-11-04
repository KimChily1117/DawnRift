#pragma once

class DepthStencilState
{
public:
    DepthStencilState();
    ~DepthStencilState();

    void SetState();
    void DepthEnable(bool enable);
    void DepthWriteMask(D3D11_DEPTH_WRITE_MASK mask);

private:
    ComPtr<ID3D11DepthStencilState> _depthStencilState;
    bool _depthEnable = true;
    D3D11_DEPTH_WRITE_MASK _writeMask = D3D11_DEPTH_WRITE_MASK_ALL;
};
