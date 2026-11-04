#pragma once
#include "MonoBehaviour.h"
#include "VertexData.h"
#include <array>

class AnnieWEffect : public MonoBehaviour
{
public:
    static shared_ptr<AnnieWEffect> Get();
    bool Play(const Vec3& origin, const Vec3& direction);
    void LateUpdate() override;

private:
    static constexpr size_t MaxCasts = 4;

    enum Layer : size_t
    {
        Ground,
        Wave,
        Edge,
        InnerEdge,
        Flame,
        FlameEcho,
        LayerCount
    };

    struct Surface
    {
        shared_ptr<GameObject> object;
        shared_ptr<class Material> material;
        bool visible = false;
    };

    struct Cast
    {
        bool active = false;
        float age = 0.f;
        Vec3 origin = Vec3::Zero, direction = Vec3::Forward;
        uint32 seed = 0;
        std::array<Surface, LayerCount> surfaces;
    };

    void Initialize();
    void UpdateCast(Cast& cast);
    void Sparks(const Cast& cast, const Vec3& right, const Vec3& up);
    std::array<Cast, MaxCasts> _casts;
    weak_ptr<Scene> _scene;
    shared_ptr<GameObject> _sparks;
    shared_ptr<class Mesh> _sparkMesh;
    vector<VertexTextureNormalTangentData> _vertices;
    size_t _used = 0;
    bool _sparksVisible = false, _ready = false;
    uint32 _nextSeed = 1;
};
