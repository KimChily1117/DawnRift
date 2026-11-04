#pragma once
#include "MonoBehaviour.h"
#include "VertexData.h"

class ProjectileScript : public MonoBehaviour
{
public:
    ~ProjectileScript() override = default;
    void Awake() override;
    void Update() override;

    void SetTarget(const Vec3& targetPos, float speed, const shared_ptr<GameObject>& targetObject = nullptr,
                   bool annieQ = false, const string& missileVfx = "");
    void OnHit(const Vec3& position);

    void SetHitVfx(const string& system)
    {
        _hitVfx = system;
    }

    void CreateTrail();
    void UpdateTrail(const Vec3& nextPos);
    void RemoveTrail();
    void ResetForPool();
    shared_ptr<GameObject> _trailObj;

private:
    Vec3 _startPos;
    Vec3 _targetPos;
    Vec3 _direction;
    float _speed = 0.f;
    float _visualTime = 0.f;
    float _trailSparkAccumulator = 0.f;
    float _trailSampleRemainder = 0.f;
    Vec3 _lastTrailPosition = Vec3::Zero;
    shared_ptr<class Mesh> _trailMesh;
    vector<Vec3> _trailHistory;
    vector<VertexTextureNormalTangentData> _trailVertices;
    bool _isMoving = false;
    bool _trailActive = false;
    weak_ptr<GameObject> _targetObject;
    weak_ptr<class AnnieQEffect> _qEffects;
    uint64 _qFlight = 0;
    bool _annieQ = false;
    string _hitVfx;
    uint64 _missileVfx = 0;
    void StopMissileVfx();
};
