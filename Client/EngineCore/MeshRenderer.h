// 메시 렌더링 컴포넌트.
#pragma once
#include "Component.h"
#include "Material.h"
#include "Mesh.h"

class Mesh;
class Shader;
class Material;

class MeshRenderer : public Component
{
    using Super = Component;

public:
    MeshRenderer();
    virtual ~MeshRenderer();

    void SetMesh(shared_ptr<Mesh> mesh)
    {
        _mesh = mesh;
    }

    void SetMaterial(shared_ptr<Material> material)
    {
        _material = material;
    }

    void SetPass(uint8 pass)
    {
        _pass = pass;
    }

    uint8 GetPass() const
    {
        return _pass;
    }

    void SetWorldOverlay(int32 order)
    {
        _worldOverlay = true;
        _overlayOrder = order;
    }

    bool IsWorldOverlay() const
    {
        return _worldOverlay || _pass == 14 || _pass == 15 || _pass == 16;
    }

    int32 GetOverlayOrder() const
    {
        return _overlayOrder;
    }

    shared_ptr<Material> GetMaterial()
    {
        return _material;
    }

    void RenderInstancing(shared_ptr<class InstancingBuffer>& buffer);
    InstanceID GetInstanceID();

    void RenderSingle();

    shared_ptr<Shader> GetShader()
    {
        return _material->GetShader();
    }

    shared_ptr<Mesh> GetMesh()
    {
        return _mesh;
    }

private:
    shared_ptr<Mesh> _mesh;
    shared_ptr<Material> _material;
    uint8 _pass = 0;
    bool _worldOverlay = false;
    int32 _overlayOrder = 0;
    D3D11_PRIMITIVE_TOPOLOGY type = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
};
