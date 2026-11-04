#pragma once
#include "ResourceBase.h"

class Texture : public ResourceBase
{
    using Super = ResourceBase;

public:
    Texture();
    ~Texture();

    ComPtr<ID3D11ShaderResourceView> GetComPtr()
    {
        return _shaderResourveView;
    }

    virtual void Load(const wstring& path) override;

    ComPtr<ID3D11Texture2D> GetTexture2D();

    void SetSRV(ComPtr<ID3D11ShaderResourceView> srv)
    {
        _shaderResourveView = srv;
    }

    Vec2 GetSize()
    {
        return _size;
    }

    const DirectX::ScratchImage& GetInfo()
    {
        return _img;
    }

private:
    ComPtr<ID3D11ShaderResourceView> _shaderResourveView;
    Vec2 _size = {0.f, 0.f};
    DirectX::ScratchImage _img = {};
};
