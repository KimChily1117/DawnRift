#include "pch.h"
#include "KitPlayerController.h"
#include "GameSounds.h"
#include "GeneralAttack.h"
#include "ModelAnimator.h"
#include "ClientPacketHandler.h"
#include "UnitStatus.h"
#include "KatarinaDaggers.h"

namespace
{
void FaceTarget(BasePlayerController& controller)
{
    if (!controller._target)
        return;
    const Vec3 direction = controller._target->GetTransform()->GetPosition() - controller.GetTransform()->GetPosition();
    controller.FaceDirection(direction);
}

void PlayBasicAttack(BasePlayerController& controller)
{
    controller._currentState = controller._lastAttackAnim == 1 ? PlayerState::ATK2 : PlayerState::ATK1;
    controller._lastAttackAnim = controller._lastAttackAnim == 1 ? 2 : 1;
    FaceTarget(controller);
    controller.StartActionAnimation();

    if (auto animator = controller.GetGameObject()->GetModelAnimator())
    {
        animator->SetAnimation((int32)controller._currentState, false);
        animator->SetAnimationEndCallback([&controller]() { controller.EndActionAnimation(); });
    }
}
}

void KitPlayerController::ProcSkill(int32 skillId)
{
    if (skillId == (int32)SkillType::GeneralAtk)
    {
        if (IsActionBusy())
            return;
        // Graves reloading: the server would refuse the shot.
        if (_playerInfo->champtype() == Protocol::PLAYER_TYPE_GRAVES)
            if (const UnitStatus::State* state = UnitStatus::Find(_playerInfo->objectid());
                state && state->gravesShells == 0 && state->gravesReloadEndsAt > TIME->GetGameTime())
                return;
        GameSounds::Cast(GetGameObject(), skillId);
        PlayBasicAttack(*this);
        make_shared<GeneralAttack>()->Use(GetGameObject(), _target);

        return;
    }

    PlayKitSkillAnimation(skillId);
}

void KitPlayerController::Update()
{
    Super::Update();
    UpdateKitChannel();

    if (_playerInfo && _playerInfo->champtype() == Protocol::PLAYER_TYPE_KATARINA)
        KatarinaDaggers::Update();

    if (_isAttackMode && !GetGameObject()->GetModelAnimator() && TIME->GetGameTime() >= _timeToIdle)
        EndActionAnimation();
}

void KitOtherPlayerController::ProcSkill(int32 skillId)
{
    if (skillId == (int32)SkillType::GeneralAtk)
    {
        PlayBasicAttack(*this);

        return;
    }

    PlayKitSkillAnimation(skillId);
}

void KitOtherPlayerController::Update()
{
    Super::Update();
    UpdateKitChannel();

    if (_playerInfo && _playerInfo->champtype() == Protocol::PLAYER_TYPE_KATARINA)
        KatarinaDaggers::Update();

    if (_isAttackMode)
    {
        if (TIME->GetGameTime() >= _timeToIdle)
            EndActionAnimation();

        return;
    }

    if (HasAuthoritativeSnapshotStream() || IsDead())
        return;
    auto animator = GetGameObject()->GetModelAnimator();

    if (!animator)
        return;

    if (_hasTargetPosition && _currentState != PlayerState::RUN)
    {
        _currentState = PlayerState::RUN;
        animator->SetAnimation((int32)PlayerState::RUN, true);
    }
    else if (!_hasTargetPosition && _currentState == PlayerState::RUN)
    {
        _currentState = PlayerState::IDLE;
        animator->SetAnimation((int32)PlayerState::IDLE, true);
    }
}
