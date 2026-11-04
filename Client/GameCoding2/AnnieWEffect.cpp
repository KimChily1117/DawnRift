#include "pch.h"
#include "AnnieWEffect.h"
#include "AnnieSkillTuning.h"
#include "Camera.h"
#include "Material.h"
#include "MeshRenderer.h"

namespace
{
constexpr float Duration = 3.4f;
constexpr float Range = AnnieSkillTuning::WRange;
constexpr float SpatialScale = AnnieSkillTuning::WVisualScale;
constexpr float GroundScale = Range / 52.153f;
const wstring AssetRoot = L"..\\Resources\\Textures\\Annie\\Particles\\OriginalW\\";

float Saturate(float value)
{
    return std::clamp(value, 0.f, 1.f);
}

float Hash(uint32 n)
{
    n ^= n >> 16;
    n *= 0x7feb352d;
    n ^= n >> 15;
    n *= 0x846ca68b;
    n ^= n >> 16;

    return (n & 0xffff) / 65535.f;
}
}

shared_ptr<AnnieWEffect> AnnieWEffect::Get()
{
    static weak_ptr<Scene> scene;
    static weak_ptr<AnnieWEffect> instance;

    if (scene.lock() == CUR_SCENE)
        if (auto existing = instance.lock())
            return existing;
    auto object = make_shared<GameObject>("AnnieWEffects");
    object->GetOrAddTransform();
    auto effect = object->GetOrAddScript<AnnieWEffect>();
    effect->_scene = CUR_SCENE;
    effect->Initialize();
    CUR_SCENE->Add(object);
    scene = CUR_SCENE;
    instance = effect;

    return effect;
}

void AnnieWEffect::Initialize()
{
    auto shader = make_shared<Shader>(L"AnnieW.fx");
    const wchar_t* names[] = {L"annie_base_w_cone_1", L"annie_base_w_cone_1_edge", L"annie_base_w_cone_1_edge2",
                              L"annie_base_w_cone_2"};
    std::array<shared_ptr<Mesh>, 4> meshes;
    _ready = true;

    for (size_t i = 0; i < meshes.size(); ++i)
    {
        meshes[i] = make_shared<Mesh>();

        if (!meshes[i]->LoadVfxMesh(L"..\\Resources\\Models\\Annie\\Vfx\\" + wstring(names[i]) + L".vfxmesh"))
            _ready = false;
    }

    if (!_ready)
        DEBUG_LOG("[AnnieW] Missing/invalid baked mesh.");
    auto texture = [](const wchar_t* name)
    {
        return RESOURCES->Load<Texture>(L"AnnieWOriginal_" + wstring(name), AssetRoot + name + L".dds");
    };
    auto ground = texture(L"annie_base_w_grounddecalfinal");
    auto flames = texture(L"annie_base_flames2");
    auto shapes = texture(L"annie_base_fireshapes_00");
    auto noise = texture(L"annie_base_w_erosionpack");
    auto mask = texture(L"annie_base_w_mask");
    const size_t meshIndex[] = {0, 0, 1, 2, 3, 3};

    for (auto& cast : _casts)
        for (size_t i = 0; i < LayerCount; ++i)
        {
            auto& surface = cast.surfaces[i];
            surface.material = make_shared<Material>();
            surface.material->SetShader(shader);
            surface.material->SetDiffuseMap(i == Ground ? ground : i >= Flame ? shapes : flames);
            surface.material->SetNormalMap(noise);
            surface.material->SetSpecularMap(mask);
            surface.object = make_shared<GameObject>("AnnieWSurface");
            surface.object->GetOrAddTransform();
            surface.object->AddComponent(make_shared<MeshRenderer>());
            auto renderer = surface.object->GetMeshRenderer();
            renderer->SetMesh(meshes[meshIndex[i]]);
            renderer->SetMaterial(surface.material);
            renderer->SetPass(i == Ground ? 0 : 1);
            renderer->SetWorldOverlay(25 + static_cast<int32>(i));
        }

    _vertices.resize(MaxCasts * 64 * 4);
    vector<uint32> indices;

    for (uint32 i = 0; i < _vertices.size(); i += 4)
        indices.insert(indices.end(), {i, i + 1, i + 2, i + 2, i + 1, i + 3});
    _sparkMesh = make_shared<Mesh>();
    _sparkMesh->CreateDynamic(_vertices, indices);
    auto material = make_shared<Material>();
    material->SetShader(shader);
    material->SetDiffuseMap(texture(L"annie_base_e_small_mote"));
    material->SetNormalMap(noise);
    material->SetSpecularMap(mask);
    material->GetMaterialDesc().ambient = Vec4(0.f, 3.f, 0.f, 0.f);
    material->GetMaterialDesc().diffuse = Vec4(1.f);
    _sparks = make_shared<GameObject>("AnnieWSparks");
    _sparks->GetOrAddTransform();
    _sparks->AddComponent(make_shared<MeshRenderer>());
    _sparks->GetMeshRenderer()->SetMesh(_sparkMesh);
    _sparks->GetMeshRenderer()->SetMaterial(material);
    _sparks->GetMeshRenderer()->SetPass(0);
    _sparks->GetMeshRenderer()->SetWorldOverlay(31);
}

bool AnnieWEffect::Play(const Vec3& origin, const Vec3& direction)
{
    if (!_ready || !std::isfinite(origin.x) || !std::isfinite(origin.y) || !std::isfinite(origin.z))
        return false;
    Vec3 forward(direction.x, 0.f, direction.z);

    if (!std::isfinite(forward.LengthSquared()) || forward.LengthSquared() < .000001f)
        return false;

    forward.Normalize();

    for (auto& cast : _casts)
        if (!cast.active)
        {
            cast.active = true;
            cast.age = 0.f;
            cast.origin = origin;

            if (auto scene = _scene.lock())
                if (auto terrain = scene->GetTerrainObj())
                    cast.origin.y = terrain->GetTransform()->GetPosition().y;
            cast.direction = forward;
            cast.seed = _nextSeed++;

            return true;
        }
    return false;
}

void AnnieWEffect::UpdateCast(Cast& cast)
{
    auto scene = _scene.lock();

    if (!scene)
        return;
    const float t = cast.age;
    const float yaw = atan2f(cast.direction.x, cast.direction.z);
    const float spread = .16f + .84f * Saturate(t / .24f);

    for (size_t i = 0; i < LayerCount; ++i)
    {
        auto& surface = cast.surfaces[i];
        float alpha = 0.f;

        if (cast.active)
        {
            if (i == Ground)
                alpha = Saturate(t / .09f) * (.46f + .28f * expf(-t * 3.f)) * Saturate((Duration - t) / .9f);
            else if (i == Wave)
                alpha = Saturate(t / .035f) * Saturate((2.4f - t) / 1.1f);
            else if (i == Edge || i == InnerEdge)
                alpha = Saturate(t / .07f) * Saturate((2.25f - t) / 1.f) * (i == Edge ? 1.f : .95f);
            else
            {
                const float start = i == Flame ? 0.f : .09f;
                alpha = Saturate((t - start) / .08f) * Saturate((2.1f + start - t) / .9f);
            }
        }

        const bool visible = alpha > .001f;

        if (visible && !surface.visible)
            scene->Add(surface.object);

        if (!visible && surface.visible)
            scene->Remove(surface.object);
        surface.visible = visible;

        if (!visible)
            continue;
        auto transform = surface.object->GetTransform();
        const float scale = GroundScale * (i == Ground ? 1.f : spread);
        transform->SetRotation(Vec3(0.f, yaw + (i <= Wave ? XM_PI : 0.f), 0.f));
        transform->SetScale(Vec3(scale, scale, scale));
        transform->SetPosition(cast.origin + Vec3(0.f, .045f + static_cast<float>(i) * .015f, 0.f));

        if (i >= Flame)
        {
            const float phase = i == Flame ? t : max(0.f, t - .09f);
            const float size = .2f + .8f * Saturate(phase / .25f);
            transform->SetScale(Vec3(.018f * size, .017f * (.8f + .2f * sinf(t * 22.f)), .0123f * size) * SpatialScale);
            transform->SetPosition(cast.origin + Vec3(0.f, .24f, 0.f));
        }

        auto& desc = surface.material->GetMaterialDesc();

        desc.ambient = Vec4(t,
                            i == Ground  ? 0.f
                            : i >= Flame ? 2.f
                                         : 1.f,
                            static_cast<float>(i) * .19f, Saturate((t - .75f) / 1.65f));
        desc.diffuse = i == Ground  ? Vec4(1.f, .17f, .025f, alpha)
                       : i >= Flame ? Vec4(1.35f, .60f, .18f, alpha)
                                    : Vec4(1.f, .25f, .05f, alpha);
        desc.specular = Vec4(cast.origin.x, cast.origin.y, cast.origin.z, Range);
        desc.emissive = Vec4(cast.direction.x, cast.direction.z,
                             cosf(XMConvertToRadians(AnnieSkillTuning::WConeAngleDegrees * .5f)), 0.f);
    }
}

void AnnieWEffect::Sparks(const Cast& cast, const Vec3& right, const Vec3& up)
{
    const Vec3 side(cast.direction.z, 0.f, -cast.direction.x);

    for (uint32 i = 0; i < 64; ++i)
    {
        const uint32 seed = cast.seed * 127 + i * 13;
        const float birth = i < 44 ? Hash(seed) * .65f : .7f + Hash(seed) * 1.65f;
        const float age = cast.age - birth, life = .55f + Hash(seed + 1) * .4f;

        if (age < 0.f || age >= life)
            continue;
        const float angle = (Hash(seed + 2) - .5f) * XMConvertToRadians(43.f);
        const float distance = (.45f + Hash(seed + 3) * 3.1f) * SpatialScale;
        const Vec3 radial = cast.direction * cosf(angle) + side * sinf(angle);
        const Vec3 position = cast.origin + radial * (distance + age * .3f * SpatialScale) +
                              Vec3(0.f, .15f + age * (.6f + Hash(seed + 4) * 1.2f), 0.f);
        const float fade = Saturate((1.f - age / life) / .8f);
        const float size = (.035f + Hash(seed + 5) * .065f) * fade;
        const Vec3 horizontal = right * size * .5f, vertical = up * size;
        const Vec3 points[] = {position - horizontal + vertical, position + horizontal + vertical,
                               position - horizontal - vertical, position + horizontal - vertical};
        const Vec2 uv[] = {Vec2(0, 0), Vec2(1, 0), Vec2(0, 1), Vec2(1, 1)};

        for (int corner = 0; corner < 4; ++corner)
        {
            auto& vertex = _vertices[_used++];
            vertex.position = points[corner];
            vertex.uv = uv[corner];
            vertex.normal = Vec3(1.f, .32f + .3f * fade, .08f);
            vertex.tangent = Vec3(fade, 0.f, 0.f);
        }
    }
}

void AnnieWEffect::LateUpdate()
{
    const float dt = TIME->GetDeltaTime();

    if (!std::isfinite(dt) || dt <= 0.f)
        return;
    auto scene = _scene.lock();

    if (!scene)
        return;
    Vec3 right = Vec3::Right, up = Vec3::Up;

    if (auto camera = scene->GetMainCamera())
    {
        right = camera->GetTransform()->GetRight();
        up = camera->GetTransform()->GetUp();
    }

    _used = 0;

    for (auto& cast : _casts)
    {
        if (!cast.active)
            continue;
        cast.age += dt;

        if (cast.age >= Duration)
            cast.active = false;
        UpdateCast(cast);

        if (cast.active)
            Sparks(cast, right, up);
    }

    if (_used)
    {
        _sparkMesh->UpdateDynamicVertices(_vertices, static_cast<uint32>(_used));

        if (!_sparksVisible)
            scene->Add(_sparks);
    }
    else if (_sparksVisible)
        scene->Remove(_sparks);
    _sparksVisible = _used != 0;
}
