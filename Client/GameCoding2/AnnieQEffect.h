#pragma once
#include "MonoBehaviour.h"
#include "VertexData.h"
#include <array>
#include <deque>
#include <random>

class AnnieQEffect : public MonoBehaviour
{
public:
    static shared_ptr<AnnieQEffect> Get();
    uint64 StartFlight(const shared_ptr<GameObject>& anchor, const Vec3& direction);
    void StopFlight(uint64 id);
    void Impact(const Vec3& position);
    void LateUpdate() override;

private:
    enum Layer : size_t
    {
        SmokeTrail,
        Smoke,
        DarkShockwave,
        HitSmoke,
        Trail,
        Flame,
        Sparks,
        FireGlow,
        Shockwave,
        HitFlame,
        Flash,
        HitGlow,
        LayerCount
    };

    struct Batch
    {
        shared_ptr<GameObject> object;
        shared_ptr<class Mesh> mesh;
        vector<VertexTextureNormalTangentData> vertices;
        size_t used = 0;
        bool visible = false;
    };

    struct Sample
    {
        Vec3 position;
        float time;
    };

    struct Flight
    {
        uint64 id = 0;
        weak_ptr<GameObject> anchor;
        Vec3 position = Vec3::Zero;
        Vec3 direction = Vec3::Forward;
        float started = 0.f;
        float stopped = -1.f;
        float emission = 0.f;
        std::deque<Sample> samples;
    };

    struct Particle
    {
        Layer layer;
        Vec3 position, velocity;
        Vec4 color;
        float age = 0.f, life = 1.f;
        float size = 1.f, endSize = 1.f, rotation = 0.f;
        uint32 frame = 0;
    };

    void Initialize();
    void AddParticle(Layer layer, const Vec3& position, const Vec3& velocity, float life, float size, float endSize,
                     const Vec4& color);
    void Quad(Layer layer, const Vec3& position, const Vec2& size, float rotation, const Vec4& color,
              float erosion = 0.f, uint32 frame = 0, bool atlas = false);
    void Ribbon(Flight& flight, Layer layer, float width, float lifetime, const Vec4& color);
    void CoreMesh(const Flight& flight);
    void Flush();
    float Random(float low, float high);

    std::array<Batch, LayerCount> _batches;
    vector<Flight> _flights;
    vector<Particle> _particles;
    shared_ptr<class Mesh> _coreMesh;
    shared_ptr<class Material> _coreMaterial;
    std::array<shared_ptr<GameObject>, 16> _cores;
    std::array<bool, 16> _coreVisible{};
    size_t _coresUsed = 0;
    std::mt19937 _random{0xA771E};
    weak_ptr<Scene> _scene;
    Vec3 _cameraPosition = Vec3::Zero, _right = Vec3::Right, _up = Vec3::Up;
    uint64 _nextId = 1;
    float _time = 0.f;
};
