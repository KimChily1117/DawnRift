#pragma once
#include "ConstantBuffer.h"

class Shader;

struct GlobalDesc
{
    Matrix V = Matrix::Identity;
    Matrix P = Matrix::Identity;
    Matrix VP = Matrix::Identity;
    Matrix VInv = Matrix::Identity;
};

struct TransformDesc
{
    Matrix W = Matrix::Identity;
};

// Light
struct LightDesc
{
    Color ambient = Color(1.f, 1.f, 1.f, 1.f);
    Color diffuse = Color(1.f, 1.f, 1.f, 1.f);
    Color specular = Color(1.f, 1.f, 1.f, 1.f);
    Color emissive = Color(1.f, 1.f, 1.f, 1.f);

    Vec3 direction;
    float padding0;
};

struct MaterialDesc
{
    Color ambient = Color(0.f, 0.f, 0.f, 1.f);
    Color diffuse = Color(1.f, 1.f, 1.f, 1.f);
    Color specular = Color(0.f, 0.f, 0.f, 1.f);
    Color emissive = Color(0.f, 0.f, 0.f, 1.f);
};

// Bone
#define MAX_MODEL_TRANSFORMS 700
#define MAX_MODEL_KEYFRAMES 500
#define MAX_MODEL_INSTANCE 500

struct BoneDesc
{
    Matrix transforms[MAX_MODEL_TRANSFORMS];
};

struct KeyframeDesc
{
    int32 animIndex = 0;
    uint32 currFrame = 0;
    uint32 nextFrame = 0;
    float ratio = 0.f;
    float sumTime = 0.f;
    float speed = 1.f;
    uint32 loop = 1;
    Vec3 padding;
};

using AnimationEvent = function<void()>;

struct TweenDesc
{
    TweenDesc()
    {
        curr.animIndex = 0;
        next.animIndex = -1;
    }

    void ClearNextAnim()
    {
        next.animIndex = -1;
        next.currFrame = 0;
        next.nextFrame = 0;
        next.sumTime = 0;
        tweenSumTime = 0;
        tweenRatio = 0;
    }

    // ✅ 애니메이션 특정 시점에 실행할 이벤트 추가
    void AddAnimationEvent(float timeRatio, std::function<void()> callback)
    {
        animationEvents[timeRatio] = callback;
    }

    // ✅ 애니메이션 이벤트 실행
    void ExecuteAnimationEvents(float ratio)
    {
        auto it = animationEvents.lower_bound(ratio);

        while (it != animationEvents.end() && it->first <= ratio)
        {
            it->second();
            it = animationEvents.erase(it);
        }
    }

    float tweenDuration = 1.0f;
    float tweenRatio = 0.f;
    float tweenSumTime = 0.f;
    float padding = 0.f;
    KeyframeDesc curr;
    KeyframeDesc next;

    std::map<float, std::function<void()>> animationEvents;
};

struct GpuKeyframeDesc
{
    int32 animIndex = 0;
    uint32 currFrame = 0;
    uint32 nextFrame = 0;
    float ratio = 0.f;
    float sumTime = 0.f;
    float speed = 1.f;
    float padding[2] = {};

    GpuKeyframeDesc() = default;

    GpuKeyframeDesc(const KeyframeDesc& k)
        : animIndex(k.animIndex), currFrame(k.currFrame), nextFrame(k.nextFrame), ratio(k.ratio), sumTime(k.sumTime),
          speed(k.speed)
    {
    }
};

static_assert(sizeof(GpuKeyframeDesc) == 32, "KeyframeDesc as the shader reads it");

struct GpuTweenDesc
{
    float tweenDuration = 1.f;
    float tweenRatio = 0.f;
    float tweenSumTime = 0.f;
    float padding = 0.f;
    GpuKeyframeDesc curr;
    GpuKeyframeDesc next;

    GpuTweenDesc() = default;

    GpuTweenDesc(const TweenDesc& t)
        : tweenDuration(t.tweenDuration), tweenRatio(t.tweenRatio), tweenSumTime(t.tweenSumTime), curr(t.curr),
          next(t.next)
    {
    }
};

static_assert(sizeof(GpuTweenDesc) == 80, "TweenFrameDesc as the shader reads it");

struct InstancedTweenDesc
{
    GpuTweenDesc tweens[MAX_MODEL_INSTANCE];
};

struct UIFillMountDesc
{
    float ratio = 0.f;
    float padding[3];
};

struct ParticleData
{
    // 토글 옵션
    bool isLoop = true;
    bool isAdditive = true;
    bool isBillboard = true;

    // 규모 옵션
    UINT count = 100;
    float duration = 1.0f;

    // 파티클 제어 옵션 (min : 최소, max : 최대)
    Vec3 minVelocity = {-1, -1, -1};
    Vec3 maxVelocity = {+1, +1, +1};
    Vec3 minAccelation;
    Vec3 maxAccelation;
    Vec3 minStartScale = {1, 1, 1};
    Vec3 maxStartScale = {1, 1, 1};
    Vec3 minEndScale = {1, 1, 1};
    Vec3 maxEndScale = {1, 1, 1};
    float minSpeed = 1.0f;
    float maxSpeed = 3.0f;
    float minAngularVelocity = -10.0f;
    float maxAngularVelocity = +10.0f;
    float minStartTime = 0.0f;
    float maxStartTime = 0.0f;
    XMFLOAT4 startColor = {1, 1, 1, 1};
    XMFLOAT4 endColor = {1, 1, 1, 1};
};

struct ParticleDesc
{
    Color startColor;
    Color endColor;

    Vec3 velocity;
    float drawDistance;

    Vec3 extent;
    float turbulence;

    Vec3 origin;
    float elapsedTime;
};
