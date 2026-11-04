#include "pch.h"
#include "AnnieQEffect.h"
#include "AnnieSkillTuning.h"
#include "Camera.h"
#include "Material.h"
#include "MeshRenderer.h"
#include <fstream>

namespace
{
constexpr size_t MaxFlights = 16;
constexpr size_t MaxParticles = 2048;
constexpr size_t MaxTrailPoints = 32;
constexpr float TrailLifetime = 0.8f;
constexpr float SmokeTrailLifetime = 1.2f;
constexpr float SampleDistance = 0.12f;
const wstring AssetRoot = L"..\\Resources\\Textures\\Annie\\Particles\\OriginalQ\\";

void SetVertex(VertexTextureNormalTangentData& vertex, const Vec3& position, const Vec2& uv, const Vec4& color,
               float erosion)
{
    vertex.position = position;
    vertex.uv = uv;
    vertex.normal = Vec3(color.x, color.y, color.z);
    vertex.tangent = Vec3(color.w, erosion, uv.y);
}

Vec3 SafeNormal(Vec3 value, const Vec3& fallback)
{
    if (!std::isfinite(value.LengthSquared()) || value.LengthSquared() < 0.000001f)
        return fallback;
    value.Normalize();

    return value;
}
}

shared_ptr<AnnieQEffect> AnnieQEffect::Get()
{
    static weak_ptr<Scene> scene;
    static weak_ptr<AnnieQEffect> instance;

    if (scene.lock() == CUR_SCENE)
        if (auto existing = instance.lock())
            return existing;
    auto object = make_shared<GameObject>("AnnieQEffects");
    object->GetOrAddTransform();
    auto effect = object->GetOrAddScript<AnnieQEffect>();
    effect->_scene = CUR_SCENE;
    effect->Initialize();
    CUR_SCENE->Add(object);
    scene = CUR_SCENE;
    instance = effect;

    return effect;
}

void AnnieQEffect::Initialize()
{
    _flights.reserve(MaxFlights);
    _particles.reserve(MaxParticles);
    auto shader = make_shared<Shader>(L"AnnieQ.fx");

    _coreMesh = make_shared<Mesh>();

    if (!_coreMesh->LoadVfxMesh(L"..\\Resources\\Models\\Annie\\Vfx\\annie_base_q_mis_01.vfxmesh"))
    {
        _coreMesh.reset();
        DEBUG_LOG("[AnnieQ] Baked mesh missing/invalid. Run AssimpTool --convert-scb-vfx.");
    }

    _coreMaterial = make_shared<Material>();
    _coreMaterial->SetShader(shader);
    _coreMaterial->SetDiffuseMap(
        RESOURCES->Load<Texture>(L"AnnieQOriginal_annie_base_q_mis_01", AssetRoot + L"annie_base_q_mis_01.dds"));
    _coreMaterial->GetMaterialDesc().diffuse = Vec4(1.f, .48f, .24f, 1.f);

    for (auto& core : _cores)
    {
        core = make_shared<GameObject>("AnnieQCore");
        core->GetOrAddTransform();
        core->AddComponent(make_shared<MeshRenderer>());
        core->GetMeshRenderer()->SetMesh(_coreMesh);
        core->GetMeshRenderer()->SetMaterial(_coreMaterial);
        core->GetMeshRenderer()->SetPass(2);
        core->GetMeshRenderer()->SetWorldOverlay(14);
    }

    const std::array<const wchar_t*, LayerCount> textures = {
        L"annie_base_q_mis_smoke_trail", L"annie_base_q_smoke_2x2", L"annie_base_shockwave",
        L"annie_base_smoke_01",          L"annie_base_q_mis_trail", L"annie_base_q_mis_flames",
        L"annie_base_q_sparks_2x2",      L"annie_fire_glow",        L"annie_base_shockwave",
        L"annie_spirit_phoenix_flames",  L"annie_base_glow4",       L"annie_base_glow3"};

    for (size_t i = 0; i < LayerCount; ++i)
    {
        Batch& batch = _batches[i];
        const size_t quads = (i == Trail || i == SmokeTrail) ? MaxFlights * MaxTrailPoints : 256;
        const size_t count = quads * 4;
        batch.vertices.resize(count);
        vector<uint32> indices;

        for (uint32 index = 0; index < count; index += 4)
            indices.insert(indices.end(), {index, index + 1, index + 2, index + 2, index + 1, index + 3});
        batch.mesh = make_shared<Mesh>();
        batch.mesh->CreateDynamic(batch.vertices, indices);
        auto material = make_shared<Material>();
        material->SetShader(shader);
        const wstring textureName = textures[i];
        auto texture = RESOURCES->Load<Texture>(L"AnnieQOriginal_" + textureName, AssetRoot + textureName + L".dds");
        material->SetDiffuseMap(texture);
        material->SetNormalMap(RESOURCES->Load<Texture>(L"AnnieQNoise", AssetRoot + L"annie_base_noise2.dds"));

        material->SetSpecularMap(texture);
        auto& desc = material->GetMaterialDesc();
        desc.ambient = Vec4(0.f);
        desc.diffuse = Vec4(1.f);

        if (i == HitSmoke)
        {
            material->SetNormalMap(
                RESOURCES->Load<Texture>(L"AnnieQSmokeErode", AssetRoot + L"annie_base_smokeerode.dds"));
            desc.ambient.x = 1.f;
        }

        if (i == HitFlame || i == Shockwave)
            desc.ambient.x = 1.f;

        if (i == HitFlame)
        {
            material->SetSpecularMap(
                RESOURCES->Load<Texture>(L"AnnieQFlamesMult", AssetRoot + L"annie_spirit_phoenix_flames_mult.dds"));
            desc.ambient.y = 1.f;
        }

        batch.object = make_shared<GameObject>("AnnieQBatch");
        batch.object->GetOrAddTransform();
        batch.object->AddComponent(make_shared<MeshRenderer>());
        auto renderer = batch.object->GetMeshRenderer();
        renderer->SetMesh(batch.mesh);
        renderer->SetMaterial(material);
        renderer->SetPass(i <= HitSmoke ? 1 : (i == Trail || i == Flame || i == HitFlame) ? 3 : 0);
        renderer->SetWorldOverlay(10 + static_cast<int32>(i) + (i >= Trail ? 1 : 0));
    }
}

float AnnieQEffect::Random(float low, float high)
{
    return std::uniform_real_distribution<float>(low, high)(_random);
}

uint64 AnnieQEffect::StartFlight(const shared_ptr<GameObject>& anchor, const Vec3& direction)
{
    if (!anchor || _flights.size() >= MaxFlights)
        return 0;
    Flight flight;
    flight.id = _nextId++;
    flight.anchor = anchor;
    flight.position = anchor->GetTransform()->GetPosition();
    flight.direction = SafeNormal(direction, Vec3(0.f, 0.f, 1.f));
    flight.started = _time;
    flight.samples.push_back({flight.position, _time});
    _flights.push_back(std::move(flight));

    return _flights.back().id;
}

void AnnieQEffect::StopFlight(uint64 id)
{
    for (auto& flight : _flights)
        if (flight.id == id && flight.stopped < 0.f)
        {
            if (auto anchor = flight.anchor.lock())
            {
                flight.position = anchor->GetTransform()->GetPosition();

                flight.samples.push_back({flight.position, _time});
            }

            flight.anchor.reset();
            flight.stopped = _time;
            break;
        }
}

void AnnieQEffect::AddParticle(Layer layer, const Vec3& position, const Vec3& velocity, float life, float size,
                               float endSize, const Vec4& color)
{
    if (_particles.size() >= MaxParticles)
        return;
    Particle particle;
    particle.layer = layer;
    particle.position = position;
    particle.velocity = velocity;
    particle.color = color;
    particle.life = life;
    particle.size = size;
    particle.endSize = endSize;
    particle.rotation = Random(-3.14159f, 3.14159f);
    particle.frame = _random() % 4;
    _particles.push_back(particle);
}

void AnnieQEffect::Impact(const Vec3& position)
{
    const size_t firstHitParticle = _particles.size();

    AddParticle(Flash, position, Vec3::Zero, .32f, 1.1f, .2f, Vec4(1.f, .6f, .3f, 1.f));
    AddParticle(HitGlow, position, Vec3::Zero, 1.2f, 1.6f, 2.0f, Vec4(1.f, .14f, .025f, 1.f));
    AddParticle(Shockwave, position, Vec3::Zero, 1.6f, .25f, 2.6f, Vec4(1.f, .28f, .07f, 1.f));
    AddParticle(DarkShockwave, position, Vec3::Zero, 1.6f, .3f, 2.9f, Vec4(.15f, .04f, .03f, .5f));

    for (int i = 0; i < 7; ++i)
    {
        Vec3 radial = SafeNormal(Vec3(Random(-1.f, 1.f), Random(-.3f, .7f), Random(-1.f, 1.f)), Vec3::Up);
        AddParticle(HitFlame, position, radial * Random(.3f, .95f), 1.4f, Random(.6f, 1.0f), .25f,
                    Vec4(1.f, .38f, .14f, 1.f));
        AddParticle(HitSmoke, position, radial * Random(.15f, .45f) + Vec3(0.f, .2f, 0.f), 1.8f, Random(.45f, .8f),
                    1.4f, Vec4(.25f, .16f, .16f, .45f));
    }

    for (int i = 0; i < 14; ++i)
        AddParticle(Sparks, position, Vec3(Random(-1.4f, 1.4f), Random(.2f, 1.4f), Random(-1.4f, 1.4f)),
                    Random(.5f, 1.1f), Random(.06f, .12f), .015f, Vec4(1.f, .42f, .11f, 1.f));

    for (size_t i = firstHitParticle; i < _particles.size(); ++i)
    {
        _particles[i].size *= AnnieSkillTuning::QImpactScale;
        _particles[i].endSize *= AnnieSkillTuning::QImpactScale;
        _particles[i].velocity *= AnnieSkillTuning::QImpactScale;
    }
}

void AnnieQEffect::Quad(Layer layer, const Vec3& position, const Vec2& size, float rotation, const Vec4& color,
                        float erosion, uint32 frame, bool atlas)
{
    Batch& batch = _batches[layer];

    if (batch.used + 4 > batch.vertices.size())
        return;
    const float c = cosf(rotation), s = sinf(rotation);
    const Vec3 right = (_right * c + _up * s) * size.x * .5f;
    const Vec3 up = (_up * c - _right * s) * size.y * .5f;
    const float tile = atlas ? .5f : 1.f;

    const float inset = atlas ? .004f : 0.f;
    const Vec2 uv0((atlas ? frame % 2 : 0) * tile + inset, (atlas ? frame / 2 : 0) * tile + inset);
    const Vec2 uv1 = uv0 + Vec2(tile - 2.f * inset);
    SetVertex(batch.vertices[batch.used++], position - right + up, Vec2(uv0.x, uv0.y), color, erosion);
    SetVertex(batch.vertices[batch.used++], position + right + up, Vec2(uv1.x, uv0.y), color, erosion);
    SetVertex(batch.vertices[batch.used++], position - right - up, Vec2(uv0.x, uv1.y), color, erosion);
    SetVertex(batch.vertices[batch.used++], position + right - up, Vec2(uv1.x, uv1.y), color, erosion);
}

void AnnieQEffect::Ribbon(Flight& flight, Layer layer, float width, float lifetime, const Vec4& color)
{
    Batch& batch = _batches[layer];
    float distance = 0.f;

    for (size_t i = flight.samples.size(); i > 1; --i)
    {
        const Sample& a = flight.samples[i - 1];
        const Sample& b = flight.samples[i - 2];
        const float segmentLength = (a.position - b.position).Length();

        if (segmentLength < .0001f || _time - a.time >= lifetime)
            continue;

        if (batch.used + 4 > batch.vertices.size() || distance >= 3.6f)
            break;
        const Vec3 tangent = SafeNormal(a.position - b.position, flight.direction);
        const Vec3 side = SafeNormal(tangent.Cross(_cameraPosition - a.position), _right);
        const float u0 = min(1.f, distance / 3.6f);
        const float u1 = min(1.f, (distance + segmentLength) / 3.6f);
        const float ageA = std::clamp((_time - a.time) / lifetime, 0.f, 1.f);
        const float ageB = std::clamp((_time - b.time) / lifetime, 0.f, 1.f);
        Vec4 colorA = color, colorB = color;
        colorA.w *= (1.f - ageA) * (1.f - u0);
        colorB.w *= (1.f - ageB) * (1.f - u1);
        const Vec3 sideA = side * width * (.15f + .85f * (1.f - u0)) * .5f;
        const Vec3 sideB = side * width * (.15f + .85f * (1.f - u1)) * .5f;
        SetVertex(batch.vertices[batch.used++], a.position - sideA, Vec2(u0, 0.f), colorA, 0.f);
        SetVertex(batch.vertices[batch.used++], a.position + sideA, Vec2(u0, 1.f), colorA, 0.f);
        SetVertex(batch.vertices[batch.used++], b.position - sideB, Vec2(u1, 0.f), colorB, 0.f);
        SetVertex(batch.vertices[batch.used++], b.position + sideB, Vec2(u1, 1.f), colorB, 0.f);
        distance += segmentLength;
    }
}

void AnnieQEffect::CoreMesh(const Flight& flight)
{
    if (!_coreMesh || _coresUsed >= _cores.size())
        return;
    auto transform = _cores[_coresUsed++]->GetTransform();
    const float pulse = 1.f + .06f * sinf((_time - flight.started) * 21.f);
    const Vec3 d = flight.direction;
    transform->SetScale(Vec3(.014f * pulse, .014f * pulse, .027f));
    transform->SetRotation(Vec3(-asinf(std::clamp(d.y, -1.f, 1.f)), atan2f(d.x, d.z), 0.f));
    transform->SetPosition(flight.position);
}

void AnnieQEffect::LateUpdate()
{
    const float dt = TIME->GetDeltaTime();

    if (!std::isfinite(dt) || dt <= 0.f)
        return;
    _time += dt;

    if (auto camera = CUR_SCENE->GetMainCamera())
    {
        _cameraPosition = camera->GetTransform()->GetPosition();
        _right = SafeNormal(camera->GetTransform()->GetRight(), Vec3::Right);
        _up = SafeNormal(camera->GetTransform()->GetUp(), Vec3::Up);
    }

    _coresUsed = 0;
    _coreMaterial->GetMaterialDesc().specular = Vec4(fmodf(_time * .2f, 1.f), -fmodf(_time, 1.f), 0.f, 0.f);

    for (auto& batch : _batches)
        batch.used = 0;

    for (auto& particle : _particles)
    {
        particle.age += dt;
        particle.position += particle.velocity * dt;
    }

    std::erase_if(_particles, [](const Particle& p) { return p.age >= p.life; });

    for (auto& flight : _flights)
    {
        if (flight.stopped < 0.f)
        {
            auto anchor = flight.anchor.lock();

            if (!anchor || _time - flight.started > 10.f)
            {
                flight.stopped = _time;
                flight.anchor.reset();
            }
            else
            {
                const Vec3 next = anchor->GetTransform()->GetPosition();
                const Vec3 movement = next - flight.position;
                flight.direction = SafeNormal(movement, flight.direction);
                const Sample previous =
                    flight.samples.empty() ? Sample{flight.position, _time - dt} : flight.samples.back();
                const Vec3 sampleDelta = next - previous.position;
                const float sampleLength = sampleDelta.Length();

                if (sampleLength >= SampleDistance)
                {
                    const int samples = min(32, static_cast<int>(sampleLength / SampleDistance));

                    for (int i = 1; i <= samples; ++i)
                    {
                        const float t = min(1.f, i * SampleDistance / sampleLength);
                        flight.samples.push_back(
                            {previous.position + sampleDelta * t, previous.time + (_time - previous.time) * t});
                    }
                }

                flight.emission += dt;
                const int count = min(4, static_cast<int>(flight.emission / .05f));
                flight.emission = fmodf(flight.emission, .05f);

                for (int i = 0; i < count; ++i)
                {
                    const Vec3 position = flight.position + movement * (static_cast<float>(i + 1) / max(1, count));
                    AddParticle(Flame, position, -flight.direction * .3f, .4f, .55f, .15f, Vec4(1.f, .34f, .09f, .9f));
                    AddParticle(Smoke, position, Vec3(0.f, .3f, 0.f), 1.1f, .4f, .95f, Vec4(.23f, .16f, .17f, .35f));

                    for (int spark = 0; spark < 2; ++spark)
                        AddParticle(Sparks, position, Vec3(Random(-.5f, .5f), Random(.1f, .8f), Random(-.5f, .5f)),
                                    1.0f, Random(.04f, .08f), .01f, Vec4(1.f, .32f, .07f, 1.f));
                }

                flight.position = next;
                CoreMesh(flight);
                Quad(FireGlow, next, Vec2(.9f), 0.f, Vec4(1.f, .18f, .025f, .65f));

                if (!_coreMesh)
                    Quad(Flame, next, Vec2(.7f), _time * 3.f, Vec4(1.f), 0.f, 0, true);
            }
        }
        while (flight.samples.size() > MaxTrailPoints ||
               (!flight.samples.empty() && _time - flight.samples.front().time > SmokeTrailLifetime))
            flight.samples.pop_front();
        Ribbon(flight, SmokeTrail, .65f, SmokeTrailLifetime, Vec4(.22f, .12f, .15f, .45f));
        Ribbon(flight, Trail, .72f, TrailLifetime, Vec4(1.f, .4f, .13f, 1.f));
    }

    std::erase_if(_flights,
                  [this](const Flight& f) { return f.stopped >= 0.f && _time - f.stopped >= SmokeTrailLifetime; });

    std::stable_sort(
        _particles.begin(), _particles.end(),
        [this](const Particle& a, const Particle& b)
        { return (a.position - _cameraPosition).LengthSquared() > (b.position - _cameraPosition).LengthSquared(); });

    for (const auto& particle : _particles)
    {
        const float t = std::clamp(particle.age / particle.life, 0.f, 1.f);
        Vec4 color = particle.color;

        const float fade = std::clamp((1.f - t) / .75f, 0.f, 1.f);
        color.w *= fade * fade * (3.f - 2.f * fade);
        const float size = particle.size + (particle.endSize - particle.size) * t;
        const bool atlas = particle.layer == Flame || particle.layer == Smoke || particle.layer == Sparks ||
                           particle.layer == HitSmoke;
        const float erosion = std::clamp((t - .25f) / .75f, 0.f, 1.f) * .95f;
        Quad(particle.layer, particle.position, Vec2(size), particle.rotation, color, erosion, particle.frame, atlas);
    }

    Flush();
}

void AnnieQEffect::Flush()
{
    auto scene = _scene.lock();

    if (!scene)
        return;

    for (size_t i = 0; i < _cores.size(); ++i)
    {
        const bool visible = i < _coresUsed;

        if (visible && !_coreVisible[i])
            scene->Add(_cores[i]);

        if (!visible && _coreVisible[i])
            scene->Remove(_cores[i]);
        _coreVisible[i] = visible;
    }

    for (auto& batch : _batches)
    {
        if (batch.used == 0)
        {
            if (batch.visible)
                scene->Remove(batch.object);
            batch.visible = false;
            continue;
        }

        batch.mesh->UpdateDynamicVertices(batch.vertices, static_cast<uint32>(batch.used));

        if (!batch.visible)
            scene->Add(batch.object);
        batch.visible = true;
    }
}
