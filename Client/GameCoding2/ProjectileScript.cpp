#include "pch.h"
#include "ProjectileScript.h"
#include "MeshRenderer.h"
#include "Material.h"
#include "Camera.h"
#include "AnnieQEffect.h"
#include "LolVfx.h"
#include "BasePlayerController.h"

namespace
{
constexpr bool EnableProjectileTrail = true;
constexpr uint32 TrailMaxPoints = 32;
constexpr float TrailPointSpacing = 0.12f;
constexpr float TrailMaxLength = 3.6f;
constexpr float TrailHeadWidth = 0.78f;
constexpr float TrailTailWidth = 0.045f;
constexpr float TrailSparkInterval = 0.15f;
constexpr float MinimumPointDistance = 0.005f;
constexpr float MissileVfxMaxSeconds = 10.f;
}

void ProjectileScript::Awake()
{
    _startPos = GetTransform()->GetPosition();
}

void ProjectileScript::SetTarget(const Vec3& targetPos, float speed, const shared_ptr<GameObject>& targetObject,
                                 bool annieQ, const string& missileVfx)
{
    ResetForPool();
    _annieQ = annieQ;
    _startPos = GetTransform()->GetPosition();
    _targetPos = targetPos;
    _speed = max(0.f, speed);
    _visualTime = 0.f;
    _trailSparkAccumulator = 0.f;
    _trailSampleRemainder = 0.f;
    _lastTrailPosition = _startPos;
    _trailHistory.clear();

    if (_trailHistory.capacity() < TrailMaxPoints)
        _trailHistory.reserve(TrailMaxPoints);
    _trailHistory.push_back(_startPos);
    _targetObject = targetObject;
    _direction = _targetPos - _startPos;
    _isMoving = _direction.LengthSquared() > 0.0001f && _speed > 0.f;

    if (_isMoving)
        _direction.Normalize();

    if (!missileVfx.empty())
    {
        LolVfx::PlayOptions options;
        options.follow = GetGameObject();
        options.duration = MissileVfxMaxSeconds;
        options.orientToMotion = true;
        _missileVfx = LolVfx::Get()->Play(missileVfx, _startPos, _direction, options);

        if (_missileVfx != 0)
            return;
    }

    if (_annieQ)
    {
        auto effects = AnnieQEffect::Get();
        _qEffects = effects;
        _qFlight = effects->StartFlight(GetGameObject(), _direction);

        if (_qFlight != 0)
            return;
    }

    CreateTrail();
    UpdateTrail(_startPos);
}

void ProjectileScript::Update()
{
    if (!_isMoving)
        return;
    const float deltaTime = TIME->GetDeltaTime();

    if (!std::isfinite(deltaTime) || deltaTime <= 0.f)
        return;
    _visualTime += deltaTime;

    if (auto target = _targetObject.lock())
    {
        _targetPos = target->GetTransform()->GetPosition();
        _targetPos.y += 1.4f;
    }

    Vec3 currentPos = GetTransform()->GetPosition();
    Vec3 toTarget = _targetPos - currentPos;
    const float remainingDistance = toTarget.Length();
    const float moveDistance = _speed * deltaTime;

    if (remainingDistance <= max(0.05f, moveDistance))
    {
        GetTransform()->SetPosition(_targetPos);
        _isMoving = false;

        if (auto effects = _qEffects.lock())
            effects->StopFlight(_qFlight);
        StopMissileVfx();
        RemoveTrail();

        return;
    }

    _direction = toTarget / remainingDistance;
    Vec3 nextPos = currentPos + _direction * moveDistance;
    GetTransform()->SetPosition(nextPos);
    _trailSparkAccumulator += deltaTime;
    const bool customVisual = _qFlight == 0 && _missileVfx == 0;

    if (customVisual && _trailSparkAccumulator >= TrailSparkInterval)
    {
        _trailSparkAccumulator = fmodf(_trailSparkAccumulator, TrailSparkInterval);
        PARTICLE->Play(L"ProjectileTrailSparks", nextPos);
    }

    if (customVisual)
        UpdateTrail(nextPos);
}

void ProjectileScript::OnHit(const Vec3& position)
{
    if (_annieQ)
    {
        auto effects = _qEffects.lock();

        if (!effects)
            effects = AnnieQEffect::Get();

        GetTransform()->SetPosition(position);
        effects->StopFlight(_qFlight);
        StopMissileVfx();

        Vec3 center = position;

        if (auto target = _targetObject.lock())
            center = BasePlayerController::GetHitVfxPosition(target);
        LolVfx::Get()->Play("Annie_Base_Q_tar", center, center - _startPos);
    }
    else if (!_hitVfx.empty())
    {
        StopMissileVfx();

        Vec3 center = position;

        if (auto target = _targetObject.lock())
            center = BasePlayerController::GetHitVfxPosition(target);
        LolVfx::Get()->Play(_hitVfx, center, center - _startPos);
    }
    else
    {
        PARTICLE->Play(L"ProjectileImpact", position);
        PARTICLE->Play(L"ProjectileExplosion", position);
    }

    RemoveTrail();
}

void ProjectileScript::CreateTrail()
{
    if (!EnableProjectileTrail)
        return;

    if (!_trailObj)
    {
        auto baseMaterial = RESOURCES->Get<Material>(L"Trail");

        if (!baseMaterial)
            return;

        _trailVertices.resize(TrailMaxPoints * 2);

        for (uint32 pointIndex = 0; pointIndex < TrailMaxPoints; ++pointIndex)
        {
            const uint32 vertexIndex = pointIndex * 2;
            _trailVertices[vertexIndex].position = _startPos;
            _trailVertices[vertexIndex].uv = Vec2(0.f, 0.f);
            _trailVertices[vertexIndex + 1].position = _startPos;
            _trailVertices[vertexIndex + 1].uv = Vec2(0.f, 1.f);
        }

        vector<uint32> indices;
        indices.reserve((TrailMaxPoints - 1) * 6);

        for (uint32 pointIndex = 0; pointIndex + 1 < TrailMaxPoints; ++pointIndex)
        {
            const uint32 left = pointIndex * 2;
            const uint32 right = left + 1;
            const uint32 nextLeft = left + 2;
            const uint32 nextRight = left + 3;
            indices.push_back(left);
            indices.push_back(right);
            indices.push_back(nextLeft);
            indices.push_back(nextLeft);
            indices.push_back(right);
            indices.push_back(nextRight);
        }

        _trailMesh = make_shared<Mesh>();
        _trailMesh->CreateDynamic(_trailVertices, indices);
        _trailObj = make_shared<GameObject>("ProjectileTrail");
        _trailObj->GetOrAddTransform();
        _trailObj->AddComponent(make_shared<MeshRenderer>());
        _trailObj->GetMeshRenderer()->SetMaterial(baseMaterial);
        _trailObj->GetMeshRenderer()->SetMesh(_trailMesh);
        _trailObj->GetMeshRenderer()->SetPass(16);
    }

    _trailObj->GetTransform()->SetPosition(Vec3::Zero);
    _trailObj->GetTransform()->SetRotation(Vec3::Zero);
    _trailObj->GetTransform()->SetScale(Vec3(1.f));

    if (!_trailActive)
    {
        CUR_SCENE->Add(_trailObj);
        _trailActive = true;
    }
}

void ProjectileScript::UpdateTrail(const Vec3& nextPos)
{
    if (!EnableProjectileTrail)
        return;

    if (!_trailObj || !_trailMesh || _trailVertices.size() != TrailMaxPoints * 2)
        return;

    Vec3 remainingDelta = nextPos - _lastTrailPosition;
    float remainingFrameDistance = remainingDelta.Length();

    if (remainingFrameDistance > MinimumPointDistance && std::isfinite(remainingFrameDistance))
    {
        const Vec3 frameDirection = remainingDelta / remainingFrameDistance;
        float distanceToNextSample = TrailPointSpacing - _trailSampleRemainder;
        uint32 emittedPointCount = 0;

        while (distanceToNextSample <= remainingFrameDistance && emittedPointCount < TrailMaxPoints)
        {
            const Vec3 samplePosition = _lastTrailPosition + frameDirection * distanceToNextSample;
            _trailHistory.insert(_trailHistory.begin(), samplePosition);

            if (_trailHistory.size() > TrailMaxPoints)
                _trailHistory.pop_back();
            remainingFrameDistance -= distanceToNextSample;
            _lastTrailPosition = samplePosition;
            distanceToNextSample = TrailPointSpacing;
            _trailSampleRemainder = 0.f;
            ++emittedPointCount;
        }

        _trailSampleRemainder = min(TrailPointSpacing, _trailSampleRemainder + remainingFrameDistance);
        _lastTrailPosition = nextPos;
    }

    std::array<Vec3, TrailMaxPoints> points = {};
    std::array<float, TrailMaxPoints> distances = {};
    uint32 pointCount = 1;
    points[0] = nextPos;
    float accumulatedDistance = 0.f;
    Vec3 previousPoint = nextPos;

    for (const Vec3& historyPoint : _trailHistory)
    {
        if (pointCount >= TrailMaxPoints)
            break;
        const Vec3 segment = historyPoint - previousPoint;
        const float segmentLength = segment.Length();

        if (!std::isfinite(segmentLength) || segmentLength < MinimumPointDistance)
            continue;

        if (accumulatedDistance + segmentLength >= TrailMaxLength)
        {
            const float remainingLength = TrailMaxLength - accumulatedDistance;
            points[pointCount] = previousPoint + segment * (remainingLength / segmentLength);
            distances[pointCount] = TrailMaxLength;
            ++pointCount;
            break;
        }

        accumulatedDistance += segmentLength;
        points[pointCount] = historyPoint;
        distances[pointCount] = accumulatedDistance;
        previousPoint = historyPoint;
        ++pointCount;
    }

    Vec3 cameraPosition = nextPos + Vec3(0.f, 1.f, -1.f);
    Vec3 cameraRight = Vec3(1.f, 0.f, 0.f);

    if (auto mainCamera = CUR_SCENE->GetMainCamera())
    {
        cameraPosition = mainCamera->GetTransform()->GetPosition();
        cameraRight = mainCamera->GetTransform()->GetRight();

        if (cameraRight.LengthSquared() > 0.0001f)
            cameraRight.Normalize();
    }

    for (uint32 pointIndex = 0; pointIndex < pointCount; ++pointIndex)
    {
        Vec3 tangent = _direction;

        if (pointCount > 1)
        {
            if (pointIndex == 0)
                tangent = points[0] - points[1];
            else if (pointIndex + 1 == pointCount)
                tangent = points[pointIndex - 1] - points[pointIndex];
            else
                tangent = points[pointIndex - 1] - points[pointIndex + 1];
        }

        if (tangent.LengthSquared() > 0.0001f)
            tangent.Normalize();

        Vec3 viewDirection = cameraPosition - points[pointIndex];

        if (viewDirection.LengthSquared() > 0.0001f)
            viewDirection.Normalize();
        else
            viewDirection = Vec3(0.f, 1.f, 0.f);

        Vec3 side = tangent.Cross(viewDirection);

        if (side.LengthSquared() <= 0.0001f)
            side = cameraRight;

        if (side.LengthSquared() > 0.0001f)
            side.Normalize();

        const float trailRatio = min(1.f, distances[pointIndex] / TrailMaxLength);
        float width = TrailHeadWidth + (TrailTailWidth - TrailHeadWidth) * trailRatio;
        width *= 1.f + sinf(_visualTime * 21.f) * 0.06f * (1.f - trailRatio);
        const float textureU = 1.f - trailRatio;
        const uint32 vertexIndex = pointIndex * 2;
        _trailVertices[vertexIndex].position = points[pointIndex] - side * (width * 0.5f);
        _trailVertices[vertexIndex].uv = Vec2(textureU, 0.f);
        _trailVertices[vertexIndex].normal = viewDirection;
        _trailVertices[vertexIndex].tangent = tangent;
        _trailVertices[vertexIndex + 1].position = points[pointIndex] + side * (width * 0.5f);
        _trailVertices[vertexIndex + 1].uv = Vec2(textureU, 1.f);
        _trailVertices[vertexIndex + 1].normal = viewDirection;
        _trailVertices[vertexIndex + 1].tangent = tangent;
    }

    const Vec3 tailPosition = points[pointCount - 1];

    for (uint32 pointIndex = pointCount; pointIndex < TrailMaxPoints; ++pointIndex)
    {
        const uint32 vertexIndex = pointIndex * 2;
        _trailVertices[vertexIndex].position = tailPosition;
        _trailVertices[vertexIndex].uv = Vec2(0.f, 0.f);
        _trailVertices[vertexIndex + 1].position = tailPosition;
        _trailVertices[vertexIndex + 1].uv = Vec2(0.f, 1.f);
    }

    _trailMesh->UpdateDynamicVertices(_trailVertices);
}

void ProjectileScript::RemoveTrail()
{
    if (!_trailObj || !_trailActive)
        return;
    CUR_SCENE->Remove(_trailObj);
    _trailActive = false;
}

void ProjectileScript::StopMissileVfx()
{
    if (_missileVfx != 0)
        LolVfx::Get()->Stop(_missileVfx, true);
    _missileVfx = 0;
}

void ProjectileScript::ResetForPool()
{
    StopMissileVfx();
    _hitVfx.clear();

    if (auto effects = _qEffects.lock())
        effects->StopFlight(_qFlight);
    _qEffects.reset();
    _qFlight = 0;
    _annieQ = false;
    _isMoving = false;
    _speed = 0.f;
    _visualTime = 0.f;
    _trailSparkAccumulator = 0.f;
    _trailSampleRemainder = 0.f;
    _lastTrailPosition = Vec3::Zero;
    _trailHistory.clear();
    _targetObject.reset();
    RemoveTrail();
}
