// 가렌 Q 스킬.
#include "pch.h"
#include "GarenQSpell.h"

void GarenQSpell::Use(shared_ptr<GameObject> caster, shared_ptr<GameObject> target)
{
    auto playerController = caster->GetScript<PlayerController>();

    if (!playerController || !target)
        return;

    Protocol::C_SkillCast skillPacket;
    skillPacket.set_casterid(GAMEMANAGER->_myPlayer->_playerInfo->objectid());
    skillPacket.set_skillid((int)SkillType::QSpell);
    skillPacket.set_targetid(target->GetScript<BasePlayerController>()->_playerInfo->objectid());
    skillPacket.mutable_targetpos()->set_x(target->GetTransform()->GetPosition().x);
    skillPacket.mutable_targetpos()->set_y(target->GetTransform()->GetPosition().y);
    skillPacket.mutable_targetpos()->set_z(target->GetTransform()->GetPosition().z);

    auto sendBuffer = ClientPacketHandler::MakeSendBuffer(skillPacket, C_SKILL_CAST);
    NETWORK->SendPacket(sendBuffer);
}
