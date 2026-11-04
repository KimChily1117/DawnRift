#pragma once

class SamplerState
{
public:
    SamplerState();
    ~SamplerState();

    void Set(UINT slot = 0);

    // SamplerState.h
public:
    ID3D11SamplerState* GetComPtr() const
    {
        return _samplerState.Get();
    }

private:
    ComPtr<ID3D11SamplerState> _samplerState;
};
