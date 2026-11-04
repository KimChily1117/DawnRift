// 데이터 기반 이펙트 재생.
#include "pch.h"
#include "LolVfx.h"
#include "Material.h"
#include "MeshRenderer.h"
#include "Camera.h"
#include "ModelAnimator.h"
#include <fstream>

namespace
{
constexpr size_t QuadsPerBatch = 1024;
constexpr size_t MeshQuadsPerBatch = 8192;
constexpr size_t MaxParticles = 4096;
constexpr size_t MaxInstances = 64;
constexpr int32 AlphaOverlayOrder = 40;
constexpr int32 AdditiveOverlayOrder = 50;

constexpr float CameraDepthBias = 0.35f;

constexpr float GroundLift = 0.02f;
const wstring ResourceRoot = L"..\\Resources\\";

const json& Field(const json& object, const char* key)
{
    static const json null;
    const auto it = object.find(key);

    return it == object.end() ? null : *it;
}

Vec3 ReadVec3(const json& value, const Vec3& fallback)
{
    if (!value.is_array() || value.size() < 3)
        return fallback;

    return Vec3(value[0].get<float>(), value[1].get<float>(), value[2].get<float>());
}

Vec4 ReadVec4(const json& value, const Vec4& fallback)
{
    if (!value.is_array() || value.size() < 3)
        return fallback;

    return Vec4(value[0].get<float>(), value[1].get<float>(), value[2].get<float>(),
                value.size() > 3 ? value[3].get<float>() : 1.f);
}

Vec3 Radians(const Vec3& degrees)
{
    return Vec3(XMConvertToRadians(degrees.x), XMConvertToRadians(degrees.y), XMConvertToRadians(degrees.z));
}

void SetVertex(VertexTextureNormalTangentData& vertex, const Vec3& position, const Vec2& uv, const Vec4& color,
               const Vec2& erosion)
{
    vertex.position = position;
    vertex.uv = uv;
    vertex.normal = Vec3(color.x, color.y, color.z);
    vertex.tangent = Vec3(color.w, erosion.x, erosion.y);
}

wstring ResourcePath(const string& relative)
{
    wstring path = ResourceRoot + Utils::ToWString(relative);
    std::replace(path.begin(), path.end(), L'/', L'\\');

    return path;
}

Vec3 SafeNormal(Vec3 value, const Vec3& fallback)
{
    if (!std::isfinite(value.LengthSquared()) || value.LengthSquared() < 0.000001f)
        return fallback;
    value.Normalize();

    return value;
}
}

Vec4 LolVfx::Curve::Evaluate(float t) const
{
    if (times.empty())
        return Vec4(1.f);

    if (t <= times.front())
        return values.front();

    for (size_t i = 1; i < times.size(); ++i)
    {
        if (t <= times[i])
        {
            const float span = times[i] - times[i - 1];
            const float s = span > 0.f ? (t - times[i - 1]) / span : 1.f;

            return Vec4::Lerp(values[i - 1], values[i], s);
        }
    }

    return values.back();
}

shared_ptr<LolVfx> LolVfx::Get()
{
    static weak_ptr<Scene> scene;
    static weak_ptr<LolVfx> instance;

    if (scene.lock() == CUR_SCENE)
        if (auto existing = instance.lock())
            return existing;
    auto object = make_shared<GameObject>("LolVfx");
    object->GetOrAddTransform();
    auto vfx = object->GetOrAddScript<LolVfx>();
    vfx->_scene = CUR_SCENE;
    vfx->_shader = make_shared<Shader>(L"LolVfx.fx");
    vfx->_particles.reserve(MaxParticles);
    CUR_SCENE->Add(object);
    scene = CUR_SCENE;
    instance = vfx;

    return vfx;
}

shared_ptr<LolVfx::MeshShape> LolVfx::LoadMesh(const string& path)
{
    if (auto it = _meshes.find(path); it != _meshes.end())
        return it->second;
    wstring file = ResourceRoot + Utils::ToWString(path);
    std::replace(file.begin(), file.end(), L'/', L'\\');
    std::ifstream stream(file);
    json root = json::parse(stream, nullptr, false);
    shared_ptr<MeshShape> shape;

    if (stream.is_open() && !root.is_discarded() && Field(root, "triangles").is_array())
    {
        shape = make_shared<MeshShape>();
        const auto& data = Field(root, "triangles");

        for (size_t i = 0; i + 5 <= data.size(); i += 5)
        {
            shape->positions.push_back(Vec3(data[i].get<float>(), data[i + 1].get<float>(), data[i + 2].get<float>()));
            shape->uvs.push_back(Vec2(data[i + 3].get<float>(), data[i + 4].get<float>()));
        }
    }

    _meshes[path] = shape;

    return shape;
}

const LolVfx::SystemDef* LolVfx::Load(const string& system)
{
    if (auto it = _systems.find(system); it != _systems.end())
        return it->second.get();

    std::ifstream file(ResourceRoot + L"Vfx\\" + Utils::ToWString(system) + L".json");
    json root = json::parse(file, nullptr, false);

    if (!file.is_open() || root.is_discarded())
    {
        DEBUG_LOG("[LolVfx] missing or invalid Resources/Vfx/" << Utils::ToWString(system) << ".json");
        _systems[system] = nullptr;

        return nullptr;
    }

    auto readCurve = [](const json& value, Curve& curve)
    {
        if (!value.is_object() || !Field(value, "times").is_array())
            return;

        for (const auto& time : Field(value, "times"))
            curve.times.push_back(time.get<float>());

        for (const auto& entry : Field(value, "values"))
            curve.values.push_back(entry.is_number() ? Vec4(entry.get<float>())
                                   : entry.is_array() && entry.size() == 3
                                       ? Vec4(ReadVec3(entry, Vec3(1.f)).x, ReadVec3(entry, Vec3(1.f)).y,
                                              ReadVec3(entry, Vec3(1.f)).z, 1.f)
                                       : ReadVec4(entry, Vec4(1.f)));

        if (curve.values.size() != curve.times.size())
            curve = Curve{};
    };

    auto def = make_unique<SystemDef>();

    for (const auto& e : Field(root, "emitters"))
    {
        if (!Field(e, "texture").is_string() || !Field(e, "skipped").empty())
            continue;
        EmitterDef emitter;
        const string primitive = e.value("primitive", "billboard");

        if (primitive == "quad")
            emitter.primitive = Primitive::Quad;
        else if (primitive == "trail")
        {
            emitter.primitive = Primitive::Trail;
            const auto& trail = Field(e, "trail");

            if (Field(trail, "tiling").is_number())
                emitter.trailTiling = Field(trail, "tiling").get<float>();

            if (Field(trail, "cutoff").is_number())
                emitter.trailCutoff = Field(trail, "cutoff").get<float>();
        }
        else if (primitive == "mesh")
        {
            emitter.primitive = Primitive::Mesh;
            emitter.mesh = Field(e, "mesh").is_string() ? LoadMesh(Field(e, "mesh").get<string>()) : nullptr;

            if (!emitter.mesh || emitter.mesh->positions.empty())
                continue;
        }

        BatchKey key;
        key.texture = Field(e, "texture").get<string>();
        key.additive = e.value("blend", "alpha") == "additive";
        key.meshCapacity = emitter.primitive == Primitive::Mesh;
        key.wrap = emitter.primitive == Primitive::Trail || Field(e, "uvScroll").is_array();

        if (Field(e, "textureMult").is_string())
            key.textureMult = Field(e, "textureMult").get<string>();
        const auto& erosion = Field(e, "erosion");

        if (erosion.is_object() && Field(erosion, "map").is_string())
        {
            emitter.erosion = true;
            key.erosionMap = Field(erosion, "map").get<string>();
            key.erosionMixer = ReadVec4(Field(erosion, "mixer"), Vec4(0.f, 0.f, 0.f, 1.f));
            emitter.erosionFeather = erosion.value("feather", 0.1f);
            readCurve(Field(erosion, "curve"), emitter.erosionCurve);
        }

        emitter.batch = BatchFor(key);
        emitter.emitterLifetime =
            Field(e, "emitterLifetime").is_number() ? Field(e, "emitterLifetime").get<float>() : -1.f;
        emitter.delay = e.value("delay", 0.f);
        emitter.rate = e.value("rate", 1.f);
        emitter.single = e.value("single", false);
        emitter.uniform = e.value("uniform", false);
        emitter.bound = e.value("bound", false);
        emitter.directionOriented = e.value("directionOriented", false);
        emitter.ray = e.value("ray", false);

        if (Field(e, "particleLife").is_array())
            emitter.particleLife =
                Vec2(Field(e, "particleLife")[0].get<float>(), Field(e, "particleLife")[1].get<float>());
        emitter.position = ReadVec3(Field(e, "position"), Vec3::Zero);
        emitter.sphereRadius = e.value("sphereRadius", 0.f);
        emitter.offsetMin = ReadVec3(Field(e, "offsetMin"), Vec3::Zero);
        emitter.offsetMax = ReadVec3(Field(e, "offsetMax"), Vec3::Zero);

        if (Field(e, "shapeRotations").is_array())
            for (const json& rotation : Field(e, "shapeRotations"))
                emitter.shapeRotations.push_back({SafeNormal(ReadVec3(Field(rotation, "axis"), Vec3::Up), Vec3::Up),
                                                  rotation.value("min", 0.f), rotation.value("max", 0.f)});
        emitter.velocityMin = ReadVec3(Field(e, "velocityMin"), Vec3::Zero);
        emitter.velocityMax = ReadVec3(Field(e, "velocityMax"), Vec3::Zero);
        emitter.drag = ReadVec3(Field(e, "drag"), Vec3::Zero);
        emitter.acceleration = ReadVec3(Field(e, "acceleration"), Vec3::Zero);
        emitter.sizeMin = ReadVec3(Field(e, "sizeMin"), Vec3(1.f));
        emitter.sizeMax = ReadVec3(Field(e, "sizeMax"), Vec3(1.f));
        emitter.rotationMin = ReadVec3(Field(e, "rotationMin"), Vec3::Zero);
        emitter.rotationMax = ReadVec3(Field(e, "rotationMax"), Vec3::Zero);
        emitter.spinMin = ReadVec3(Field(e, "spinMin"), Vec3::Zero);
        emitter.spinMax = ReadVec3(Field(e, "spinMax"), Vec3::Zero);
        emitter.colorBirth = ReadVec4(Field(e, "colorBirth"), Vec4(1.f));
        readCurve(Field(e, "sizeCurve"), emitter.sizeCurve);
        readCurve(Field(e, "colorCurve"), emitter.colorCurve);
        readCurve(Field(e, "rampCurve"), emitter.rampCurve);

        if (Field(e, "flipbook").is_object())
        {
            const auto& flipbook = Field(e, "flipbook");
            emitter.columns = max(1, flipbook.value("columns", 1));
            emitter.rows = max(1, flipbook.value("rows", 1));
            emitter.frames = max(1, flipbook.value("frames", emitter.columns * emitter.rows));
            emitter.fps = flipbook.value("fps", 0.f);
        }

        if (const auto& scroll = Field(e, "uvScroll"); scroll.is_array() && scroll.size() == 2)
            emitter.uvScroll = Vec2(scroll[0].get<float>(), scroll[1].get<float>());
        const float emission = emitter.emitterLifetime >= 0.f ? emitter.emitterLifetime : emitter.particleLife.y;
        def->burstDuration = max(def->burstDuration, emitter.delay + emission);
        def->emitters.push_back(emitter);
    }

    const SystemDef* result = def.get();
    _systems[system] = std::move(def);

    return result;
}

size_t LolVfx::BatchFor(const BatchKey& key)
{
    std::ostringstream id;
    id << key.texture << '|' << key.erosionMap << '|' << key.textureMult << '|' << key.erosionMixer.x << ','
       << key.erosionMixer.y << ',' << key.erosionMixer.z << ',' << key.erosionMixer.w << '|' << key.additive
       << key.meshCapacity << key.wrap;

    if (auto it = _batchIndex.find(id.str()); it != _batchIndex.end())
        return it->second;
    const bool meshCapacity = key.meshCapacity;
    const bool additive = key.additive;

    Batch batch;
    batch.additive = additive;
    batch.vertices.resize((meshCapacity ? MeshQuadsPerBatch : QuadsPerBatch) * 4);
    vector<uint32> indices;
    indices.reserve(batch.vertices.size() / 4 * 6);

    for (uint32 index = 0; index < batch.vertices.size(); index += 4)
        indices.insert(indices.end(), {index, index + 1, index + 2, index + 2, index + 1, index + 3});
    batch.mesh = make_shared<Mesh>();
    batch.mesh->CreateDynamic(batch.vertices, indices);

    auto load = [](const string& relative)
    {
        return RESOURCES->Load<Texture>(L"LolVfx_" + Utils::ToWString(relative), ResourcePath(relative));
    };
    auto material = make_shared<Material>();
    material->SetShader(_shader);
    auto texture = load(key.texture);
    material->SetDiffuseMap(texture);

    material->SetNormalMap(key.erosionMap.empty() ? texture : load(key.erosionMap));
    material->SetSpecularMap(key.textureMult.empty() ? texture : load(key.textureMult));
    auto& desc = material->GetMaterialDesc();
    desc.ambient =
        Vec4(key.erosionMap.empty() ? 0.f : 1.f, key.textureMult.empty() ? 0.f : 1.f, key.wrap ? 1.f : 0.f, 0.f);
    desc.diffuse = key.erosionMixer;

    batch.object = make_shared<GameObject>("LolVfxBatch");
    batch.object->GetOrAddTransform();
    batch.object->AddComponent(make_shared<MeshRenderer>());
    auto renderer = batch.object->GetMeshRenderer();
    renderer->SetMesh(batch.mesh);
    renderer->SetMaterial(material);
    renderer->SetPass(additive ? 1 : 0);
    renderer->SetWorldOverlay(additive ? AdditiveOverlayOrder : AlphaOverlayOrder);

    _batches.push_back(std::move(batch));
    _batchIndex[id.str()] = _batches.size() - 1;

    return _batches.size() - 1;
}

uint64 LolVfx::Play(const string& system, const Vec3& position, const Vec3& forward, const PlayOptions& options)
{
    const SystemDef* def = Load(system);

    if (!def || def->emitters.empty() || _instances.size() >= MaxInstances)
        return 0;
    auto instance = make_shared<Instance>();
    instance->handle = _nextHandle++;
    instance->def = def;
    instance->follow = options.follow;
    instance->followHeight = options.followHeight;
    instance->position = options.follow
                             ? options.follow->GetTransform()->GetPosition() + Vec3(0.f, options.followHeight, 0.f)
                             : position;
    instance->previousPosition = instance->position;
    instance->up = Vec3::Up;
    instance->forward = SafeNormal(Vec3(forward.x, 0.f, forward.z), Vec3::Forward);
    instance->right = SafeNormal(instance->up.Cross(instance->forward), Vec3::Right);
    instance->age = -max(0.f, options.delay);
    instance->loopDuration = max(0.f, options.duration);
    instance->orientToMotion = options.orientToMotion && options.follow;
    instance->bone = options.follow ? options.followBone : wstring();

    if (!instance->bone.empty())
        if (auto animator = options.follow->GetModelAnimator())
            animator->GetBoneWorldPosition(instance->bone, instance->position);
    instance->previousPosition = instance->position;
    instance->emitters.resize(def->emitters.size());

    if (_playLog.size() >= 64)
        _playLog.erase(_playLog.begin());
    _playLog.push_back({system, instance->position, instance->forward, TIME->GetGameTime()});
    _instances.push_back(std::move(instance));

    return _nextHandle - 1;
}

void LolVfx::Stop(uint64 handle, bool clearAttached)
{
    for (auto& instance : _instances)
        if (instance->handle == handle)
            instance->stopped = true;

    if (clearAttached)
        std::erase_if(_particles,
                      [handle](const Particle& p) { return p.anchor && p.def->bound && p.anchor->handle == handle; });
}

float LolVfx::Random(float low, float high)
{
    if (high <= low)
        return low;

    return std::uniform_real_distribution<float>(low, high)(_random);
}

Vec3 LolVfx::Random(const Vec3& low, const Vec3& high)
{
    return Vec3(Random(low.x, high.x), Random(low.y, high.y), Random(low.z, high.z));
}

void LolVfx::Spawn(const shared_ptr<Instance>& instance, const EmitterDef& def, float frameFraction)
{
    if (_particles.size() >= MaxParticles)
        return;
    Vec3 local = def.position;

    if (def.sphereRadius > 0.f)
        local += SafeNormal(Random(Vec3(-1.f), Vec3(1.f)), Vec3::Up) * Random(0.f, def.sphereRadius);
    Vec3 offset = Random(def.offsetMin, def.offsetMax);
    Vec3 velocity = Random(def.velocityMin, def.velocityMax);

    for (const EmitterDef::ShapeRotation& rotation : def.shapeRotations)
    {
        const Matrix turn = Matrix::CreateFromAxisAngle(
            rotation.axis, XMConvertToRadians(Random(rotation.minDegrees, rotation.maxDegrees)));
        offset = Vec3::TransformNormal(offset, turn);
        velocity = Vec3::TransformNormal(velocity, turn);
    }

    local += offset;

    Particle particle;
    particle.def = &def;

    if (def.bound || def.primitive != Primitive::Billboard)
        particle.anchor = instance;

    if (def.bound)
    {
        particle.position = local;
        particle.velocity = velocity;
    }
    else
    {
        particle.position = instance->position + instance->ToWorld(local);
        particle.velocity = instance->ToWorld(velocity);
    }

    particle.size = Random(def.sizeMin, def.sizeMax);

    if (def.uniform)
        particle.size.y = particle.size.z = particle.size.x;

    if (def.particleLife.y < 0.f)
        particle.life =
            instance->loopDuration > 0.f ? max(0.1f, instance->loopDuration - max(0.f, instance->age)) : 30.f;
    else
        particle.life = max(0.01f, Random(def.particleLife.x, def.particleLife.y));
    particle.rotation = Radians(Random(def.rotationMin, def.rotationMax));
    particle.spin = Radians(Random(def.spinMin, def.spinMax));

    if (def.primitive == Primitive::Trail)
    {
        particle.anchor.reset();
        const Vec3 origin = Vec3::Lerp(instance->previousPosition, instance->position, frameFraction);
        particle.position = origin + instance->ToWorld(local);
        particle.velocity = instance->ToWorld(velocity);
        particle.trail = instance->handle * 64 + static_cast<uint64>(&def - instance->def->emitters.data());
        particle.weaponTrail = !instance->bone.empty();
    }

    _particles.push_back(particle);
}

void LolVfx::LateUpdate()
{
    const float dt = TIME->GetDeltaTime();

    if (!std::isfinite(dt) || dt <= 0.f)
        return;

    if (auto camera = CUR_SCENE->GetMainCamera())
    {
        _cameraPosition = camera->GetTransform()->GetPosition();
        _right = SafeNormal(camera->GetTransform()->GetRight(), Vec3::Right);
        _up = SafeNormal(camera->GetTransform()->GetUp(), Vec3::Up);
    }

    for (auto& instance : _instances)
    {
        instance->age += dt;
        instance->previousPosition = instance->position;

        if (auto follow = instance->follow.lock())
        {
            Vec3 position = follow->GetTransform()->GetPosition() + Vec3(0.f, instance->followHeight, 0.f);

            if (!instance->bone.empty())
                if (auto animator = follow->GetModelAnimator())
                    animator->GetBoneWorldPosition(instance->bone, position);
            const Vec3 motion(position.x - instance->position.x, 0.f, position.z - instance->position.z);

            if (instance->orientToMotion && motion.LengthSquared() > 0.000001f)
            {
                instance->forward = SafeNormal(motion, instance->forward);
                instance->right = SafeNormal(instance->up.Cross(instance->forward), Vec3::Right);
            }

            instance->position = position;
        }

        if (instance->stopped || instance->age < 0.f)
            continue;

        for (size_t i = 0; i < instance->emitters.size(); ++i)
        {
            const EmitterDef& def = instance->def->emitters[i];
            EmitterState& state = instance->emitters[i];
            const float local = instance->age - def.delay;

            if (local < 0.f)
                continue;

            if (def.single)
            {
                if (!state.fired)
                    Spawn(instance, def);
                state.fired = true;
                continue;
            }

            float end = def.emitterLifetime >= 0.f     ? def.emitterLifetime
                        : instance->loopDuration > 0.f ? instance->loopDuration
                                                       : def.particleLife.y;

            if (instance->loopDuration > 0.f)
                end = min(end, instance->loopDuration - def.delay);

            if (local - dt > end)
                continue;
            state.spawnCarry += def.rate * min(dt, max(0.f, end - (local - dt)));
            const int32 count = static_cast<int32>(state.spawnCarry);

            for (int32 j = 0; j < count; ++j)
                Spawn(instance, def, (j + 1.f) / count);
            state.spawnCarry -= static_cast<float>(count);
        }
    }

    std::erase_if(
        _instances, [](const shared_ptr<Instance>& i)
        { return i->stopped || i->age > (i->loopDuration > 0.f ? i->loopDuration : i->def->burstDuration) + 0.05f; });

    for (auto& particle : _particles)
    {
        const EmitterDef& def = *particle.def;
        particle.age += dt;
        particle.velocity += def.acceleration * dt;
        particle.velocity.x *= max(0.f, 1.f - def.drag.x * dt);
        particle.velocity.y *= max(0.f, 1.f - def.drag.y * dt);
        particle.velocity.z *= max(0.f, 1.f - def.drag.z * dt);
        particle.position += particle.velocity * dt;
        particle.rotation += particle.spin * dt;
    }

    std::erase_if(_particles, [](const Particle& p) { return p.age >= p.life; });

    auto worldPosition = [](const Particle& p)
    {
        return p.anchor && p.def->bound ? p.anchor->position + p.anchor->ToWorld(p.position) : p.position;
    };
    // Alpha sprites back to front; additive order does not matter.
    for (auto& particle : _particles)
        particle.cameraDistance = (worldPosition(particle) - _cameraPosition).LengthSquared();
    std::sort(_particles.begin(), _particles.end(),
              [](const Particle& a, const Particle& b) { return a.cameraDistance > b.cameraDistance; });
    Profiler::Gauge("vfx particles", static_cast<double>(_particles.size()));

    for (auto& batch : _batches)
        batch.used = 0;
    PROFILE_SCOPE("    vfx build + trails + upload");

    for (const auto& particle : _particles)
    {
        if (particle.trail)
            continue;
        const EmitterDef& def = *particle.def;
        const float t = std::clamp(particle.age / particle.life, 0.f, 1.f);
        const Vec4 curve = def.sizeCurve.Evaluate(t);
        Vec3 size = particle.size * Vec3(curve.x, def.uniform ? curve.x : curve.y, def.uniform ? curve.x : curve.z);
        const Vec4 color = def.colorBirth * def.colorCurve.Evaluate(t) * def.rampCurve.Evaluate(t);

        if (color.w <= 0.001f)
            continue;
        const int32 frame = def.frames <= 1 ? 0
                            : def.fps > 0.f ? static_cast<int32>(particle.age * def.fps) % def.frames
                                            : min(def.frames - 1, static_cast<int32>(t * def.frames));
        Batch& batch = _batches[def.batch];
        const Vec3 center = worldPosition(particle);
        const Vec2 erosion = def.erosion ? Vec2(def.erosionCurve.Evaluate(t).x, def.erosionFeather) : Vec2(-1.f, 0.01f);

        if (def.primitive == Primitive::Billboard && def.ray)
        {
            const Vec3 right = SafeNormal(Vec3(_right.x, 0.f, _right.z), Vec3::Right) * size.x;
            const Vec3 up = Vec3(0.f, size.y, 0.f);
            const float du = 1.f / def.columns, dv = 1.f / def.rows;
            const Vec2 uv0((frame % def.columns) * du, (frame / def.columns) * dv);
            const Vec3 corners[4] = {center - right + up, center + right + up, center - right - up,
                                     center + right - up};
            const Vec2 uvs[4] = {uv0, Vec2(uv0.x + du, uv0.y), Vec2(uv0.x, uv0.y + dv), uv0 + Vec2(du, dv)};
            Polygon(batch, corners, uvs, 4, color, erosion);
            continue;
        }

        if (def.primitive == Primitive::Billboard)
        {
            Billboard(batch, center, Vec2(size.x, size.y), particle.rotation.x, color, erosion, frame, def);
            continue;
        }

        const Instance& frameOwner = *particle.anchor;
        Vec3 rotation = particle.rotation;

        if (def.directionOriented)
        {
            Vec3 v = particle.velocity;

            if (!def.bound)
                v = Vec3(v.Dot(frameOwner.right), v.Dot(frameOwner.up), v.Dot(frameOwner.forward));

            if (v.x * v.x + v.z * v.z > 0.000001f)
                rotation.y += atan2f(v.x, v.z);
        }

        const Matrix orientation = Matrix::CreateFromYawPitchRoll(rotation.y, rotation.x, rotation.z);
        const Vec3 origin = center + Vec3(0.f, GroundLift, 0.f);
        auto place = [&](const Vec3& emitterPoint)
        {
            return origin + frameOwner.ToWorld(Vec3::Transform(emitterPoint, orientation));
        };

        if (def.primitive == Primitive::Quad)
        {
            const float du = 1.f / def.columns, dv = 1.f / def.rows;
            const Vec2 uv0((frame % def.columns) * du, (frame / def.columns) * dv);
            const Vec3 corners[4] = {place(Vec3(-size.x, size.y, 0.f)), place(Vec3(size.x, size.y, 0.f)),
                                     place(Vec3(-size.x, -size.y, 0.f)), place(Vec3(size.x, -size.y, 0.f))};
            const Vec2 scroll = def.uvScroll * particle.age;
            const Vec2 uvs[4] = {uv0 + scroll, Vec2(uv0.x + du, uv0.y) + scroll, Vec2(uv0.x, uv0.y + dv) + scroll,
                                 uv0 + Vec2(du, dv) + scroll};
            Polygon(batch, corners, uvs, 4, color, erosion);
            continue;
        }

        const MeshShape& mesh = *def.mesh;
        const size_t cornerCount = mesh.positions.size() / 3 * 3;

        if (cornerCount == 0)
            continue;
        const Matrix basis(frameOwner.right.x, frameOwner.right.y, frameOwner.right.z, 0.f, frameOwner.up.x,
                           frameOwner.up.y, frameOwner.up.z, 0.f, frameOwner.forward.x, frameOwner.forward.y,
                           frameOwner.forward.z, 0.f, 0.f, 0.f, 0.f, 1.f);
        const Matrix transform = Matrix::CreateScale(size) * orientation * basis * Matrix::CreateTranslation(origin);
        _meshScratch.resize(cornerCount);
        XMVector3TransformStream(_meshScratch.data(), sizeof(XMFLOAT4), mesh.positions.data(), sizeof(Vec3),
                                 cornerCount, transform);
        const Vec2 scroll = def.uvScroll * particle.age;

        for (size_t i = 0; i < cornerCount; i += 3)
        {
            if (batch.used + 4 > batch.vertices.size())
                break;
            const Vec3 corners[3] = {Vec3(_meshScratch[i].x, _meshScratch[i].y, _meshScratch[i].z),
                                     Vec3(_meshScratch[i + 1].x, _meshScratch[i + 1].y, _meshScratch[i + 1].z),
                                     Vec3(_meshScratch[i + 2].x, _meshScratch[i + 2].y, _meshScratch[i + 2].z)};
            const Vec2 uvs[3] = {mesh.uvs[i] + scroll, mesh.uvs[i + 1] + scroll, mesh.uvs[i + 2] + scroll};
            Polygon(batch, corners, uvs, 3, color, erosion);
        }
    }

    {
        PROFILE_SCOPE("    vfx trails");
        DrawTrails();
    }

    PROFILE_SCOPE("    vfx upload");
    Flush();
}

void LolVfx::DrawTrails()
{
    struct Point
    {
        Vec3 position;
        Vec4 color;
        float width = 0.f, distance = 0.f;
        float age = 0.f;
    };

    vector<const Particle*> trail;

    for (const auto& particle : _particles)
        if (particle.trail)
            trail.push_back(&particle);
    // One ribbon per (effect, emitter), newest point first.
    std::sort(trail.begin(), trail.end(),
              [](const Particle* a, const Particle* b)
              { return a->trail != b->trail ? a->trail < b->trail : a->age < b->age; });

    vector<Point> points;
    _trailSegments = 0;

    for (size_t begin = 0; begin < trail.size();)
    {
        size_t end = begin;

        while (end < trail.size() && trail[end]->trail == trail[begin]->trail)
            ++end;
        const EmitterDef& def = *trail[begin]->def;
        const bool weaponTrail = trail[begin]->weaponTrail;
        points.clear();

        for (size_t i = begin; i < end; ++i)
        {
            const Particle& particle = *trail[i];
            const float t = std::clamp(particle.age / particle.life, 0.f, 1.f);
            Point point;
            point.position = particle.position;
            point.color = def.colorBirth * def.colorCurve.Evaluate(t) * def.rampCurve.Evaluate(t);
            point.width = particle.size.x * def.sizeCurve.Evaluate(t).x;
            point.age = t;

            if (!points.empty())
            {
                const float step = (point.position - points.back().position).Length();

                if (step < 0.001f)
                    continue;
                point.distance = points.back().distance + step;

                if (def.trailCutoff > 0.f && point.distance > def.trailCutoff)
                    break;
            }

            points.push_back(point);
        }

        begin = end;

        if (points.size() < 2)
            continue;

        Batch& batch = _batches[def.batch];
        const float length = points.back().distance;
        auto edge = [&](size_t i, Vec3& left, Vec3& right, float& u)
        {
            const Vec3 tangent = SafeNormal(
                points[i == 0 ? 0 : i - 1].position - points[min(i + 1, points.size() - 1)].position, Vec3::Forward);
            const Vec3 side =
                SafeNormal(tangent.Cross(_cameraPosition - points[i].position), _right) * (points[i].width * 0.5f);
            left = points[i].position - side;
            right = points[i].position + side;

            if (weaponTrail)
                u = points[i].age;
            else
                u = def.trailTiling > 0.f ? points[i].distance / def.trailTiling
                                          : points[i].distance / max(length, 0.001f);
        };
        Vec3 left0, right0;
        float u0;
        edge(0, left0, right0, u0);

        for (size_t i = 1; i < points.size(); ++i)
        {
            Vec3 left1, right1;
            float u1;
            edge(i, left1, right1, u1);
            const Vec3 corners[4] = {left0, right0, left1, right1};
            const Vec2 uvs[4] = {Vec2(u0, 0.f), Vec2(u0, 1.f), Vec2(u1, 0.f), Vec2(u1, 1.f)};
            const Vec4 colors[4] = {points[i - 1].color, points[i - 1].color, points[i].color, points[i].color};
            Polygon(batch, corners, uvs, 4, colors, Vec2(-1.f, 0.01f));
            ++_trailSegments;
            left0 = left1;
            right0 = right1;
            u0 = u1;
        }
    }
}

void LolVfx::Billboard(Batch& batch, const Vec3& center, const Vec2& halfSize, float roll, const Vec4& color,
                       const Vec2& erosion, int32 frame, const EmitterDef& def)
{
    const float bias =
        batch.additive ? std::clamp(0.7f * max(halfSize.x, halfSize.y), CameraDepthBias, 4.f) : CameraDepthBias;
    const Vec3 position = center + SafeNormal(_cameraPosition - center, Vec3::Zero) * bias;
    const float c = cosf(roll), s = sinf(roll);
    const Vec3 right = (_right * c + _up * s) * halfSize.x;
    const Vec3 up = (_up * c - _right * s) * halfSize.y;
    const float du = 1.f / def.columns, dv = 1.f / def.rows;
    const Vec2 uv0((frame % def.columns) * du, (frame / def.columns) * dv);
    const Vec3 corners[4] = {position - right + up, position + right + up, position - right - up,
                             position + right - up};
    const Vec2 uvs[4] = {uv0, Vec2(uv0.x + du, uv0.y), Vec2(uv0.x, uv0.y + dv), uv0 + Vec2(du, dv)};
    Polygon(batch, corners, uvs, 4, color, erosion);
}

void LolVfx::Polygon(Batch& batch, const Vec3* corners, const Vec2* uvs, size_t count, const Vec4& color,
                     const Vec2& erosion)
{
    const Vec4 colors[4] = {color, color, color, color};
    Polygon(batch, corners, uvs, count, colors, erosion);
}

void LolVfx::Polygon(Batch& batch, const Vec3* corners, const Vec2* uvs, size_t count, const Vec4* colors,
                     const Vec2& erosion)
{
    if (batch.used + 4 > batch.vertices.size())
        return;

    for (size_t i = 0; i < 4; ++i)
    {
        const size_t source = min(i, count - 1);
        SetVertex(batch.vertices[batch.used++], corners[source], uvs[source], colors[source], erosion);
    }
}

void LolVfx::Flush()
{
    auto scene = _scene.lock();

    if (!scene)
        return;

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
