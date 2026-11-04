// 애니 Q 스킬.
#include "pch.h"
#include "AnnieQSpell.h"

void AnnieQSpell::Use(shared_ptr<GameObject> caster, shared_ptr<GameObject> target)
{
    auto playerController = caster->GetScript<PlayerController>();

    if (!playerController || !target)
        return;

    // Animation 처리 부분은 부모인  Champ클래스에서 처리함

    // ✅ 서버에 공격 패킷 전송
    Protocol::C_SkillCast skillPacket;
    skillPacket.set_casterid(GAMEMANAGER->_myPlayer->_playerInfo->objectid());
    skillPacket.set_skillid(1);

    skillPacket.set_targetid(target->GetScript<BasePlayerController>()->_playerInfo->objectid());

    skillPacket.mutable_targetpos()->set_x(target->GetTransform()->GetPosition().x);
    skillPacket.mutable_targetpos()->set_y(target->GetTransform()->GetPosition().y);
    skillPacket.mutable_targetpos()->set_z(target->GetTransform()->GetPosition().z);

    auto sendBuffer = ClientPacketHandler::MakeSendBuffer(skillPacket, C_SKILL_CAST);
    NETWORK->SendPacket(sendBuffer);
}
