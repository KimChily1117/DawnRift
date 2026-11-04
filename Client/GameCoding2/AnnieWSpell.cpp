// 애니 W 스킬.
#include "pch.h"
#include "AnnieWSpell.h"
#include "AnnieSkillTuning.h"

void AnnieWSpell::Use(shared_ptr<GameObject> caster, shared_ptr<GameObject> target)
{
    if (!caster)
        return;

    Vec3 targetPosition =
        caster->GetTransform()->GetPosition() + caster->GetTransform()->GetLook() * AnnieSkillTuning::WRange;

    if (target)
        targetPosition = target->GetTransform()->GetPosition();
    UseAt(caster, targetPosition);
}

void AnnieWSpell::UseAt(const shared_ptr<GameObject>& caster, const Vec3& targetPosition)
{
    auto playerController = caster ? caster->GetScript<PlayerController>() : nullptr;

    if (!playerController || playerController->IsActionBusy() || !std::isfinite(targetPosition.x) ||
        !std::isfinite(targetPosition.y) || !std::isfinite(targetPosition.z))
        return;

    playerController->HoldMovementForSkillRequest();
    Protocol::C_SkillCast skillPacket;
    skillPacket.set_casterid(GAMEMANAGER->_myPlayer->_playerInfo->objectid());
    skillPacket.set_skillid((int32)SkillType::WSpell);
    skillPacket.set_isareaskill(true);
    skillPacket.set_arearadius(AnnieSkillTuning::WRange);
    skillPacket.mutable_targetpos()->set_x(targetPosition.x);
    skillPacket.mutable_targetpos()->set_y(targetPosition.y);
    skillPacket.mutable_targetpos()->set_z(targetPosition.z);

    auto sendBuffer = ClientPacketHandler::MakeSendBuffer(skillPacket, C_SKILL_CAST);
    NETWORK->SendPacket(sendBuffer);
}
