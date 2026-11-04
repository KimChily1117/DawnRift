using Google.Protobuf.Protocol;
using Server.Game.Objects;
using Server.Game.Room;
using System;

public static class Recall
{
    public static double Seconds =
        double.TryParse(Environment.GetEnvironmentVariable("DAWN_RECALL_SECONDS"), out double s) ? s : 8.0;

    public static bool IsRecalling(Player player) =>
        player != null && player.RecallEndsAt != 0 && StatusEffects.Now < player.RecallEndsAt;

    public static bool Start(GameRoom room, Player player)
    {
        if (room == null || player?.Info == null || !Progression.IsChampion(player) || player.Info.Hp <= 0)
            return false;

        if (IsRecalling(player) || player.Status.IsDisabled || player.Status.InStasis ||
            player.IsPerformingCombatAction)
            return false;
        room.CancelPlayerMovement(player);
        player.CancelBasicAttackWindup();
        player.RecallEndsAt = StatusEffects.After(Seconds);
        room.Broadcast(new S_Recall { TargetId = player.Info.ObjectId, State = 0, Duration = (float)Seconds,
                                      Position = player.Info.Position });
        Buffs.State(room, player, BUFF_TYPE.BuffRecall, Buffs.PassiveSkillId, 0f, Seconds);

        return true;
    }

    public static void Cancel(GameRoom room, Player player)
    {
        if (player?.Info == null || player.RecallEndsAt == 0)
            return;
        player.RecallEndsAt = 0;
        room?.Broadcast(new S_Recall { TargetId = player.Info.ObjectId, State = 2, Position = player.Info.Position });

        if (room != null)
            Buffs.State(room, player, BUFF_TYPE.BuffRecall, Buffs.PassiveSkillId, 0f, 0.0);
    }

    // Per Room tick: the recall completes at the fountain.
    public static void Update(GameRoom room, Player player)
    {
        if (player?.Info == null || player.RecallEndsAt == 0 || StatusEffects.Now < player.RecallEndsAt)
            return;
        player.RecallEndsAt = 0;

        if (player.Info.Hp <= 0)
            return;
        room.TeleportPlayer(player, player.Progress.SpawnPosition);
        player.Info.Hp = player.Info.MaxHp;
        player.Progress.Mana = player.Progress.MaxMana;
        player.Info.Mp = (int)player.Progress.Mana;
        player.Progress.Dirty = true;
        room.Broadcast(new S_Recall { TargetId = player.Info.ObjectId, State = 1, Position = player.Info.Position });
        Buffs.State(room, player, BUFF_TYPE.BuffRecall, Buffs.PassiveSkillId, 0f, 0.0);
        Buffs.SendHp(room, player);
        Console.WriteLine($"[Recall] {player.Info.ObjectId} arrived at the fountain.");
    }
}
