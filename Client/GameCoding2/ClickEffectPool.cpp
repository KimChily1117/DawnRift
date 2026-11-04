#include "pch.h"
#include "ClickEffectPool.h"
#include "Model.h"
#include "ModelRenderer.h"

namespace
{
constexpr float EffectLifeTime = 0.45f;
constexpr float EffectHeight = 1.95f;
constexpr float MoveStartScale = 0.012f;
constexpr float AttackStartScale = 0.016f;
constexpr float EndScaleRatio = 0.15f;
constexpr size_t MoveEffectCount = 6;
constexpr size_t AttackEffectCount = 4;

shared_ptr<Model> LoadClickModel(const wstring& material)
{
    auto model = make_shared<Model>();
    model->ReadModel(L"Effect/Cursor");
    model->ReadMaterial(material);

    return model;
}
}

void ClickEffectController::Update()
{
    if (!_playing)
        return;

    _elapsed += DT;

    if (_elapsed >= EffectLifeTime)
    {
        Hide();

        return;
    }

    if (auto follow = _follow.lock())
        SetPosition(follow->GetTransform()->GetPosition());

    // Ease-out: the arrows snap inward first, then settle.
    const float t = _elapsed / EffectLifeTime;
    const float eased = 1.f - (1.f - t) * (1.f - t);
    const float scale = _startScale * (1.f - (1.f - EndScaleRatio) * eased);
    GetTransform()->SetLocalScale(Vec3(scale));
}

void ClickEffectController::Play(const Vec3& position, shared_ptr<GameObject> follow, float startScale)
{
    _playing = true;
    _elapsed = 0.f;
    _startScale = startScale;
    _follow = follow;
    SetPosition(position);
    GetTransform()->SetLocalScale(Vec3(startScale));
}

void ClickEffectController::SetPosition(const Vec3& position)
{
    GetTransform()->SetPosition(Vec3(position.x, EffectHeight, position.z));
}

void ClickEffectController::Hide()
{
    _playing = false;
    _follow.reset();
    GetTransform()->SetLocalScale(Vec3(0.f));
}

void ClickEffectPool::Init(shared_ptr<Shader> shader)
{
    if (!_moveEffects.empty())
        return;

    auto createEffects = [&](const wstring& material, size_t count, vector<shared_ptr<ClickEffectController>>& out)
    {
        auto model = LoadClickModel(material);

        for (size_t i = 0; i < count; ++i)
        {
            auto object = make_shared<GameObject>("ClickEffect");
            object->GetOrAddTransform()->SetRotation(Vec3(XMConvertToRadians(90.f), 0.f, 0.f));
            auto controller = make_shared<ClickEffectController>();
            object->AddComponent(controller);
            object->AddComponent(make_shared<ModelRenderer>(shader));
            object->GetModelRenderer()->SetModel(model);
            object->GetModelRenderer()->SetPass(0);
            CUR_SCENE->Add(object);
            controller->Hide();
            out.push_back(controller);
        }
    };

    createEffects(L"Effect/Cursor", MoveEffectCount, _moveEffects);
    createEffects(L"Effect/CursorAttack", AttackEffectCount, _attackEffects);
}

void ClickEffectPool::PlayAt(Kind kind, const Vec3& position)
{
    auto effect = Acquire(kind);

    if (!effect)
        return;
    effect->Play(position, nullptr, kind == Kind::Move ? MoveStartScale : AttackStartScale);

    if (kind == Kind::Move)
        _lastMoveEffect = effect;
}

void ClickEffectPool::PlayOnTarget(shared_ptr<GameObject> target)
{
    if (!target)
        return;

    if (auto effect = Acquire(Kind::Attack))
        effect->Play(target->GetTransform()->GetPosition(), target, AttackStartScale);
}

void ClickEffectPool::MoveLastMoveEffect(const Vec3& position)
{
    auto effect = _lastMoveEffect.lock();

    if (effect && effect->IsPlaying())
        effect->SetPosition(position);
}

shared_ptr<ClickEffectController> ClickEffectPool::Acquire(Kind kind)
{
    auto& effects = kind == Kind::Move ? _moveEffects : _attackEffects;
    auto& next = kind == Kind::Move ? _nextMove : _nextAttack;

    if (effects.empty())
        return nullptr;

    auto effect = effects[next];
    next = (next + 1) % effects.size();

    return effect;
}
