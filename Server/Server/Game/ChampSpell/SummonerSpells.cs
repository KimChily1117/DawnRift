using Google.Protobuf.Protocol;
using Server.Game.Map;
using Server.Game.Movement;
using Server.Game.Objects;
using Server.Game.Room;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json;
using Vec3 = System.Numerics.Vector3;

public sealed class SummonerSpellData
{
    public SUMMONER_SPELL Spell;
    public float Cooldown;
    public float CastRange;
    public float Distance;
    public float Damage;
    public int Charges;
    public float RechargeSeconds;
    public float DamageBase, DamagePerLevel, Duration, GrievousWounds;
    public float HealMin, HealMax, AllyRange, MoveSpeed, MoveSpeedDuration, RecentHealFactor, RecentHealSeconds;
    public float MoveSpeedMin, MoveSpeedMax;

    private static readonly Dictionary<string, SUMMONER_SPELL> Names = new Dictionary<string, SUMMONER_SPELL> {
        ["Flash"] = SUMMONER_SPELL.SummonerFlash,   ["Smite"] = SUMMONER_SPELL.SummonerSmite,
        ["Ignite"] = SUMMONER_SPELL.SummonerIgnite, ["Heal"] = SUMMONER_SPELL.SummonerHeal,
        ["Ghost"] = SUMMONER_SPELL.SummonerGhost,
    };

    private static readonly object Lock = new object();
    private static Dictionary<SUMMONER_SPELL, SummonerSpellData> _spells;

    public static void Load(string path)
    {
        using JsonDocument document = JsonDocument.Parse(File.ReadAllText(path));
        float unit =
            document.RootElement.TryGetProperty("unitScale", out JsonElement scale) ? scale.GetSingle() : 0.01f;
        var spells = new Dictionary<SUMMONER_SPELL, SummonerSpellData>();

        foreach (JsonElement s in document.RootElement.GetProperty("spells").EnumerateArray())
        {
            if (!Names.TryGetValue(s.GetProperty("name").GetString(), out SUMMONER_SPELL spell))
                continue;
            float F(string name) => s.TryGetProperty(name, out JsonElement v) ? v.GetSingle() : 0f;
            spells[spell] = new SummonerSpellData {
                Spell = spell,
                Cooldown = F("cooldown"),
                CastRange = F("castRange") * unit,
                Distance = F("distance") * unit,
                Damage = F("damage"),
                Charges = (int)F("charges"),
                RechargeSeconds = F("rechargeSeconds"),
                DamageBase = F("damageBase"),
                DamagePerLevel = F("damagePerLevel"),
                Duration = F("duration"),
                GrievousWounds = F("grievousWounds"),
                HealMin = F("healMin"),
                HealMax = F("healMax"),
                AllyRange = F("allyRange") * unit,
                MoveSpeed = F("moveSpeed"),
                MoveSpeedDuration = F("moveSpeedDuration"),
                RecentHealFactor = F("recentHealFactor"),
                RecentHealSeconds = F("recentHealSeconds"),
                MoveSpeedMin = F("moveSpeedMin"),
                MoveSpeedMax = F("moveSpeedMax"),
            };
        }

        lock (Lock) _spells = spells;
    }

    public static SummonerSpellData Get(SUMMONER_SPELL spell)
    {
        lock (Lock)
        {
            if (_spells == null)
                Load(Path.Combine(AppContext.BaseDirectory, "Content", "SummonerSpells.json"));

            return _spells.TryGetValue(spell, out SummonerSpellData data) ? data : null;
        }
    }

    public static float ByLevel(float min, float max, int level) => min +
                                                                    (max - min) * (Math.Clamp(level, 1, 18) - 1) / 17f;
}

public sealed class SummonerSlot
{
    public SUMMONER_SPELL Spell;
    public long ReadyAt;
    public double Duration;
    public int Charges;
    public long NextChargeAt;
}

public static class SummonerSpells
{
    public const int SlotCount = 2;
    public const int SkillIdBase = 5;
    private const double IgniteTickSeconds = 1.0;

    public static readonly SUMMONER_SPELL[] Defaults = { SUMMONER_SPELL.SummonerFlash, SUMMONER_SPELL.SummonerIgnite };

    public static void Assign(Player player, IReadOnlyList<SUMMONER_SPELL> chosen)
    {
        chosen ??= FromEnvironment();
        SUMMONER_SPELL[] spells = chosen != null && chosen.Count == SlotCount &&
                                          chosen.Distinct().Count() == SlotCount &&
                                          chosen.All(s => SummonerSpellData.Get(s) != null)
                                      ? chosen.ToArray()
                                      : Defaults;
        player.Info.SummonerSpells.Clear();

        for (int slot = 0; slot < SlotCount; ++slot)
        {
            SummonerSpellData data = SummonerSpellData.Get(spells[slot]);
            player.SummonerSlots[slot] =
                new SummonerSlot { Spell = spells[slot], Charges = Math.Max(0, data?.Charges ?? 0) };
            player.Info.SummonerSpells.Add(spells[slot]);
        }
    }

    private static SUMMONER_SPELL[] FromEnvironment()
    {
        string value = Environment.GetEnvironmentVariable("DAWN_SPELLS");

        if (string.IsNullOrWhiteSpace(value))
            return null;
        var names = new Dictionary<string, SUMMONER_SPELL>(StringComparer.OrdinalIgnoreCase) {
            ["flash"] = SUMMONER_SPELL.SummonerFlash,   ["smite"] = SUMMONER_SPELL.SummonerSmite,
            ["ignite"] = SUMMONER_SPELL.SummonerIgnite, ["heal"] = SUMMONER_SPELL.SummonerHeal,
            ["ghost"] = SUMMONER_SPELL.SummonerGhost,
        };
        SUMMONER_SPELL[] spells =
            value.Split(',')
                .Select(s =>
                            names.TryGetValue(s.Trim(), out SUMMONER_SPELL spell) ? spell : SUMMONER_SPELL.SummonerNone)
                .ToArray();

        return spells.Length == SlotCount && spells.All(s => s != SUMMONER_SPELL.SummonerNone) ? spells : null;
    }

    public static void SendCooldowns(GameRoom room, Player player)
    {
        for (int slot = 0; slot < SlotCount; ++slot)
            SendCooldown(player, slot);
    }

    private static double SecondsUntil(long at) => Math.Max(0.0, (at - StatusEffects.Now) /
                                                                     (double)System.Diagnostics.Stopwatch.Frequency);

    private static void SendCooldown(Player player, int slot)
    {
        SummonerSlot s = player.SummonerSlots[slot];

        if (s == null || player.Session == null)
            return;
        player.Session.Send(new S_SummonerCooldown {
            Slot = slot,
            Spell = s.Spell,
            Remaining = (float)SecondsUntil(s.ReadyAt),
            Duration = (float)s.Duration,
            Charges = s.Charges,
            Recharge = s.NextChargeAt != 0 ? (float)SecondsUntil(s.NextChargeAt) : 0f,
        });
    }

    private static void Reject(Player player, int slot, SKILL_REJECT_REASON reason) =>
        player.Session?.Send(new S_SkillRejected { SkillId = SkillIdBase + slot, Reason = reason });

    public static SKILL_REJECT_REASON Cast(GameRoom room, Player caster, int slot, ulong targetId, Vec3 targetPos)
    {
        SKILL_REJECT_REASON reason = TryCast(room, caster, slot, targetId, targetPos);

        if (reason != SKILL_REJECT_REASON.SkillRejectNone)
            Reject(caster, slot, reason);

        return reason;
    }

    private static SKILL_REJECT_REASON TryCast(GameRoom room, Player caster, int slot, ulong targetId, Vec3 targetPos)
    {
        if (slot < 0 || slot >= SlotCount || caster.SummonerSlots[slot] == null || caster.Info == null)
            return SKILL_REJECT_REASON.SkillRejectInvalid;

        if (caster.Info.Hp <= 0)
            return SKILL_REJECT_REASON.SkillRejectDead;

        if (caster.Status.IsStunned)
            return SKILL_REJECT_REASON.SkillRejectStunned;
        SummonerSlot s = caster.SummonerSlots[slot];
        SummonerSpellData data = SummonerSpellData.Get(s.Spell);

        if (data == null)
            return SKILL_REJECT_REASON.SkillRejectInvalid;
        RechargeSmite(caster, slot);

        if (StatusEffects.Now < s.ReadyAt || (data.Charges > 0 && s.Charges <= 0))
            return SKILL_REJECT_REASON.SkillRejectCooldown;

        bool cast = s.Spell switch {
            SUMMONER_SPELL.SummonerFlash => Flash(room, caster, data, targetPos),
            SUMMONER_SPELL.SummonerSmite => Smite(room, caster, data, room.FindObject(targetId)),
            SUMMONER_SPELL.SummonerIgnite => Ignite(room, caster, data, room.FindObject(targetId)),
            SUMMONER_SPELL.SummonerHeal => Heal(room, caster, data, targetPos),
            SUMMONER_SPELL.SummonerGhost => Ghost(room, caster, data),
            _ => false,
        };

        if (!cast)
            return SKILL_REJECT_REASON.SkillRejectInvalid;

        float summonerCooldown = data.Cooldown * 100f / (100f + Server.Game.Items.ItemEffects.SummonerHaste(caster));
        s.ReadyAt = StatusEffects.After(summonerCooldown);
        s.Duration = summonerCooldown;

        if (data.Charges > 0)
        {
            --s.Charges;

            if (s.NextChargeAt == 0)
                s.NextChargeAt = StatusEffects.After(data.RechargeSeconds);
        }

        SendCooldown(caster, slot);

        return SKILL_REJECT_REASON.SkillRejectNone;
    }

    private static Vec3 PositionOf(GameObject unit) => unit.Info.Position.ToNumericsVector3();

    private static float EdgeDistance(GameObject from, GameObject to)
    {
        Vec3 d = PositionOf(to) - PositionOf(from);
        d.Y = 0f;

        return d.Length() - MapUnit.RadiusOf(to);
    }

    private static void Announce(GameRoom room, Player caster, SUMMONER_SPELL spell, ulong targetId, Vec3 from, Vec3 to,
                                 int amount)
    {
        room.Broadcast(new S_SummonerSpell {
            CasterId = caster.Info.ObjectId,
            Spell = spell,
            TargetId = targetId,
            From = from.ToProtoVector3(),
            To = to.ToProtoVector3(),
            Amount = amount,
        });
    }

    public static bool FlashDestination(GameRoom room, Vec3 start, Vec3 cursor, float distance, out Vec3 landing)
    {
        landing = start;
        Vec3 direction = cursor - start;
        direction.Y = 0f;
        float length = direction.Length();
        Vec3 wanted = length > distance ? start + direction / length * distance : new Vec3(cursor.X, start.Y, cursor.Z);
        var navigation = room.Navigation;

        if (!navigation.TryWorldToCell(wanted.X, navigation.Origin.Y, wanted.Z, out var cell) ||
            !AuthoritativeMoveService.TryResolveDestination(navigation, cell, out var walkable, out bool adjusted))
            return false;

        if (!adjusted)
        {
            landing = new Vec3(wanted.X, start.Y, wanted.Z);

            return true;
        }

        if (!navigation.TryGetCellWorldCenter(walkable.X, walkable.Z, out Vec3 center))
            return false;
        landing = new Vec3(center.X, start.Y, center.Z);

        return true;
    }

    private static bool Flash(GameRoom room, Player caster, SummonerSpellData data, Vec3 cursor)
    {
        Vec3 start = PositionOf(caster);

        if (!FlashDestination(room, start, cursor, data.Distance, out Vec3 landing))
            return false;
        caster.CancelBasicAttackWindup();
        room.TeleportPlayer(caster, landing);
        Announce(room, caster, SUMMONER_SPELL.SummonerFlash, 0, start, landing, 0);

        return true;
    }

    public static bool CanSmite(Player caster, GameObject target) =>
        target?.Info != null && target.Info.Hp > 0 && target.Info.TeamId != caster.Info.TeamId &&
        (target is JungleMonster || target is Minion) &&
        !(caster.Room?.Vision?.IsHiddenFrom(caster.Info.TeamId, target.Info.ObjectId) ?? false);

    private static bool Smite(GameRoom room, Player caster, SummonerSpellData data, GameObject target)
    {
        if (!CanSmite(caster, target) || EdgeDistance(caster, target) > data.CastRange + MapUnit.ChampionRadius)
            return false;
        Announce(room, caster, SUMMONER_SPELL.SummonerSmite, target.Info.ObjectId, PositionOf(caster),
                 PositionOf(target), (int)data.Damage);
        CombatMath.ApplyDamage(room, caster, target, data.Damage, DamageTypes.True);

        return true;
    }

    private static bool Ignite(GameRoom room, Player caster, SummonerSpellData data, GameObject target)
    {
        if (!(target is Player victim) || !Progression.IsChampion(victim) || victim.Info.Hp <= 0 ||
            victim.Info.TeamId == caster.Info.TeamId ||
            room.Vision.IsHiddenFrom(caster.Info.TeamId, victim.Info.ObjectId) ||
            EdgeDistance(caster, victim) > data.CastRange + MapUnit.ChampionRadius)
            return false;
        float total = data.DamageBase + data.DamagePerLevel * (CombatMath.LevelOf(caster) - 1);
        int ticks = Math.Max(1, (int)Math.Round(data.Duration / IgniteTickSeconds));
        victim.Status.SetIgnite(caster.Info.ObjectId, total / ticks, ticks, IgniteTickSeconds, data.GrievousWounds);
        room.Broadcast(new S_Buff {
            TargetId = victim.Info.ObjectId,
            SourceId = caster.Info.ObjectId,
            BuffType = BUFF_TYPE.BuffIgnite,
            SkillId = Buffs.PassiveSkillId,
            Amount = data.GrievousWounds,
            Duration = data.Duration,
        });
        Announce(room, caster, SUMMONER_SPELL.SummonerIgnite, victim.Info.ObjectId, PositionOf(caster),
                 PositionOf(victim), (int)total);

        return true;
    }

    private static bool Heal(GameRoom room, Player caster, SummonerSpellData data, Vec3 cursor)
    {
        Player ally = room.Units.OfType<Player>()
                          .Where(p => p != caster && Progression.IsChampion(p) && p.Info.TeamId == caster.Info.TeamId &&
                                      p.Info.Hp > 0 &&
                                      Vec3.Distance(Flat(PositionOf(p)), Flat(PositionOf(caster))) <= data.AllyRange)
                          .OrderBy(p => Vec3.Distance(Flat(PositionOf(p)), Flat(cursor)))
                          .FirstOrDefault();
        float amount = SummonerSpellData.ByLevel(data.HealMin, data.HealMax, CombatMath.LevelOf(caster));
        HealOne(room, caster, caster, data, amount);
        int healed = ally != null ? HealOne(room, caster, ally, data, amount) : 0;
        Announce(room, caster, SUMMONER_SPELL.SummonerHeal, ally?.Info.ObjectId ?? 0, PositionOf(caster),
                 ally != null ? PositionOf(ally) : PositionOf(caster), healed);

        return true;
    }

    private static int HealOne(GameRoom room, Player caster, Player target, SummonerSpellData data, float amount)
    {
        if (StatusEffects.Now < target.Status.RecentHealUntil)
            amount *= data.RecentHealFactor;
        target.Status.RecentHealUntil = StatusEffects.After(data.RecentHealSeconds);
        int heal = Buffs.Heal(room, caster, target, amount, Buffs.PassiveSkillId);
        Buffs.MoveSpeed(room, caster, target, data.MoveSpeed, data.MoveSpeedDuration, Buffs.PassiveSkillId,
                        decays: true);

        return heal;
    }

    private static bool Ghost(GameRoom room, Player caster, SummonerSpellData data)
    {
        float bonus = SummonerSpellData.ByLevel(data.MoveSpeedMin, data.MoveSpeedMax, CombatMath.LevelOf(caster));
        caster.Status.AddMoveSpeed(bonus, data.Duration, false);
        room.Broadcast(new S_Buff {
            TargetId = caster.Info.ObjectId,
            SourceId = caster.Info.ObjectId,
            BuffType = BUFF_TYPE.BuffGhost,
            SkillId = Buffs.PassiveSkillId,
            Amount = bonus,
            Duration = data.Duration,
        });
        Announce(room, caster, SUMMONER_SPELL.SummonerGhost, 0, PositionOf(caster), PositionOf(caster), 0);

        return true;
    }

    private static Vec3 Flat(Vec3 v) => new Vec3(v.X, 0f, v.Z);

    // Smite charges come back one at a time.
    private static void RechargeSmite(Player player, int slot)
    {
        SummonerSlot s = player.SummonerSlots[slot];
        SummonerSpellData data = s != null ? SummonerSpellData.Get(s.Spell) : null;

        if (data == null || data.Charges <= 0 || s.NextChargeAt == 0 || StatusEffects.Now < s.NextChargeAt)
            return;
        ++s.Charges;
        s.NextChargeAt = s.Charges < data.Charges ? StatusEffects.After(data.RechargeSeconds) : 0;
        SendCooldown(player, slot);
    }

    // Per Room tick: Smite charges and Ignite's burn.
    public static void Update(GameRoom room, Player player)
    {
        for (int slot = 0; slot < SlotCount; ++slot)
            if (player.SummonerSlots[slot] != null)
                RechargeSmite(player, slot);

        StatusEffects status = player.Status;

        if (status.IgniteTicksLeft <= 0 || player.Info == null || StatusEffects.Now < status.IgniteNextTickAt)
            return;

        if (player.Info.Hp <= 0)
        {
            status.ClearIgnite();

            return;
        }

        --status.IgniteTicksLeft;
        status.IgniteNextTickAt += (long)(IgniteTickSeconds * System.Diagnostics.Stopwatch.Frequency);
        GameObject source = room.FindObject(status.IgniteSourceId);
        CombatMath.ApplyDamage(room, source, player, status.IgniteDamagePerTick, DamageTypes.True);
    }
}
