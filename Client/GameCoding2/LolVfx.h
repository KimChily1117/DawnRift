// 데이터 기반 이펙트 재생.
#pragma once
#include "MonoBehaviour.h"
#include "VertexData.h"
#include <random>

class Mesh;
class Shader;

class LolVfx : public MonoBehaviour
{
public:
    struct PlayOptions
    {
        shared_ptr<GameObject> follow;
        float followHeight = 0.f;
        float duration = 0.f;
        float delay = 0.f;
        bool orientToMotion = false;
        wstring followBone;
    };

    static shared_ptr<LolVfx> Get();

    uint64 Play(const string& system, const Vec3& position, const Vec3& forward, const PlayOptions& options = {});

    void Preload(const string& system)
    {
        Load(system);
    }

    void Stop(uint64 handle, bool clearAttached = false);
    void LateUpdate() override;

    struct PlayRecord
    {
        string system;
        Vec3 position, forward;
        float time = 0.f;
    };

    // Recent successful plays, for diagnostics and tests.
    const vector<PlayRecord>& GetPlayLog() const
    {
        return _playLog;
    }

    size_t GetLiveParticleCount() const
    {
        return _particles.size();
    }

    size_t GetBatchCount() const
    {
        return _batches.size();
    }

    size_t GetActiveEffectCount() const
    {
        return _instances.size();
    }

    size_t GetTrailSegmentCount() const
    {
        return _trailSegments;
    }

private:
    enum class Primitive
    {
        Billboard,
        Quad,
        Mesh,
        Trail
    };

    struct Curve
    {
        vector<float> times;
        vector<Vec4> values;
        Vec4 Evaluate(float t) const;
    };

    struct MeshShape
    {
        vector<Vec3> positions;
        vector<Vec2> uvs;
    };

    struct EmitterDef
    {
        Primitive primitive = Primitive::Billboard;
        shared_ptr<MeshShape> mesh;
        size_t batch = 0;
        float emitterLifetime = -1.f;
        float delay = 0.f, rate = 1.f;
        bool single = false, uniform = false, bound = false, directionOriented = false;
        bool ray = false;
        Vec2 particleLife = Vec2(1.f);
        Vec3 position = Vec3::Zero, velocityMin = Vec3::Zero, velocityMax = Vec3::Zero;
        Vec3 drag = Vec3::Zero, acceleration = Vec3::Zero, sizeMin = Vec3(1.f), sizeMax = Vec3(1.f);
        Vec3 rotationMin = Vec3::Zero, rotationMax = Vec3::Zero, spinMin = Vec3::Zero, spinMax = Vec3::Zero;
        float sphereRadius = 0.f;
        Vec3 offsetMin = Vec3::Zero, offsetMax = Vec3::Zero;

        struct ShapeRotation
        {
            Vec3 axis = Vec3::Up;
            float minDegrees = 0.f, maxDegrees = 0.f;
        };

        vector<ShapeRotation> shapeRotations;
        Vec4 colorBirth = Vec4(1.f);
        Curve sizeCurve, colorCurve, rampCurve;
        bool erosion = false;
        Curve erosionCurve;
        float erosionFeather = 0.1f;
        int32 columns = 1, rows = 1, frames = 1;
        float fps = 0.f;
        Vec2 uvScroll = Vec2::Zero;
        float trailTiling = 0.f;
        float trailCutoff = 0.f;
    };

    struct SystemDef
    {
        vector<EmitterDef> emitters;
        float burstDuration = 0.f;
    };

    struct EmitterState
    {
        float spawnCarry = 1.f;
        bool fired = false;
    };

    struct Instance
    {
        uint64 handle = 0;
        const SystemDef* def = nullptr;
        Vec3 position, right, up, forward;
        Vec3 previousPosition;
        weak_ptr<GameObject> follow;
        float followHeight = 0.f;
        float age = 0.f;
        float loopDuration = 0.f;
        bool stopped = false;
        bool orientToMotion = false;
        wstring bone;
        vector<EmitterState> emitters;

        Vec3 ToWorld(const Vec3& v) const
        {
            return right * v.x + up * v.y + forward * v.z;
        }
    };

    struct Particle
    {
        const EmitterDef* def = nullptr;
        shared_ptr<Instance> anchor;
        Vec3 position, velocity, size;
        Vec3 rotation, spin;
        float age = 0.f, life = 1.f;
        uint64 trail = 0;
        bool weaponTrail = false;
        float cameraDistance = 0.f;
    };

    struct BatchKey
    {
        string texture, erosionMap, textureMult;
        Vec4 erosionMixer = Vec4(0.f, 0.f, 0.f, 1.f);
        bool additive = false, meshCapacity = false;
        bool wrap = false;
    };

    struct Batch
    {
        shared_ptr<GameObject> object;
        shared_ptr<Mesh> mesh;
        vector<VertexTextureNormalTangentData> vertices;
        size_t used = 0;
        bool visible = false;
        bool additive = false;
    };

    const SystemDef* Load(const string& system);
    shared_ptr<MeshShape> LoadMesh(const string& path);
    size_t BatchFor(const BatchKey& key);

    void Spawn(const shared_ptr<Instance>& instance, const EmitterDef& def, float frameFraction = 1.f);
    void Billboard(Batch& batch, const Vec3& center, const Vec2& halfSize, float roll, const Vec4& color,
                   const Vec2& erosion, int32 frame, const EmitterDef& def);
    void Polygon(Batch& batch, const Vec3* corners, const Vec2* uvs, size_t count, const Vec4& color,
                 const Vec2& erosion);
    void Polygon(Batch& batch, const Vec3* corners, const Vec2* uvs, size_t count, const Vec4* colors,
                 const Vec2& erosion);
    void DrawTrails();
    void Flush();
    float Random(float low, float high);
    Vec3 Random(const Vec3& low, const Vec3& high);

    unordered_map<string, unique_ptr<SystemDef>> _systems;
    unordered_map<string, shared_ptr<MeshShape>> _meshes;
    vector<XMFLOAT4> _meshScratch;
    vector<Batch> _batches;
    map<string, size_t> _batchIndex;
    vector<shared_ptr<Instance>> _instances;
    vector<Particle> _particles;
    vector<PlayRecord> _playLog;
    shared_ptr<Shader> _shader;
    weak_ptr<Scene> _scene;
    Vec3 _cameraPosition = Vec3::Zero, _right = Vec3::Right, _up = Vec3::Up;
    uint64 _nextHandle = 1;
    size_t _trailSegments = 0;
    std::mt19937 _random{0x10A1u};
};
