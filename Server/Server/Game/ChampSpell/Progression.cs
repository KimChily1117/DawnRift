using Google.Protobuf.Protocol;
using Server.Game.Objects;
using Server.Game.Room;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using Vec3 = System.Numerics.Vector3;

public sealed class Progress
{
    public int Level = 1;
    public readonly int[] Ranks = new int[4];
    public float Xp;
    public float Gold;
    public float Mana, MaxMana;
    public long JoinedAt;
    public long RespawnAt;
    public Vec3 SpawnPosition;
    internal float HpRegenCarry, ManaRegenCarry, GoldCarry;
    internal bool Dirty;
    internal float SyncTimer;

    public bool IsDead => RespawnAt != 0;
}

public static class Progression
{
    public const int MaxLevel = 18;

    public static readonly int StartingGold =
        int.TryParse(Environment.GetEnvironmentVariable("DAWN_GOLD"), out int startingGold) &&
        startingGold > 0? startingGold : 500;
    public const int KillGold = 300;
    public const float PassiveGoldPerSecond = 2.04f;
    public const double PassiveGoldDelaySeconds = 65.0;
    private const float SyncIntervalSeconds = 0.25f;

    // Experience for killing a champion of each level.
    private static readonly int[] KillXp = { 42,  114, 144, 174, 204, 234, 308, 392, 486,
                                             590, 640, 690, 740, 790, 840, 890, 940, 990 };
    // Base respawn wait by level, in seconds.
    private static readonly float[] DeathSeconds = { 10f,   10f, 12f,   12f, 14f,   16f, 20f,   25f, 28f,
                                                     32.5f, 35f, 37.5f, 40f, 42.5f, 45f, 47.5f, 50f, 52.5f };

    public static int XpToNext(int level) => level >= MaxLevel ? 0 : 180 + 100 * level;
    public static int KillXpFor(int victimLevel) => KillXp[Math.Clamp(victimLevel, 1, MaxLevel) - 1];
    public static float DeathSecondsAt(int level) => DeathSeconds[Math.Clamp(level, 1, MaxLevel) - 1];

    internal static int SkillRank(int level, SkillType slot)
    {
        if (slot == SkillType.RSpell)
            return 1 + (level >= 11 ? 1 : 0) + (level >= 16 ? 1 : 0);

        return Math.Min(5, (level + 1) / 2);
    }

    public static bool AutoSkillRanks { get; set; } = Environment.GetEnvironmentVariable("DAWN_AUTO_SKILLS") == "1";

    internal static int SkillRank(GameObject unit, SkillType slot)
    {
        if (!AutoSkillRanks && unit is Player player && IsChampion(player) && slot >= SkillType.QSpell &&
            slot <= SkillType.RSpell)
            return player.Progress.Ranks[(int)slot - 1];

        return SkillRank(CombatMath.LevelOf(unit), slot);
    }

    internal static int MaxSkillRank(int level, SkillType slot)
    {
        if (slot == SkillType.RSpell)
            return (level >= 6 ? 1 : 0) + (level >= 11 ? 1 : 0) + (level >= 16 ? 1 : 0);

        return Math.Min(5, (level + 1) / 2);
    }

    public static int SkillPoints(Player player)
    {
        if (AutoSkillRanks || !IsChampion(player))
            return 0;
        int spent = 0;

        foreach (int rank in player.Progress.Ranks)
            spent += rank;

        return Math.Max(0, player.Progress.Level - spent);
    }

    internal static bool LevelUpSkill(Player player, SkillType slot)
    {
        if (slot < SkillType.QSpell || slot > SkillType.RSpell || SkillPoints(player) <= 0)
            return false;
        int index = (int)slot - 1;

        if (player.Progress.Ranks[index] >= MaxSkillRank(player.Progress.Level, slot))
            return false;
        ++player.Progress.Ranks[index];
        player.Progress.Dirty = true;
        player.Progress.SyncTimer = 0f;

        return true;
    }

    public static bool IsChampion(GameObject unit) => unit is Player && !(unit is Tibbers) && !(unit is MapUnit);

    public static void Init(Player player, Vec3 spawnPosition)
    {
        Progress progress = player.Progress;
        progress.Level = 1;
        progress.Xp = 0f;
        Array.Clear(progress.Ranks, 0, progress.Ranks.Length);
        progress.Gold = StartingGold;
        progress.JoinedAt = StatusEffects.Now;
        progress.SpawnPosition = spawnPosition;
        ApplyLevelStats(player);
        progress.Mana = progress.MaxMana;
        player.Info.Hp = player.Info.MaxHp;
        player.Info.Mp = (int)progress.Mana;
    }

    public static void ApplyLevelStats(Player player)
    {
        ChampionData data = CombatMath.DataOf(player);

        if (data == null || player.Info == null)
            return;
        Progress progress = player.Progress;
        int level = progress.Level;
        Server.Game.Items.ItemStats items = player.ItemStats;
        int maxHp = (int)Math.Round(data.Hp.AtLevel(level) + items.Hp);

        if (player.Info.MaxHp > 0 && player.Info.Hp > 0)
            player.Info.Hp = Math.Min(maxHp, player.Info.Hp + Math.Max(0, maxHp - player.Info.MaxHp));
        player.Info.MaxHp = maxHp;
        player.Info.Hp = Math.Min(player.Info.Hp, maxHp);
        float maxMana = data.Mana.AtLevel(level) + (data.Mana.AtLevel(level) > 0f ? items.Mana : 0f);
        progress.Mana = Math.Min(maxMana, progress.Mana + Math.Max(0f, maxMana - progress.MaxMana));
        progress.Mana = Math.Min(progress.Mana, maxMana);
        progress.MaxMana = maxMana;
        player.Info.Level = level;
        player.Info.Mp = (int)progress.Mana;
        player.Info.MaxMp = (int)Math.Round(maxMana);
        player.Info.Attack = (int)Math.Round(CombatMath.AttackDamage(player));
        player.Info.Defence = (int)Math.Round(data.Armor.AtLevel(level));
        progress.Dirty = true;
    }

    public static void GrantXp(GameRoom room, Player player, float xp)
    {
        Progress progress = player.Progress;

        if (progress.Level >= MaxLevel || xp <= 0f)
            return;
        progress.Xp += xp;
        bool leveled = false;

        while (progress.Level < MaxLevel && progress.Xp >= XpToNext(progress.Level))
        {
            progress.Xp -= XpToNext(progress.Level);
            progress.Level++;
            leveled = true;
        }

        if (progress.Level >= MaxLevel)
            progress.Xp = 0f;

        if (leveled)
        {
            ApplyLevelStats(player);
            Server.Game.Items.ItemEffects.OnLevelUp(room, player);
            Console.WriteLine($"[Progress] {player.Info.ObjectId} reached level {progress.Level}.");
        }

        progress.Dirty = true;
    }

    public const int FirstBloodBonus = 100;

    public static void GrantGold(Player player, float gold)
    {
        player.Progress.Gold += gold;
        player.Progress.Dirty = true;

        if (gold > 0f && player.Room != null && player.Info != null && IsChampion(player))
            player.Room.Scoreboard.Of(player.Info.ObjectId).GoldEarned += (int)gold;
    }

    public static bool TrySpendMana(Player player, float cost)
    {
        if (cost <= 0f)
            return true;

        if (player.Progress.Mana + 0.001f < cost)
            return false;
        player.Progress.Mana = Math.Max(0f, player.Progress.Mana - cost);
        player.Info.Mp = (int)player.Progress.Mana;
        player.Progress.Dirty = true;

        return true;
    }

    public static float OnChampionDeath(GameRoom room, Player victim, GameObject source, out ulong killerId)
    {
        killerId = 0;
        Player killer = source as Player;

        if (killer is Tibbers && killer.Info != null)
            killer = room.FindObject(killer.Info.OwnerId) as Player;

        if (killer?.Info != null && IsChampion(killer) && killer.Info.TeamId != victim.Info.TeamId)
        {
            killerId = killer.Info.ObjectId;
            GrantGold(killer, KillGold);

            if (!room.Scoreboard.FirstBloodTaken)
            {
                room.Scoreboard.FirstBloodTaken = true;
                GrantGold(killer, FirstBloodBonus);
            }

            GrantXp(room, killer, KillXpFor(victim.Progress.Level));
            Console.WriteLine($"[Progress] {killer.Info.ObjectId} killed {victim.Info.ObjectId}: +{KillGold} gold, " +
                              $"+{KillXpFor(victim.Progress.Level)} xp.");
        }

        Recall.Cancel(room, victim);
        victim.Session?.Send(room.Scoreboard.BuildRecap(victim, killerId));
        List<ulong> takedown = room.Scoreboard.RecentAttackers(victim, ChampionKit.TakedownSeconds);

        if (killerId != 0 && !takedown.Contains(killerId))
            takedown.Add(killerId);
        room.Scoreboard.OnChampionKilled(victim, killerId);

        if (killerId != 0 && room.FindObject(killerId) is Player credited)
            ChampionRegistry.Find(credited.Info.ChampType)?.OnKill?.Invoke(room, credited, victim);

        foreach (ulong id in takedown)
            if (room.FindObject(id) is Player participant && participant.Info != null)
            {
                ChampionRegistry.Find(participant.Info.ChampType)?.OnTakedown?.Invoke(room, participant, victim);
                Server.Game.Items.ItemEffects.OnTakedown(room, participant, victim, id == killerId);
            }
        Server.Game.Items.ItemEffects.OnDeath(room, victim);
        room.Announcer.OnChampionKilled(victim, killerId, source);
        float seconds = room.Demo?.RespawnSeconds(victim) ?? DeathSecondsAt(victim.Progress.Level);
        victim.Progress.RespawnAt = StatusEffects.After(seconds);
        victim.Progress.HpRegenCarry = victim.Progress.ManaRegenCarry = 0f;
        victim.Progress.Dirty = true;

        return seconds;
    }

    public static void Update(GameRoom room, Player player, float deltaTime)
    {
        if (!IsChampion(player) || player.Info == null)
            return;
        Progress progress = player.Progress;

        if (progress.IsDead)
        {
            if (StatusEffects.Now >= progress.RespawnAt)
                Respawn(room, player);
        }
        else
        {
            Regenerate(player, deltaTime);
        }

        if (StatusEffects.Now - progress.JoinedAt >= (long)(PassiveGoldDelaySeconds * Stopwatch.Frequency))
        {
            progress.GoldCarry += PassiveGoldPerSecond * deltaTime;

            if (progress.GoldCarry >= 1f)
            {
                int gold = (int)progress.GoldCarry;
                progress.GoldCarry -= gold;
                GrantGold(player, gold);
            }
        }

        progress.SyncTimer -= deltaTime;

        if (progress.Dirty && progress.SyncTimer <= 0f)
            SendStats(room, player);
    }

    private static void Regenerate(Player player, float deltaTime)
    {
        ChampionData data = CombatMath.DataOf(player);

        if (data == null || player.Info.Hp <= 0)
            return;
        Progress progress = player.Progress;
        int level = progress.Level;

        if (player.Info.Hp < player.Info.MaxHp)
        {
            float regen = data.HpRegen.AtLevel(level) * Server.Game.Items.ItemEffects.HpRegenMultiplier(player);

            if (player.Room?.EpicBuffs != null)
                regen +=
                    (player.Info.MaxHp - player.Info.Hp) * player.Room.EpicBuffs.MissingHealthRegenPerSecond(player);
            progress.HpRegenCarry += regen * player.Status.HealingMultiplier * deltaTime;

            if (progress.HpRegenCarry >= 1f)
            {
                int heal = (int)progress.HpRegenCarry;
                progress.HpRegenCarry -= heal;
                player.Info.Hp = Math.Min(player.Info.MaxHp, player.Info.Hp + heal);
                progress.Dirty = true;
            }
        }
        else
        {
            progress.HpRegenCarry = 0f;
        }

        if (progress.Mana < progress.MaxMana)
        {
            int before = (int)progress.Mana;
            progress.Mana =
                Math.Min(progress.MaxMana,
                         progress.Mana + data.ManaRegen.AtLevel(level) *
                                             Server.Game.Items.ItemEffects.ManaRegenMultiplier(player) * deltaTime);
            player.Info.Mp = (int)progress.Mana;

            if ((int)progress.Mana != before)
                progress.Dirty = true;
        }
    }

    public static void Respawn(GameRoom room, Player player)
    {
        Progress progress = player.Progress;
        progress.RespawnAt = 0;
        player.Status.Clear();
        player.Info.Hp = player.Info.MaxHp;
        progress.Mana = progress.MaxMana;
        player.Info.Mp = (int)progress.Mana;
        player.Info.State = OBJECT_STATE_TYPE.Idle;
        room.TeleportPlayer(player, progress.SpawnPosition);
        room.Broadcast(new S_Respawn {
            TargetId = player.Info.ObjectId,
            Position = player.Info.Position,
            Hp = player.Info.Hp,
            MaxHp = player.Info.MaxHp,
            Mp = player.Info.Mp,
            MaxMp = player.Info.MaxMp,
        });
        progress.Dirty = true;
        Console.WriteLine($"[Progress] {player.Info.ObjectId} respawned.");
    }

    public static void SendStats(GameRoom room, Player player)
    {
        Progress progress = player.Progress;
        progress.Dirty = false;
        progress.SyncTimer = SyncIntervalSeconds;
        S_UnitStats stats = BuildStats(player, true);
        player.Session?.Send(stats);
        room.Broadcast(BuildStats(player, false), player.Info.ObjectId);
    }

    public static S_UnitStats BuildStats(Player player, bool owner)
    {
        Progress progress = player.Progress;
        var stats = new S_UnitStats {
            TargetId = player.Info.ObjectId, Level = progress.Level,  Hp = player.Info.Hp,
            MaxHp = player.Info.MaxHp,       Mp = (int)progress.Mana, MaxMp = (int)Math.Round(progress.MaxMana),
        };

        if (owner)
        {
            stats.Xp = (int)progress.Xp;
            stats.XpToNext = XpToNext(progress.Level);
            stats.Gold = (int)progress.Gold;
        }

        ChampionData data = CombatMath.DataOf(player);
        Server.Game.Items.ItemStats items = player.ItemStats;
        stats.AttackSpeed = Server.Game.Items.ItemEffects.AttackSpeedBonus(player.Room, player);

        if (owner && data != null)
        {
            int level = progress.Level;
            stats.AttackDamage = CombatMath.AttackDamage(player);
            stats.AbilityPower = CombatMath.AbilityPower(player);
            stats.Armor = data.Armor.AtLevel(level) + items.Armor;
            stats.MagicResist = data.MagicResist.AtLevel(level) + items.Mr;
            stats.MoveSpeed = player.EffectiveMovementSpeed / ChampionData.UnitScale;
            stats.CritChance = items.Crit;
            stats.AbilityHaste = Server.Game.Items.ItemEffects.Haste(player, 1);
            stats.Lethality = items.Lethality;
            stats.ArmorPenPercent = items.ArmorPen;
            stats.MagicPen = items.MagicPen;
            stats.MagicPenPercent = items.MagicPenPercent;
            stats.LifeSteal = items.LifeSteal;
            stats.Omnivamp = items.Omnivamp;
            stats.HealShieldPower = items.HealShieldPower;
            stats.Tenacity = items.Tenacity;
            stats.HpRegen = data.HpRegen.AtLevel(level) * 5f * Server.Game.Items.ItemEffects.HpRegenMultiplier(player);
            stats.ManaRegen =
                data.ManaRegen.AtLevel(level) * 5f * Server.Game.Items.ItemEffects.ManaRegenMultiplier(player);
        }

        foreach (SkillType slot in new[] { SkillType.QSpell, SkillType.WSpell, SkillType.ESpell, SkillType.RSpell })
            stats.SkillRanks.Add(SkillRank(player, slot));
        stats.SkillPoints = SkillPoints(player);

        return stats;
    }
}
