// 컴포넌트를 조합하는 게임 오브젝트.
#pragma once
#include "Component.h"
class MonoBehaviour;
class Transform;
class Camera;
class MeshRenderer;
class ModelRenderer;
class ModelAnimator;
class Light;
class BaseCollider;
class Terrain;
class Button;
class ParticleRenderer;

class GameObject : public enable_shared_from_this<GameObject>
{
public:
    GameObject();
    GameObject(std::string name);
    ~GameObject();

    void Awake();
    void Start();
    void Update();
    void LateUpdate();
    void FixedUpdate();

    void Render();

    void SaveTrasnformData();
    void LoadTrasnformData();

    void GUIRender();

    shared_ptr<Component> GetFixedComponent(ComponentType type);
    shared_ptr<Transform> GetTransform();
    shared_ptr<Camera> GetCamera();
    shared_ptr<MeshRenderer> GetMeshRenderer();
    shared_ptr<ModelRenderer> GetModelRenderer();
    shared_ptr<ModelAnimator> GetModelAnimator();
    shared_ptr<Light> GetLight();
    shared_ptr<BaseCollider> GetCollider();
    shared_ptr<Terrain> GetTerrain();

    shared_ptr<Button> GetButton();
    shared_ptr<ParticleRenderer> GetParticleRenderer();

    template <typename T> std::shared_ptr<T> GetScript()
    {
        for (auto& script : _scripts)
        {
            // T 타입으로 캐스팅 가능한 경우 반환
            std::shared_ptr<T> castedScript = std::dynamic_pointer_cast<T>(script);

            if (castedScript)
                return castedScript;
        }

        return nullptr;
    }

    template <typename T> std::shared_ptr<T> GetOrAddScript()
    {
        std::shared_ptr<T> script = GetScript<T>();

        if (script == nullptr)
        {
            script = std::make_shared<T>();
            AddComponent(script);
        }

        return script;
    }

    shared_ptr<Transform> GetOrAddTransform();
    void AddComponent(shared_ptr<Component> component);

    std::string _name;
    bool _enableGUI = false;

    void SetLayerIndex(uint8 layer)
    {
        _layerIndex = layer;
    }

    uint8 GetLayerIndex()
    {
        return _layerIndex;
    }

protected:
    array<shared_ptr<Component>, FIXED_COMPONENT_COUNT> _components;
    vector<shared_ptr<MonoBehaviour>> _scripts;

    uint8 _layerIndex = 0;
};
