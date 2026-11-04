#pragma once
#include "Component.h"

class Light : public Component
{
public:
    Light();
    virtual ~Light();

    virtual void Update();

public:
    LightDesc& GetLightDesc()
    {
        return _desc;
    }

    void SetLightDesc(LightDesc& desc)
    {
        _desc = desc;
    }

    void SetAmbient(const Color& color)
    {
        _desc.ambient = color;
    }

    void SetDiffuse(const Color& color)
    {
        _desc.diffuse = color;
    }

    void SetSpecular(const Color& color)
    {
        _desc.specular = color;
    }

    void SetEmissive(const Color& color)
    {
        _desc.emissive = color;
    }

    void SetLightDirection(Vec3 direction)
    {
        _desc.direction = direction;
    }

private:
    LightDesc _desc;
};
