#include "pch.h"
#include "OtherPlayerController.h"
#include "GameObject.h"
#include "Transform.h"
#include "Time.h"
#include "ModelAnimator.h"
#include "ClientPacketHandler.h"

void OtherPlayerController::Awake() {}

void OtherPlayerController::StopMovementForAction()
{
    BasePlayerController::StopMovementForAction();
    _hasTargetPosition = false;
    _targetPosition = GetTransform()->GetPosition();
}

void OtherPlayerController::Update()
{
    BasePlayerController::Update();

    if (HasAuthoritativeSnapshotStream())
    {
        _hasTargetPosition = false;

        return;
    }

    if (IsMovementLocked() || !_hasTargetPosition)
        return;

    Vec3 currentPosition = GetTransform()->GetPosition();
    direction = _targetPosition - currentPosition;
    direction.y = 0.0f;

    float distance = direction.Length();

    uint64 objectId = _playerInfo->objectid();

    bool isESkill = false;

    // ✅ 현재 플레이어가 E 스킬 중이면 Run 애니메이션을 무시
    auto it = ClientPacketHandler::g_lastPlayedSkill.find(objectId);

    if (it != ClientPacketHandler::g_lastPlayedSkill.end())
    {
        if (it->second == (int)SkillType::ESpell)
            isESkill = true;
    }

    if (distance < 0.05f)
    {
        _hasTargetPosition = false;
        GetTransform()->SetPosition(Vec3(_targetPosition.x, 1.6f, _targetPosition.z));

        if (!_isAttackMode)
        {
            _currentState = PlayerState::IDLE;
            GetGameObject()->GetModelAnimator()->GetTweenDesc().curr.animIndex = (int32)_currentState;
        }
    }
    else
    {
        direction.Normalize();
        const float speed = _playerInfo ? GetChampionData(_playerInfo->champtype()).MoveSpeedUnits() : _speed;
        Vec3 newPosition = currentPosition + direction * speed * TIME->GetDeltaTime();
        newPosition.y = 1.6f;

        float angle = atan2f(direction.x, direction.z) + XM_PI;
        GetTransform()->SetRotation(Vec3(XMConvertToRadians(90.f), angle, 0.0f));

        GetTransform()->SetPosition(newPosition);

        // ✅ E 스킬 중엔 애니메이션 변경 X
        if (!_isAttackMode)
        {
            _currentState = PlayerState::RUN;
            GetGameObject()->GetModelAnimator()->GetTweenDesc().curr.animIndex = (int32)_currentState;
        }
    }
}

void OtherPlayerController::SetTargetPosition(const Vec3& position)
{
    if (IsMovementLocked())
        return;
    _targetPosition = position;
    _hasTargetPosition = true;
}

void OtherPlayerController::ProcSkill(int32 skillId)
{
    // 필요 시 추가 구현
}
