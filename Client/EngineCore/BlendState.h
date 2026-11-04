#pragma once

class BlendState
{
public:
    BlendState();
    ~BlendState();

    void SetState();
    void SetState(float* blendFactor, UINT mask = 0xffffffff);
    void Alpha(bool enable = true);
    void Additive();

private:
    ComPtr<ID3D11BlendState> _blendState;
    float _blendFactor[4] = {0, 0, 0, 0};
    UINT _sampleMask = 0xffffffff;
};
