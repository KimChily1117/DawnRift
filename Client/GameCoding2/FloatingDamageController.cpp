#include "pch.h"
#include "FloatingDamageController.h"
#include "Material.h"
#include "MeshRenderer.h"

void FloatingDamageController::Spawn(const shared_ptr<GameObject>& target, int32 damage)
{
    if (!target || damage <= 0 || !CUR_SCENE)
        return;
    auto root = make_shared<GameObject>("FloatingDamage");
    root->GetOrAddTransform();
    auto controller = make_shared<FloatingDamageController>();
    root->AddComponent(controller);
    controller->Configure(target, damage);
    CUR_SCENE->Add(root);
}

void FloatingDamageController::Configure(const shared_ptr<GameObject>& target, int32 damage)
{
    _target = target;
    _damage = max(0, damage);

    struct StackState
    {
        float lastSpawnTime = -100.f;
        uint32 nextSlot = 0;
    };

    static unordered_map<GameObject*, StackState> targetStacks;
    const float now = TIME ? TIME->GetGameTime() : 0.f;
    auto& stack = targetStacks[target.get()];

    if (now - stack.lastSpawnTime > _lifetime)
        stack.nextSlot = 0;
    const uint32 slot = stack.nextSlot++ % 5u;
    stack.lastSpawnTime = now;
    _verticalStackOffset = static_cast<float>(slot) * 0.42f;
}

void FloatingDamageController::Start()
{
    static array<shared_ptr<Material>, 10> digitMaterials;
    auto target = _target.lock();
    auto shader = CUR_SCENE ? CUR_SCENE->_shader : nullptr;
    auto quad = RESOURCES->Get<Mesh>(L"ParticleQuad");

    if (!target || !shader || !quad || _damage <= 0)
    {
        _expired = true;

        return;
    }

    const string text = std::to_string(_damage);
    _digits.reserve(text.size());

    for (char character : text)
    {
        const int32 digit = character - '0';

        if (!digitMaterials[digit] || digitMaterials[digit]->GetShader() != shader)
        {
            auto material = make_shared<Material>();
            material->SetShader(shader);
            material->GetMaterialDesc().ambient.x = static_cast<float>(digit);
            material->GetMaterialDesc().diffuse = Color(1.f, 0.48f, 0.05f, 1.f);
            digitMaterials[digit] = material;
        }

        auto digitObject = make_shared<GameObject>("FloatingDamageDigit");
        digitObject->SetLayerIndex(target->GetLayerIndex());
        digitObject->GetOrAddTransform();
        digitObject->AddComponent(make_shared<MeshRenderer>());
        digitObject->GetMeshRenderer()->SetMaterial(digitMaterials[digit]);
        digitObject->GetMeshRenderer()->SetMesh(quad);
        digitObject->GetMeshRenderer()->SetPass(14);
        CUR_SCENE->Add(digitObject);
        _digits.push_back(digitObject);
    }

    Vec3 initialAnchor = target->GetTransform()->GetPosition();
    initialAnchor.y += 3.5f + _verticalStackOffset;
    GetTransform()->SetPosition(initialAnchor);
    const float initialCenter = (_digits.size() - 1) * 0.5f;

    for (size_t i = 0; i < _digits.size(); ++i)
    {
        Vec3 position = initialAnchor;
        position.x += (static_cast<float>(i) - initialCenter) * 0.42f;
        _digits[i]->GetTransform()->SetPosition(position);
        _digits[i]->GetTransform()->SetScale(Vec3(0.38f, 0.62f, 1.f));
    }
}

void FloatingDamageController::Update()
{
    if (_expired)
    {
        RemoveVisuals();

        return;
    }

    const float deltaTime = TIME->GetDeltaTime();

    if (!std::isfinite(deltaTime) || deltaTime <= 0.f)
        return;
    _elapsed += deltaTime;
    const float t = min(1.f, _elapsed / _lifetime);
    const float popIn = t < 0.14f ? 0.65f + (t / 0.14f) * 0.55f : 1.2f;
    const float settle = 1.2f - min(1.f, max(0.f, (t - 0.14f) / 0.46f)) * 0.2f;
    const float fadeScale = t < 0.72f ? 1.f : max(0.f, 1.f - (t - 0.72f) / 0.28f);
    const float pop = (t < 0.14f ? popIn : settle) * fadeScale;

    Vec3 anchor = GetTransform()->GetPosition();

    if (auto target = _target.lock())
        anchor = target->GetTransform()->GetPosition();
    anchor.y += 3.5f + _verticalStackOffset + t * 1.35f;
    GetTransform()->SetPosition(anchor);

    const float center = (_digits.size() - 1) * 0.5f;

    for (size_t i = 0; i < _digits.size(); ++i)
    {
        Vec3 position = anchor;
        position.x += (static_cast<float>(i) - center) * 0.64f * pop;
        position.y += (i % 2 == 0 ? 0.03f : -0.03f);
        _digits[i]->GetTransform()->SetPosition(position);
        _digits[i]->GetTransform()->SetScale(Vec3(0.58f * pop, 0.94f * pop, 1.f));
    }

    if (_elapsed >= _lifetime)
    {
        _expired = true;
        RemoveVisuals();
    }
}

void FloatingDamageController::RemoveVisuals()
{
    if (!CUR_SCENE)
        return;

    for (auto& digit : _digits)
        if (digit)
            CUR_SCENE->Remove(digit);
    _digits.clear();

    if (auto root = GetGameObject())
        CUR_SCENE->Remove(root);
}
