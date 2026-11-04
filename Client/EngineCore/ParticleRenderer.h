#pragma once
#include "Component.h"

#define MAX_PARTICLE_COUNT 500

struct VertexParticleFX
{
    Vec3 position;
    Vec2 uv;
    Vec2 scale;
    Vec2 random;
};

class ParticleRenderer : public Component
{
    using Super = Component;

public:
    ParticleRenderer();
    virtual ~ParticleRenderer();

    void Update();
    void ApplyData(const ParticleData& data);

    void SetMaterial(shared_ptr<Material> mat)
    {
        _material = mat;
    }

    void SetPass(uint8 pass)
    {
        _pass = pass;
    }

private:
    void CreateParticles();

private:
    shared_ptr<Material> _material;
    shared_ptr<VertexBuffer> _vertexBuffer;
    shared_ptr<IndexBuffer> _indexBuffer;

    vector<VertexParticleFX> _vertices;
    vector<uint32> _indices;

    uint32 _drawCount = 0;
    uint8 _pass = 0;

    ParticleData _data;
    float _elapsedTime = 0.f;
};
