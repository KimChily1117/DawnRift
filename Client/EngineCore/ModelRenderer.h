#pragma once
#include "Component.h"

class Model;
class Shader;
class Material;
class InstancingBuffer;

class ModelRenderer : public Component
{
    using Super = Component;

public:
    ModelRenderer(shared_ptr<Shader> shader);
    virtual ~ModelRenderer();

    void SetModel(shared_ptr<Model> model);

    void SetPass(uint8 pass)
    {
        _pass = pass;
    }

    bool PickMesh(int32 screenX, int32 screenY, Vec3& pickPos, float& distance);

    void RenderInstancing(shared_ptr<class InstancingBuffer>& buffer);
    InstanceID GetInstanceID();

private:
    shared_ptr<Shader> _shader;
    uint8 _pass = 0;
    shared_ptr<Model> _model;

    D3D11_PRIMITIVE_TOPOLOGY type = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
};
