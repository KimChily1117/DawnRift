#include "pch.h"
#include "NpcController.h"
#include "ModelAnimator.h"
#include "MapUnits.h"

void NpcController::Update()
{
    Super::Update();

    if (_isAttackMode && TIME->GetGameTime() >= _timeToIdle)
        FinishActionAnimation();
}

void NpcController::ProcSkill(int32 skillId)
{
    // Map units only have basic attacks.
    if (skillId != (int32)SkillType::GeneralAtk || IsDead())
        return;
    _currentState = PlayerState::ATK1;

    const MapUnitInfo* unit = _playerInfo ? FindMapUnit(_playerInfo->champtype()) : nullptr;

    if (_target && !(unit && unit->bar == MapUnitBar::Structure))
        AlignToDirection(_target->GetTransform()->GetPosition() - GetTransform()->GetPosition());
    BeginActionAnimation();

    if (auto animator = GetGameObject()->GetModelAnimator())
        animator->SetAnimation((int32)_currentState, false);
}
