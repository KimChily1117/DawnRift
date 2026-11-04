using Google.Protobuf.Protocol;
using Server.Game.Objects;
using Server.Game.Room;
using System;
using Vec3 = System.Numerics.Vector3;

public static class Practice
{
    public const int GoldPerUse = 10000;

    public static bool Apply(GameRoom room, Player player, PRACTICE_ACTION action, Vec3 position)
    {
        if (room == null || player?.Info == null || !room.Practice)
            return false;

        switch (action)
        {
        case PRACTICE_ACTION.PracticeResetCooldowns:
            for (int skill = 1; skill <= 4; ++skill)
            {
                player.StartSkillCooldown(skill, 0.0);
                player.Session?.Send(new S_SkillCooldown { SkillId = skill, Duration = 0f });
            }

            foreach (SummonerSlot slot in player.SummonerSlots)
                if (slot != null)
                {
                    slot.ReadyAt = 0;
                    slot.NextChargeAt = 0;

                    if (slot.Spell == SUMMONER_SPELL.SummonerSmite)
                        slot.Charges = 2;
                }
            SummonerSpells.SendCooldowns(room, player);
            player.Trinket.Charges = Trinket.MaxCharges;
            player.Trinket.NextChargeAt = 0;
            Trinket.Send(player);

            foreach (var item in player.Items.Slots)
                item.CooldownEndsAt = 0;
            player.Items.Dirty = true;

            return true;
        case PRACTICE_ACTION.PracticeAddGold:
            Progression.GrantGold(player, GoldPerUse);
            Progression.SendStats(room, player);

            return true;
        case PRACTICE_ACTION.PracticeLevelUp:
            Progression.GrantXp(room, player, Progression.XpToNext(player.Progress.Level));
            Progression.SendStats(room, player);

            return true;
        case PRACTICE_ACTION.PracticeMaxLevel:
            while (player.Progress.Level < Progression.MaxLevel)
                Progression.GrantXp(room, player, Progression.XpToNext(player.Progress.Level));
            Progression.SendStats(room, player);

            return true;
        case PRACTICE_ACTION.PracticeHeal:
            if (player.Info.Hp <= 0)
                return false;
            player.Info.Hp = player.Info.MaxHp;
            player.Progress.Mana = player.Progress.MaxMana;
            player.Info.Mp = (int)player.Progress.Mana;
            player.Progress.Dirty = true;
            Buffs.SendHp(room, player);
            Progression.SendStats(room, player);

            return true;
        case PRACTICE_ACTION.PracticeTeleport: {
            if (player.Info.Hp <= 0)
                return false;
            Vec3 spot = MapUnit.Walkable(room, new Vec3(position.X, player.Info.Position.Y, position.Z));
            player.CancelBasicAttackWindup();
            room.TeleportPlayer(player, spot);
            room.Broadcast(
                new S_Recall { TargetId = player.Info.ObjectId, State = 1, Position = player.Info.Position });

            return true;
        }
        case PRACTICE_ACTION.PracticeSpawnDummy: {
            if (room.Dummies.Count >= 8)
                return false;
            Vec3 spot = new Vec3(position.X, player.Info.Position.Y, position.Z);

            if (Vec3.Distance(spot, player.Info.Position.ToNumericsVector3()) > 20f)
                spot = player.Info.Position.ToNumericsVector3() + new Vec3(3f, 0f, 0f);
            TrainingDummy dummy = TrainingDummy.Create(room, 3 - player.Info.TeamId, spot);
            dummy.Spawn();
            room.Dummies.Add(dummy);

            return true;
        }
        case PRACTICE_ACTION.PracticeClearDummies:
            foreach (TrainingDummy dummy in room.Dummies.ToArray())
                dummy.Despawn();
            room.Dummies.Clear();

            return true;
        default:
            return false;
        }
    }
}
