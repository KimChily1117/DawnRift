using Server.Game.Room;
using System;
using System.Collections.Generic;
using System.Text;
using Server.Game.Objects;
using Google.Protobuf.Protocol;

enum SkillType
{
    Gerneral,
    QSpell,
    WSpell,
    ESpell,
    RSpell
}

public interface IChampionSkillHandler
{
    bool HandleSkill(GameRoom room, GameObject caster, C_SkillCast skillPacket);
}
