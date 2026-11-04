// 가렌 E 스킬.
#include "pch.h"
#include "GarenESpell.h"

void GarenESpell::Use(shared_ptr<GameObject> caster, shared_ptr<GameObject> target)
{
    auto playerController = caster->GetScript<PlayerController>();

    if (!playerController)
        return;

    // ✅ 서버로 스킬 시전 패킷 전송
    Protocol::C_SkillCast skillPacket;
    skillPacket.set_casterid(GAMEMANAGER->_myPlayer->_playerInfo->objectid());
    skillPacket.set_skillid((int)SkillType::ESpell);

    // 🔥 타겟 없음 (범위 판정은 서버에서)
    auto sendBuffer = ClientPacketHandler::MakeSendBuffer(skillPacket, C_SKILL_CAST);
    NETWORK->SendPacket(sendBuffer);
}
