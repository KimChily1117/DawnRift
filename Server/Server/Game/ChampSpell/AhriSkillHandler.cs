using Google.Protobuf.Protocol;
using Server.Game.Movement;
using Server.Game.Objects;
using Server.Game.Room;
using System;
using System.Collections.Generic;
using System.Linq;
using Vec3 = System.Numerics.Vector3;

public class AhriSkillHandler : IChampionSkillHandler
{
    public const float OrbRange = 9.7f;
    public const float OrbWidth = 1.0f;
    public const float OrbSpeed = 25f;
    public const float OrbReturnSpeed = 0.6f;
    public const float OrbReturnAcceleration = 19f;
    public const float OrbReturnMaxSpeed = 26f;
    public const float CharmRange = 10f;
    public const float CharmWidth = 0.6f;
    public const float CharmSpeed = 15.5f;
    public const float FoxFireSpeed = 18f;
    public const float SpiritRushBoltSpeed = 22f;
    public const int EssenceKills = 9;

    public const float EssenceHealMin = 20f, EssenceHealMax = 80f;
    public const float TakedownHealMin = 75f, TakedownHealMax = 165f;

    public static double? CastLockSeconds(int skillId) => skillId == 2 || skillId == 4 ? (double?)null : 0.25;

    public bool HandleSkill(GameRoom room, GameObject caster, C_SkillCast skillPacket)
    {
        if (!(caster is Player ahri) || ahri.Info == null)
            return false;

        switch (skillPacket.SkillId)
        {
        case (int)SkillType.Gerneral:
            return BasicAttackHandler.Handle(room, caster, skillPacket);
        case (int)SkillType.QSpell:
            return OrbOfDeception(room, ahri, skillPacket);
        case (int)SkillType.WSpell:
            return FoxFire(room, ahri);
        case (int)SkillType.ESpell:
            return Charm(room, ahri, skillPacket);
        case (int)SkillType.RSpell:
            return SpiritRush(room, ahri, skillPacket);
        default:
            return false;
        }
    }

    private static SpellData Spell(GameObject caster,
                                   SkillType slot) => CombatMath.DataOf(caster)?.Spell(slot) ?? SpellData.Empty;

    private static bool Enemy(Player ahri,
                              GameObject unit) => unit?.Info != null
                                                  && unit.Info.Hp > 0 && unit.Info.TeamId != ahri.Info.TeamId &&
                                                  !(unit is Ward) &&
                                                  !(unit is MapUnit structure && structure.IsStructure);

    private static bool Aim(Player caster, C_SkillCast packet, out Vec3 origin, out Vec3 direction, out float distance)
    {
        origin = caster.Info.Position.ToNumericsVector3();
        direction = Vec3.UnitX;
        distance = 0f;

        if (packet.TargetPos == null)
            return false;
        direction = packet.TargetPos.ToNumericsVector3() - origin;
        direction.Y = 0f;
        distance = direction.Length();

        if (!float.IsFinite(distance) || distance < 0.01f)
            return false;
        direction /= distance;

        return true;
    }

    private static void Announce(GameRoom room, Player ahri, SkillType skill, Vec3 origin, Vec3 direction)
    {
        room.Broadcast(new S_SkillResult {
            CasterId = ahri.Info.ObjectId,
            SkillId = (int)skill,
            CastOrigin = origin.ToProtoVector3(),
            CastDirection = direction.ToProtoVector3(),
        });
    }

    // ---- passive ----

    private static float Heal(Player ahri, float atLevel1,
                              float atLevel18) => atLevel1 + (atLevel18 - atLevel1) *
                                                                 (Math.Clamp(ahri.Progress.Level, 1, 18) - 1) / 17f;

    public static void OnKill(GameRoom room, Player ahri, GameObject victim)
    {
        float heal;

        if (Progression.IsChampion(victim))
            heal = Heal(ahri, TakedownHealMin, TakedownHealMax);
        else
        {
            ahri.Status.AhriEssence++;

            if (ahri.Status.AhriEssence < EssenceKills)
            {
                Buffs.State(room, ahri, BUFF_TYPE.BuffAhriPassive, Buffs.PassiveSkillId, ahri.Status.AhriEssence, 1.0);

                return;
            }

            ahri.Status.AhriEssence = 0;
            heal = Heal(ahri, EssenceHealMin, EssenceHealMax);
        }

        Buffs.State(room, ahri, BUFF_TYPE.BuffAhriPassive, Buffs.PassiveSkillId, ahri.Status.AhriEssence, 0.0);

        if (ahri.Info.Hp <= 0 || heal <= 0f)
            return;
        Buffs.Heal(room, ahri, ahri, heal, Buffs.PassiveSkillId);
    }

    // ---- Q ----

    private bool OrbOfDeception(GameRoom room, Player ahri, C_SkillCast packet)
    {
        if (!Aim(ahri, packet, out Vec3 origin, out Vec3 direction, out _) ||
            !CombatActionMovement.TryBegin(room, ahri, packet.SkillId))
            return false;
        int rank = Progression.SkillRank(ahri, SkillType.QSpell);
        float damage =
            Spell(ahri, SkillType.QSpell).Value("BaseDamage", rank, 35f) + 0.5f * CombatMath.AbilityPower(ahri);
        Announce(room, ahri, SkillType.QSpell, origin, direction);
        var outward =
            new Skillshot(room.GenerateProjectileId(), ahri, (int)SkillType.QSpell, origin, direction, OrbSpeed) {
                Range = OrbRange,
                Width = OrbWidth,
                CanHit = unit => Enemy(ahri, unit),
                OnHit = (shot, unit) =>
                    CombatMath.ApplyDamage(room, ahri, unit, damage, DamageTypes.Magic, (int)SkillType.QSpell),
            };
        outward.OnEnd = shot =>
        {
            if (ahri.Info == null || ahri.Info.Hp <= 0 || !ReferenceEquals(room.FindObject(ahri.Info.ObjectId), ahri))
                return;
            var back = new Skillshot(room.GenerateProjectileId(), ahri, (int)SkillType.QSpell, shot.Position,
                                     -direction, OrbReturnSpeed) {
                Variant = 1,
                Homing = ahri,
                Acceleration = OrbReturnAcceleration,
                MaxSpeed = OrbReturnMaxSpeed,
                Width = OrbWidth,
                CanHit = unit => Enemy(ahri, unit),
                OnHit = (s, unit) =>
                    CombatMath.ApplyDamage(room, ahri, unit, damage, DamageTypes.True, (int)SkillType.QSpell),
            };
            room.AddSkillshot(back);
        };
        room.AddSkillshot(outward);

        return true;
    }

    // ---- W ----

    private bool FoxFire(GameRoom room, Player ahri)
    {
        if (ahri.Info.Hp <= 0)
            return false;
        SpellData w = Spell(ahri, SkillType.WSpell);
        int rank = Progression.SkillRank(ahri, SkillType.WSpell);
        float damage = w.Value("BaseDamage", rank, 40f) + 0.4f * CombatMath.AbilityPower(ahri);
        float repeat = w.Value("RepeatDamageMod", rank, 0.4f);
        float range = w.Value("BonusAcquisitionRange", rank, 725f) * ChampionData.UnitScale;
        float minionBonus = w.Value("MinionBonusDamageMultiplier", rank, 2f);
        float minionThreshold = w.Value("MinionBonusDamageThreshold", rank, 0.2f);

        double initialDelay = w.Value("InitialDelay", rank, 0.25f),
               secondaryDelay = w.Value("SecondaryDelay", rank, 0.4f);
        long flamesEndAt = StatusEffects.After(w.Value("FlameDuration", rank, 2.5f));
        Buffs.MoveSpeed(room, ahri, ahri, w.Value("MovementSpeed", rank, 0.4f),
                        w.Value("MovementSpeedDuration", rank, 2f), (int)SkillType.WSpell, decays: true);
        room.Broadcast(new S_SkillResult { CasterId = ahri.Info.ObjectId, SkillId = (int)SkillType.WSpell });
        var struck = new Dictionary<ulong, int>();

        for (int i = 0; i < 3; ++i)
        {
            int delay = (int)Math.Round((initialDelay + (i > 0 ? secondaryDelay * i / 2.0 : 0.0)) * 1000.0);

            Action seek = null;
            seek = () =>
            {
                if (ahri.Info == null || ahri.Info.Hp <= 0 ||
                    !ReferenceEquals(room.FindObject(ahri.Info.ObjectId), ahri))
                    return;
                GameObject target = Nearest(room, ahri, range, champion => true);

                if (target == null)
                {
                    if (StatusEffects.Now < flamesEndAt)
                        room.PushAfter(100, seek);

                    return;
                }

                struck.TryGetValue(target.Info.ObjectId, out int hits);
                struck[target.Info.ObjectId] = hits + 1;
                float amount = damage * (hits > 0 ? repeat : 1f);

                if (target is Minion && target.Info.Hp < target.Info.MaxHp * minionThreshold)
                    amount *= minionBonus;
                ulong id = room.GenerateProjectileId();
                room.AddProjectile(
                    new Projectile(id, ahri, target, FoxFireSpeed, amount, DamageTypes.Magic, (int)SkillType.WSpell));
                room.Broadcast(new S_ProjectileSpawn {
                    ProjectileId = id,
                    CasterId = ahri.Info.ObjectId,
                    TargetId = target.Info.ObjectId,
                    StartPos = ahri.Info.Position,
                    EndPos = target.Info.Position,
                    Speed = FoxFireSpeed,
                    SkillId = (int)SkillType.WSpell,
                });
            };
            room.PushAfter(delay, seek);
        }

        return true;
    }

    private static GameObject Nearest(GameRoom room, Player ahri, float range, Func<GameObject, bool> filter,
                                      ICollection<ulong> skip = null)
    {
        Vec3 at = ahri.Info.Position.ToNumericsVector3();

        return room.GetObjectsInRange(at, range)
            .Where(unit => Enemy(ahri, unit) && filter(unit) && (skip == null || !skip.Contains(unit.Info.ObjectId)) &&
                           !room.Vision.IsHiddenFrom(ahri.Info.TeamId, unit.Info.ObjectId))
            .OrderBy(unit => Progression.IsChampion(unit) ? 0 : 1)
            .ThenBy(unit => Vec3.DistanceSquared(unit.Info.Position.ToNumericsVector3(), at))
            .FirstOrDefault();
    }

    // ---- E ----

    private bool Charm(GameRoom room, Player ahri, C_SkillCast packet)
    {
        if (!Aim(ahri, packet, out Vec3 origin, out Vec3 direction, out _) ||
            !CombatActionMovement.TryBegin(room, ahri, packet.SkillId))
            return false;
        SpellData e = Spell(ahri, SkillType.ESpell);
        int rank = Progression.SkillRank(ahri, SkillType.ESpell);
        float damage = e.Value("BaseDamage", rank, 80f) + 0.85f * CombatMath.AbilityPower(ahri);

        double seconds = e.Value("CharmDuration", rank, 1.2f);
        float slow = Math.Abs(e.Value("SlowPercent", rank, -0.65f));
        Announce(room, ahri, SkillType.ESpell, origin, direction);
        room.AddSkillshot(
            new Skillshot(room.GenerateProjectileId(), ahri, (int)SkillType.ESpell, origin, direction, CharmSpeed) {
                Range = CharmRange,
                Width = CharmWidth,
                StopAtFirstHit = true,
                CanHit = unit => Enemy(ahri, unit),
                OnHit =
                    (shot, unit) =>
                {
                    CombatMath.ApplyDamage(room, ahri, unit, damage, DamageTypes.Magic, (int)SkillType.ESpell);

                    if (unit is Player victim)
                        Buffs.Charm(room, ahri, victim, seconds, slow, (int)SkillType.ESpell);
                },
            });

        return true;
    }

    // ---- R ----

    public static float? Cooldown(Player ahri, int skillId, int rank)
    {
        if (skillId != (int)SkillType.RSpell)
            return null;
        SpellData r = Spell(ahri, SkillType.RSpell);

        return ahri.Status.AhriRDashes > 0 ? r.Value("RDashCooldown", rank, 1f) : r.CooldownAt(rank, 140f);
    }

    public static void Update(GameRoom room, Player ahri, float deltaTime)
    {
        if (ahri.Status.AhriRDashes > 0 && StatusEffects.Now >= ahri.Status.AhriRWindowEndsAt)
        {
            ahri.Status.AhriRDashes = 0;
            int rank = Math.Max(1, Progression.SkillRank(ahri, SkillType.RSpell));
            float cooldown = Spell(ahri, SkillType.RSpell).CooldownAt(rank, 140f);
            ahri.StartSkillCooldown((int)SkillType.RSpell, cooldown);
            ahri.Session?.Send(new S_SkillCooldown { SkillId = (int)SkillType.RSpell, Duration = cooldown });
            Buffs.State(room, ahri, BUFF_TYPE.BuffAhriR, (int)SkillType.RSpell, 0, 0);
        }
    }

    private bool SpiritRush(GameRoom room, Player ahri, C_SkillCast packet)
    {
        if (!Aim(ahri, packet, out Vec3 origin, out Vec3 direction, out _))
            return false;
        SpellData r = Spell(ahri, SkillType.RSpell);
        int rank = Progression.SkillRank(ahri, SkillType.RSpell);

        if (ahri.Status.AhriRDashes <= 0)
        {
            // The first dash opens the window.
            ahri.Status.AhriRDashes = (int)r.Value("RMaxCasts", rank, 3f);
            ahri.Status.AhriRWindowEndsAt = StatusEffects.After(r.Value("RRecastWindow", rank, 15f));
        }

        float distance = r.Value("RDashDistance", rank, 500f) * ChampionData.UnitScale;
        float speed = r.Value("RBaseDashSpeed", rank, 1200f) * ChampionData.UnitScale;
        Vec3 end = Dash.Clip(room, origin, direction, distance);
        float length = Vec3.Distance(origin, end);
        --ahri.Status.AhriRDashes;

        double window = Math.Max(0.0, (ahri.Status.AhriRWindowEndsAt - StatusEffects.Now) /
                                          (double)System.Diagnostics.Stopwatch.Frequency);
        Buffs.State(room, ahri, BUFF_TYPE.BuffAhriR, (int)SkillType.RSpell, ahri.Status.AhriRDashes,
                    ahri.Status.AhriRDashes > 0 ? window : 0.0);

        if (ahri.Status.AhriRDashes == 0)
            ahri.Status.AhriRWindowEndsAt = 0;
        Announce(room, ahri, SkillType.RSpell, origin, direction);

        double seconds = Dash.Start(room, ahri, end, speed);

        float damage = r.Value("RBaseDamage", rank, 75f) + 0.35f * CombatMath.AbilityPower(ahri);
        int targets = (int)r.Value("RMaxTargetsPerCast", rank, 3f);
        float acquisition = r.Value("RAcquisitionRange", rank, 600f) * ChampionData.UnitScale;

        long giveUpAt = StatusEffects.After(seconds + 1.0);
        Action fire = null;
        fire = () =>
        {
            if (ahri.Info == null || ahri.Info.Hp <= 0 || !ReferenceEquals(room.FindObject(ahri.Info.ObjectId), ahri))
                return;

            if (ahri.Status.IsDashing && StatusEffects.Now < giveUpAt)
            {
                room.PushAfter(50, fire);

                return;
            }

            var chosen = new List<ulong>();

            for (int i = 0; i < targets; ++i)
            {
                GameObject target = Nearest(room, ahri, acquisition, unit => true, chosen);

                if (target == null)
                    break;
                chosen.Add(target.Info.ObjectId);
                ulong id = room.GenerateProjectileId();
                room.AddProjectile(new Projectile(id, ahri, target, SpiritRushBoltSpeed, damage, DamageTypes.Magic,
                                                  (int)SkillType.RSpell));
                room.Broadcast(new S_ProjectileSpawn {
                    ProjectileId = id,
                    CasterId = ahri.Info.ObjectId,
                    TargetId = target.Info.ObjectId,
                    StartPos = ahri.Info.Position,
                    EndPos = target.Info.Position,
                    Speed = SpiritRushBoltSpeed,
                    SkillId = (int)SkillType.RSpell,
                });
            }
        };
        room.PushAfter((int)Math.Round(seconds * 1000.0), fire);

        return true;
    }
}

public static class Dash
{
    public static Vec3 Clip(GameRoom room, Vec3 origin, Vec3 direction, float distance)
    {
        Vec3 last = origin;
        const float step = 0.2f;

        for (float d = step; d <= distance + 1e-3f; d += step)
        {
            Vec3 point = origin + direction * Math.Min(d, distance);

            if (!room.Navigation.TryWorldToCell(point, out var cell) || !room.Navigation.IsWalkable(cell.X, cell.Z))
                break;
            last = point;
        }

        return last;
    }

    // Returns how long it takes.
    public static double Start(GameRoom room, Player unit, Vec3 end, float speed)
    {
        Vec3 start = unit.Info.Position.ToNumericsVector3();
        float length = Vec3.Distance(new Vec3(start.X, 0f, start.Z), new Vec3(end.X, 0f, end.Z));

        double seconds = length / Math.Max(0.1f, speed);

        if (length < 0.05f)
            return 0.0;
        room.CancelPlayerMovement(unit);
        unit.Status.DashSpeed = speed;

        unit.Status.DashEndsAt = StatusEffects.After(seconds * 1.5 + 0.3);

        if (!AuthoritativeMoveService.TryStartServerMove(room, unit, end))
            unit.Status.DashEndsAt = 0;

        return seconds;
    }
}
