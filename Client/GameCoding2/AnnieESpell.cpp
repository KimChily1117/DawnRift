// 애니 E 스킬.
#include "pch.h"
#include "AnnieESpell.h"

void AnnieESpell::Use(shared_ptr<GameObject> caster, shared_ptr<GameObject> target)
{
    auto playerController = caster->GetScript<PlayerController>();

    if (!playerController)
        return;
    const Vec3 castPosition = target ? target->GetTransform()->GetPosition() : caster->GetTransform()->GetPosition();

    // Animation 처리 부분은 부모인  Champ클래스에서 처리함

    // ✅ 서버에 공격 패킷 전송
    Protocol::C_SkillCast skillPacket;
    skillPacket.set_casterid(GAMEMANAGER->_myPlayer->_playerInfo->objectid());
    skillPacket.set_skillid(3);
    skillPacket.mutable_targetpos()->set_x(castPosition.x);
    skillPacket.mutable_targetpos()->set_y(castPosition.y);
    skillPacket.mutable_targetpos()->set_z(castPosition.z);

    if (target)
        if (auto ally = target->GetScript<BasePlayerController>();
            ally && ally->_playerInfo && ally->_playerInfo->champtype() != Protocol::PLAYER_TYPE_TIBBERS &&
            ally->_playerInfo->teamid() == GAMEMANAGER->_myPlayer->_playerInfo->teamid())
            skillPacket.set_targetid(ally->_playerInfo->objectid());

    auto sendBuffer = ClientPacketHandler::MakeSendBuffer(skillPacket, C_SKILL_CAST);
    NETWORK->SendPacket(sendBuffer);
}
