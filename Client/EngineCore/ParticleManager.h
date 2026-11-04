#pragma once

class ParticleManager
{
    DECLARE_SINGLE(ParticleManager)
    ~ParticleManager();

public:
    void Update();
    void Render();

    void Play(wstring key, Vec3 pos, Vec3 rot = Vec3());

    void Add(wstring key, wstring file, UINT poolSize);

private:
    typedef vector<class ParticleSystem*> Particles;
    map<wstring, Particles> totalParticle;
};
