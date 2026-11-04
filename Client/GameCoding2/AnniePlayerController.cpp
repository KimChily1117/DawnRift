// 애니 조작과 서버 입력 전송.
#include "pch.h"
#include "GameSounds.h"
#include "AnniePlayerController.h"
#include "ModelAnimator.h"
#include "TimeManager.h"
#include "ClientPacketHandler.h"
#include "AnnieQSpell.h"
#include "AnnieWSpell.h"
#include "AnnieESpell.h"
#include "AnnieRSpell.h"
#include "GeneralAttack.h"

void AnniePlayerController::ProcSkill(int32 skillId)
{
    if (IsActionBusy())
        return;

    AlignToTarget();
    shared_ptr<ISkill> skillInstance = nullptr;
    GameSounds::Cast(GetGameObject(), skillId);

    if (skillId == (int)SkillType::GeneralAtk)
    {
        skillInstance = make_shared<GeneralAttack>();

        _currentState = (_lastAttackAnim == 1) ? PlayerState::ATK2 : PlayerState::ATK1;
        _lastAttackAnim = (_lastAttackAnim == 1) ? 2 : 1;
    }
    else
    {
        switch ((SkillType)skillId)
        {
        case SkillType::QSpell:
        {
            _currentState = PlayerState::Q;

            break;
        }
        case SkillType::WSpell:
        {
            _currentState = PlayerState::W;
            break;
        }
        case SkillType::ESpell:
            _currentState = PlayerState::E;
            break;
        case SkillType::RSpell:
            _currentState = PlayerState::R;
            break;
        }

        skillInstance = CreateSkillInstance(skillId);
    }

    BeginActionAnimation();
    auto animator = GetGameObject()->GetModelAnimator();

    if (animator)
    {
        _isAttackMode = true;
        animator->SetAnimation((int32)_currentState, false);

        DEBUG_LOG("[Client] ▶ AnniePlayer Skill Started: " << skillId);
    }

    if (skillInstance && !_serverPresentationOnly)
    {
        skillInstance->Use(GetGameObject(), _target);
    }
}

void AnniePlayerController::PlayServerSkillResult(int32 skillId, const Vec3& castOrigin, const Vec3& castDirection)
{
    if (skillId != (int32)SkillType::WSpell)
        return;
    ConfirmSkillCooldown(skillId);

    if (_isAttackMode)
        return;

    _serverSkillCastOrigin = castOrigin;
    _serverSkillCastDirection = castDirection;
    direction = castDirection;
    direction.y = 0.f;

    if (direction.LengthSquared() <= 0.0001f)
        return;
    direction.Normalize();
    _target.reset();
    AlignToDirection(direction);
    ClearPendingSkillRequest();
    _serverPresentationOnly = true;
    ProcSkill(skillId);
    _serverPresentationOnly = false;
}

void AnniePlayerController::Update()
{
    Super::Update();

    if (_isAttackMode && TIME->GetGameTime() >= _timeToIdle)
    {
        FinishActionAnimation();

        uint64 casterId = _playerInfo->objectid();
        ClientPacketHandler::g_lastPlayedSkill.erase(casterId);
        DEBUG_LOG("[Client] ✅ AnniePlayer Skill Timeout → IDLE + Reset");
    }
}

shared_ptr<ISkill> AnniePlayerController::CreateSkillInstance(int32 skillId)
{
    switch ((SkillType)skillId)
    {
    case SkillType::QSpell:
        return make_shared<AnnieQSpell>();
    case SkillType::WSpell:
        return make_shared<AnnieWSpell>();
    case SkillType::ESpell:
        return make_shared<AnnieESpell>();
    case SkillType::RSpell:
        return make_shared<AnnieRSpell>();
    default:
        return nullptr;
    }
}

void AnniePlayerController::Awake() {}

void AnniePlayerController::Start()
{
    Super::Start();
}

void AnniePlayerController::LateUpdate() {}

void AnniePlayerController::FixedUpdate() {}

void AnniePlayerController::AlignToTarget()
{
    if (_target)
    {
        Vec3 dir = _target->GetTransform()->GetPosition() - GetTransform()->GetPosition();
        AlignToDirection(dir);
    }

    else
    {
        // 타겟이 없으면 현재 바라보는 방향 유지
        Vec3 dir = GetTransform()->GetLook();
        CalculateRotationFromDirection(dir);
    }
}

Vec3 AnniePlayerController::CalculateRotationFromDirection(const Vec3& dir)
{
    Vec3 forward = dir;

    forward.y = 0.f;

    forward.Normalize();

    float angleY = atan2f(forward.x, forward.z);

    return Vec3((0.f), XMConvertToRadians(angleY), 0.f);
}
