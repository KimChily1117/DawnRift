#include "pch.h"
#include "AnnieOtherPlayerController.h"
#include "ModelAnimator.h"
#include "ISkill.h"
#include "TimeManager.h"
#include "ClientPacketHandler.h"

void AnnieOtherPlayerController::ProcSkill(int32 skillId)
{
    if (skillId != (int)SkillType::WSpell)
        AlignToTarget();

    if (skillId == (int)SkillType::GeneralAtk)
    {
        _currentState = (_lastAttackAnim == 1) ? PlayerState::ATK2 : PlayerState::ATK1;
        _lastAttackAnim = (_lastAttackAnim == 1) ? 2 : 1;
    }
    else
    {
        switch ((SkillType)skillId)
        {
        case SkillType::QSpell:
            _currentState = PlayerState::Q;
            break;
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
    }

    BeginActionAnimation();
    auto animator = GetGameObject()->GetModelAnimator();

    if (animator)
    {
        _isAttackMode = true;
        animator->SetAnimation((int32)_currentState, false);
    }
}

void AnnieOtherPlayerController::Awake() {}

void AnnieOtherPlayerController::Start() {}

void AnnieOtherPlayerController::LateUpdate() {}

void AnnieOtherPlayerController::FixedUpdate() {}

void AnnieOtherPlayerController::Update()
{
    Super::Update();

    if (_isAttackMode)
    {
        if (TIME->GetGameTime() >= _timeToIdle)
        {
            FinishActionAnimation();
            uint64 casterId = _playerInfo->objectid();
            ClientPacketHandler::g_lastPlayedSkill.erase(casterId);
        }

        return;
    }

    if (HasAuthoritativeSnapshotStream() || IsDead())
        return;

    if (_hasTargetPosition)
    {
        if (_currentState != PlayerState::RUN)
        {
            _currentState = PlayerState::RUN;
            GetGameObject()->GetModelAnimator()->SetAnimation((int32)PlayerState::RUN, true);
        }
    }
    else if (_currentState == PlayerState::RUN)
    {
        _currentState = PlayerState::IDLE;
        GetGameObject()->GetModelAnimator()->SetAnimation((int32)PlayerState::IDLE, true);
    }
}

void AnnieOtherPlayerController::AlignToTarget()
{
    if (_target)
    {
        Vec3 myPosition = GetTransform()->GetPosition();
        Vec3 targetPosition = _target->GetTransform()->GetPosition();
        Vec3 direction = targetPosition - myPosition;
        AlignToDirection(direction);
    }
}

Vec3 AnnieOtherPlayerController::CalculateRotationFromDirection(const Vec3& dir)
{
    Vec3 forward = dir;

    forward.y = 0.f;

    forward.Normalize();

    float angleY = atan2f(forward.x, forward.z);

    return Vec3((0.f), XMConvertToRadians(angleY), 0.f);
}
