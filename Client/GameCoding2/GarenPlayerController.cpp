// 가렌 조작과 서버 입력 전송.
#include "pch.h"
#include "GameSounds.h"
#include "GarenPlayerController.h"
#include "ISkill.h"
#include "GeneralAttack.h"
#include "ModelAnimator.h"
#include "TimeManager.h"
#include "GarenQSpell.h"
#include "GarenESpell.h"
#include "GarenRSpell.h"
#include "ClientPacketHandler.h"
#include "ModelRenderer.h"

void GarenPlayerController::ProcSkill(int32 skillId)
{
    if (skillId == (int)SkillType::WSpell)
    {
        Protocol::C_SkillCast packet;
        packet.set_casterid(_playerInfo->objectid());
        packet.set_skillid(skillId);
        NETWORK->SendPacket(ClientPacketHandler::MakeSendBuffer(packet, C_SKILL_CAST));

        return;
    }

    if (IsActionBusy())
        return;

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

        case SkillType::ESpell:
        {
            _currentState = PlayerState::E;

            break;
        }
        case SkillType::RSpell:
        {
            _currentState = PlayerState::R;
            break;
        }
        }

        skillInstance = CreateSkillInstance(skillId);
    }

    AlignToTarget();

    BeginActionAnimation();
    auto animator = GetGameObject()->GetModelAnimator();

    if (animator)
    {
        _isAttackMode = true;
        animator->SetAnimation((int32)_currentState, false);

        animator->SetAnimationEndCallback(
            [this]()
            {
                FinishActionAnimation();

                uint64 casterId = _playerInfo->objectid();
                ClientPacketHandler::g_lastPlayedSkill.erase(casterId);
            });
    }

    if (skillInstance)
        skillInstance->Use(GetGameObject(), _target);
}

void GarenPlayerController::Awake() {}

void GarenPlayerController::Start()
{
    Super::Start();
}

void GarenPlayerController::LateUpdate() {}

void GarenPlayerController::FixedUpdate() {}

void GarenPlayerController::Update()
{
    Super::Update();

    if (_isAttackMode && !GetGameObject()->GetModelAnimator() && TIME->GetGameTime() >= _timeToIdle)
        FinishActionAnimation();
}

void GarenPlayerController::AlignToTarget()
{
    if (_target)
    {
        Vec3 myPosition = GetTransform()->GetPosition();
        Vec3 targetPosition = _target->GetTransform()->GetPosition();
        Vec3 direction = targetPosition - myPosition;
        AlignToDirection(direction);
    }
}

shared_ptr<ISkill> GarenPlayerController::CreateSkillInstance(int32 skillId)
{
    switch ((SkillType)skillId)
    {
    case SkillType::QSpell:
        return make_shared<GarenQSpell>();
    case SkillType::ESpell:
        return make_shared<GarenESpell>();
    case SkillType::RSpell:
        return make_shared<GarenRSpell>();
    default:
        return nullptr;
    }
}
