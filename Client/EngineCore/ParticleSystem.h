#pragma once
#include "BlendState.h"
#include "DepthStencilState.h"

class ParticleSystem
{
private:
    struct InstanceData
    {
        Matrix transform = XMMatrixIdentity();
    };

    struct ParticleData
    {
        bool isLoop = true;
        bool isAdditive = true;
        bool isBillboard = true;
        UINT count = 100;
        float duration = 1.0f;
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
        Color startColor = {1, 1, 1, 1};
        Color endColor = {1, 1, 1, 1};
    };

    struct ParticleInfo
    {
        Transform transform;
        Vec3 velocity;
        Vec3 accelation;
        Vec3 startScale;
        Vec3 endScale;

        float speed = 1.0f;
        float angularVelocity = 0.0f;
        float startTime = 0.0f;
    };

public:
    ParticleSystem(wstring file);
    ~ParticleSystem();

    void Update();
    void Render();

    void Play(Vec3 pos, Vec3 rot = Vec3());
    void Stop();

    bool IsActive()
    {
        return isActive;
    }

private:
    void UpdatePhysical();
    void UpdateColor();
    void Init();

    void LoadData(wstring file);

private:
    shared_ptr<GameObject> _quad;

    //vector<InstanceData> instances; // Before Code
    vector<InstancingData> instances;

    vector<ParticleInfo> particleInfos;

    //VertexBuffer* instanceBuffer;	// Before Code
    shared_ptr<InstancingBuffer> instancingBuffer;

    ParticleData data;

    float lifeTime = 0.0f;
    UINT drawCount = 0;
    UINT particleCount = 100;

    Vec3 _originPos = Vec3::Zero;
    bool initialized = false;
    bool isActive = false;

    BlendState* blendState[2];
    DepthStencilState* depthState[2];
};
