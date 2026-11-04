using Google.Protobuf.Protocol;
using Server.Game.Objects;
using Server.Game.Room;

namespace Server.Game.Movement
{
    public static class CombatActionMovement
    {
        public static bool TryBegin(GameRoom room, GameObject caster, int skillId)
        {
            if (!(caster is Player player) || room == null || caster.Info == null || skillId < 0 || skillId > 4)
                return false;

            double? duration = ChampionRegistry.Find(caster.Info.ChampType)?.CastLockSeconds(skillId);

            if (duration == null)
                return false;

            if (!player.TryBeginCombatAction(skillId, duration.Value))
                return false;

            if (player.IsMovementLockedByCombat)
                room.CancelPlayerMovement(player);

            return true;
        }

        public static bool PlayPlaceholder(GameRoom room, GameObject caster, int skillId)
        {
            if (!TryBegin(room, caster, skillId))
                return false;
            room.Broadcast(new S_SkillResult { CasterId = caster.Info.ObjectId, SkillId = skillId });

            return true;
        }
    }
}
