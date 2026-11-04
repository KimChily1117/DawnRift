#include "pch.h"
#include "SkillIndicatorController.h"
#include "MeshRenderer.h"
#include "Material.h"
#include "BasePlayerController.h"
#include "GroundHeight.h"

void SkillIndicatorController::Awake()
{
    if (!_material)
    {
        _material = make_shared<Material>();
        _material->SetShader(make_shared<Shader>(L"SkillRange.fx"));
        auto renderer = GetGameObject()->GetMeshRenderer();

        if (!renderer)
        {
            renderer = make_shared<MeshRenderer>();
            GetGameObject()->AddComponent(renderer);
        }

        renderer->SetMesh(RESOURCES->Get<Mesh>(L"Quad"));
        renderer->SetMaterial(_material);
        renderer->SetPass(0);
        renderer->SetWorldOverlay(5);
    }

    Hide();
}

void SkillIndicatorController::Update()
{
    if (!_visible)
        return;

    auto owner = _owner.lock();

    if (!owner)
    {
        Hide();

        return;
    }

    if (auto player = owner->GetScript<BasePlayerController>())
        if (player->_playerInfo && player->_playerInfo->hp() <= 0)
        {
            Hide();

            return;
        }

    Vec3 origin = owner->GetTransform()->GetPosition();

    if (GroundHeight::IsActive())
        origin.y = GroundHeight::At(origin.x, origin.z);
    else if (auto terrain = CUR_SCENE->GetTerrainObj())
        origin.y = terrain->GetTransform()->GetPosition().y;
    origin.y += .035f;

    if (_circle)
    {
        GetTransform()->SetPosition(origin);
        GetTransform()->SetRotation(Vec3(XM_PIDIV2, 0.f, 0.f));
        GetTransform()->SetScale(Vec3(_range * 2.f, _range * 2.f, 1.f));

        return;
    }

    Vec3 direction = _aimPosition - origin;
    direction.y = 0.f;

    if (direction.LengthSquared() <= 0.0001f)
        direction = owner->GetTransform()->GetLook();
    direction.y = 0.f;

    if (direction.LengthSquared() <= 0.0001f)
        direction = Vec3(0.f, 0.f, 1.f);
    direction.Normalize();

    Vec3 position = origin + direction * (_range * 0.5f);
    GetTransform()->SetPosition(position);

    const float yaw = atan2f(direction.x, direction.z);
    GetTransform()->SetRotation(Vec3(XMConvertToRadians(90.f), yaw, 0.f));

    if (_line)
    {
        GetTransform()->SetScale(Vec3(_halfWidth * 2.f, _range, 1.f));

        return;
    }

    const float halfAngle = XMConvertToRadians(_angleDegrees * 0.5f);
    const float coneWidth = 2.f * _range * sinf(halfAngle);
    GetTransform()->SetScale(Vec3(max(0.1f, coneWidth), _range, 1.f));
}

void SkillIndicatorController::Show(const shared_ptr<GameObject>& owner, float range, float angleDegrees)
{
    if (!owner || !std::isfinite(range) || range <= 0.f || !std::isfinite(angleDegrees) || angleDegrees <= 0.f ||
        angleDegrees >= 180.f)
        return;

    _owner = owner;
    _range = range;
    _angleDegrees = angleDegrees;
    _circle = false;
    _line = false;
    _visible = true;
    const float halfAngle = XMConvertToRadians(_angleDegrees * .5f);
    _material->GetMaterialDesc().ambient = Vec4(0.f, sinf(halfAngle), cosf(halfAngle), .05f / _range);
    _material->GetMaterialDesc().diffuse = Vec4(.12f, .82f, 1.f, .9f);
    Update();
}

void SkillIndicatorController::ShowCircle(const shared_ptr<GameObject>& owner, float range)
{
    if (!owner || !std::isfinite(range) || range <= 0.f)
        return;
    _owner = owner;
    _range = range;
    _circle = true;
    _line = false;
    _visible = true;
    _material->GetMaterialDesc().ambient = Vec4(1.f, 0.f, 0.f, .05f / _range);
    _material->GetMaterialDesc().diffuse = Vec4(.18f, .86f, 1.f, .85f);
    Update();
}

void SkillIndicatorController::ShowLine(const shared_ptr<GameObject>& owner, float range, float halfWidth)
{
    if (!owner || !std::isfinite(range) || range <= 0.f || !std::isfinite(halfWidth) || halfWidth <= 0.f)
        return;
    _owner = owner;
    _range = range;
    _halfWidth = halfWidth;
    _circle = false;
    _line = true;
    _visible = true;

    _material->GetMaterialDesc().ambient = Vec4(-1.f, halfWidth / range, 0.f, .05f / range);
    _material->GetMaterialDesc().diffuse = Vec4(.12f, .82f, 1.f, .9f);
    Update();
}

void SkillIndicatorController::Hide()
{
    _visible = false;
    _owner.reset();
    GetTransform()->SetScale(Vec3::Zero);
}

void SkillIndicatorController::SetAimPosition(const Vec3& worldPosition)
{
    if (std::isfinite(worldPosition.x) && std::isfinite(worldPosition.y) && std::isfinite(worldPosition.z))
        _aimPosition = worldPosition;
}
