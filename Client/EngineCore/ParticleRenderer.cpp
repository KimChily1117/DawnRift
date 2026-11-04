#include "pch.h"
#include "ParticleRenderer.h"
#include "Camera.h"
#include "Shader.h"
#include "Material.h"
#include "Scene.h"
#include "Utils.h"

ParticleRenderer::ParticleRenderer() : Component(ComponentType::Particle) {}

ParticleRenderer::~ParticleRenderer() {}

void ParticleRenderer::ApplyData(const ParticleData& data)
{
    _data = data;
    CreateParticles();
}

void ParticleRenderer::CreateParticles()
{
    _drawCount = _data.count;
    _vertices.resize(_drawCount * 4);
    _indices.resize(_drawCount * 6);

    for (int i = 0; i < _drawCount; ++i)
    {
        Vec3 pos;

        Vec2 scale = MathUtils::RandomVec2(_data.minStartScale.x, _data.maxEndScale.x);
        Vec2 rand = MathUtils::RandomVec2(0.f, 1.f);

        int idx = i * 4;
        _vertices[idx + 0] = {pos, Vec2(0, 1), scale, rand};
        _vertices[idx + 1] = {pos, Vec2(0, 0), scale, rand};
        _vertices[idx + 2] = {pos, Vec2(1, 1), scale, rand};
        _vertices[idx + 3] = {pos, Vec2(1, 0), scale, rand};

        _indices[i * 6 + 0] = idx + 0;
        _indices[i * 6 + 1] = idx + 1;
        _indices[i * 6 + 2] = idx + 2;
        _indices[i * 6 + 3] = idx + 2;
        _indices[i * 6 + 4] = idx + 1;
        _indices[i * 6 + 5] = idx + 3;
    }

    _vertexBuffer = make_shared<VertexBuffer>();
    _vertexBuffer->Create(_vertices, 0, true);

    _indexBuffer = make_shared<IndexBuffer>();
    _indexBuffer->Create(_indices);
}

void ParticleRenderer::Update()
{
    if (_drawCount == 0 || _material == nullptr)
        return;

    _elapsedTime += DT;

    // 쉐이더에 전달
    ParticleDesc desc;
    {
        desc.velocity.x = MathUtils::Random(_data.minVelocity.x, _data.maxVelocity.x);
        desc.velocity.y = MathUtils::Random(_data.minVelocity.y, _data.maxVelocity.y);
        desc.velocity.z = MathUtils::Random(_data.minVelocity.z, _data.maxVelocity.z);

        desc.origin = CUR_SCENE->GetMainCamera()->GetTransform()->GetPosition();
        desc.elapsedTime = _elapsedTime;

        desc.startColor = _data.startColor;
        desc.endColor = _data.endColor;
    }

    _material->SetShader(CUR_SCENE->_shader);
    auto shader = _material->GetShader();

    shader->PushTransformData(TransformDesc{GetTransform()->GetWorldMatrix()});
    shader->PushGlobalData(Camera::S_MatView, Camera::S_MatProjection);
    shader->PushParticleData(desc);

    _material->Update();

    _vertexBuffer->PushData();
    _indexBuffer->PushData();

    shader->DrawIndexed(0, 12, _drawCount * 6);
}
