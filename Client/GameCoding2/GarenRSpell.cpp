// 가렌 R 스킬.
#include "pch.h"
#include "GarenRSpell.h"

void GarenRSpell::Use(shared_ptr<GameObject> caster, shared_ptr<GameObject> target)
{
    auto playerController = caster->GetScript<PlayerController>();

    if (!playerController || !target)
        return;

    Protocol::C_SkillCast skillPacket;
    skillPacket.set_casterid(GAMEMANAGER->_myPlayer->_playerInfo->objectid());
    skillPacket.set_skillid((int)SkillType::RSpell);

    skillPacket.set_targetid(target->GetScript<BasePlayerController>()->_playerInfo->objectid());
    skillPacket.mutable_targetpos()->set_x(target->GetTransform()->GetPosition().x);
    skillPacket.mutable_targetpos()->set_y(target->GetTransform()->GetPosition().y);
    skillPacket.mutable_targetpos()->set_z(target->GetTransform()->GetPosition().z);

    auto sendBuffer = ClientPacketHandler::MakeSendBuffer(skillPacket, C_SKILL_CAST);
    NETWORK->SendPacket(sendBuffer);
}
