#include "pch.h"
#include "GarenOtherPlayerController.h"
#include "ModelAnimator.h"
#include "ISkill.h"
#include "TimeManager.h"
#include "ModelRenderer.h"
#include "ClientPacketHandler.h"

void GarenOtherPlayerController::Awake()
{
    Super::Awake();
}

void GarenOtherPlayerController::Start()
{
    Super::Start();
}

void GarenOtherPlayerController::LateUpdate()
{
    Super::LateUpdate();
}

void GarenOtherPlayerController::FixedUpdate()
{
    Super::FixedUpdate();
}

void GarenOtherPlayerController::Update()
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

void GarenOtherPlayerController::ProcSkill(int32 skillId)
{
    if (skillId == (int)SkillType::WSpell)
        return;

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
        case SkillType::ESpell:
            _currentState = PlayerState::E;

            break;
        case SkillType::RSpell:
            _currentState = PlayerState::R;
            break;
        }
    }

    AlignToTarget();

    BeginActionAnimation();
    auto animator = GetGameObject()->GetModelAnimator();

    if (animator)
    {
        _isAttackMode = true;
        animator->SetAnimation((int32)_currentState, false);
    }
}

void GarenOtherPlayerController::AlignToTarget()
{
    if (_target)
    {
        Vec3 myPosition = GetTransform()->GetPosition();
        Vec3 targetPosition = _target->GetTransform()->GetPosition();
        Vec3 direction = targetPosition - myPosition;
        AlignToDirection(direction);
    }
}
