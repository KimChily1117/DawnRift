#include "pch.h"
#include "ParticleManager.h"
#include "ParticleSystem.h"

ParticleManager::~ParticleManager()
{
    for (const auto& particles : totalParticle)
    {
        for (ParticleSystem* particle : particles.second)
        {
            delete particle;
        }
    }
}

void ParticleManager::Update()
{
    for (const auto& particles : totalParticle)
    {
        for (ParticleSystem* particle : particles.second)
        {
            if (particle->IsActive())
                particle->Update();
        }
    }
}

void ParticleManager::Render()
{
    for (const auto& particles : totalParticle)
    {
        for (ParticleSystem* particle : particles.second)
        {
            if (particle->IsActive())
                particle->Render();
        }
    }
}

void ParticleManager::Play(wstring key, Vec3 pos, Vec3 rot)
{
    if (totalParticle.count(key) == 0)
        return;

    for (ParticleSystem* particle : totalParticle[key])
    {
        if (!particle->IsActive())
        {
            particle->Play(pos, rot);

            return;
        }
    }
}

void ParticleManager::Add(wstring key, wstring file, UINT poolSize)
{
    if (totalParticle.count(key) > 0)
        return;

    Particles particles(poolSize);

    for (ParticleSystem*& particle : particles)
    {
        particle = new ParticleSystem(file);
    }

    totalParticle[key] = particles;
}
