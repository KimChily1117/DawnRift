using Google.Protobuf.Protocol;
using Server.Game.Movement;
using Server.Game.Objects;
using Server.Game.Room;
using System;
using System.Collections.Generic;
using Vec3 = System.Numerics.Vector3;

public class AsheSkillHandler : IChampionSkillHandler
{
    public const float FrostSlow = 0.2f;
    public const double FrostSeconds = 2.0;
    public const float FrostBonusDamage = 0.1f;
    public const float VolleyRange = 12f;
    public const float VolleySpeed = 20f;
    public const float VolleyWidth = 0.2f;
    public const float VolleyConeDegrees = 40f;
    public const float HawkSpeed = 14f;
    public const float HawkSightRadius = 10f;
    public const float HawkMaxRange = 150f;
    public const float ArrowSpeed = 16f;
    public const float ArrowWidth = 1.3f;
    public const float ArrowRange = 150f;
    public const float ArrowFullStunDistance = 15f;
    public const float ArrowSplashSlow = 0.5f;
    public const double ArrowSplashSlowSeconds = 3.0;

    public static double? CastLockSeconds(int skillId) => skillId == 1 ? (double?)null : 0.25;

    public bool HandleSkill(GameRoom room, GameObject caster, C_SkillCast skillPacket)
    {
        if (!(caster is Player ashe) || ashe.Info == null)
            return false;

        switch (skillPacket.SkillId)
        {
        case (int)SkillType.Gerneral:
            return BasicAttackHandler.Handle(room, caster, skillPacket);
        case (int)SkillType.QSpell:
            return RangersFocus(room, ashe);
        case (int)SkillType.WSpell:
            return Volley(room, ashe, skillPacket);
        case (int)SkillType.ESpell:
            return Hawkshot(room, ashe, skillPacket);
        case (int)SkillType.RSpell:
            return CrystalArrow(room, ashe, skillPacket);
        default:
            return false;
        }
    }

    private static SpellData Spell(GameObject caster,
                                   SkillType slot) => CombatMath.DataOf(caster)?.Spell(slot) ?? SpellData.Empty;

    // ---- passive and Q stacks ----

    public static void OnBasicAttackHit(GameRoom room, Player ashe, GameObject target)
    {
        bool crit = ashe.Status.AsheCritPending;
        ashe.Status.AsheCritPending = false;

        if (target is Player victim && victim.Info != null && victim.Info.Hp > 0)
            Buffs.Slow(room, ashe, victim, crit ? FrostSlow * 2f : FrostSlow, FrostSeconds, Buffs.PassiveSkillId);

        if (ashe.Status.AsheQActive || Progression.SkillRank(ashe, SkillType.QSpell) <= 0)
            return;
        SpellData q = Spell(ashe, SkillType.QSpell);
        int rank = Math.Max(1, Progression.SkillRank(ashe, SkillType.QSpell));
        int max = (int)q.Value("MaxStacks", rank, 4f);
        ashe.Status.AsheFocus = Math.Min(max, ashe.Status.AsheFocus + 1);

        double seconds = q.Value("StackDuration", rank, 4f);
        ashe.Status.AsheFocusExpiresAt = StatusEffects.After(seconds);
        Buffs.State(room, ashe, BUFF_TYPE.BuffAsheFocus, (int)SkillType.QSpell, ashe.Status.AsheFocus, seconds);
    }

    public static float BasicAttackDamage(Player ashe, GameObject target)
    {
        float critBonus = Server.Game.Items.ItemEffects.TotalStats(ashe).Crit *
                          (Server.Game.Items.ItemEffects.CritMultiplier(ashe) - 1f);
        float multiplier = target is Player victim && victim.Status.IsSlowed ? 1f + FrostBonusDamage + critBonus : 1f;

        if (!ashe.Status.AsheQActive)
            return multiplier;
        int rank = Math.Max(1, Progression.SkillRank(ashe, SkillType.QSpell));

        return multiplier * Spell(ashe, SkillType.QSpell).Value("DamagePerStrike", rank, 1.1f);
    }

    public static float? Cooldown(Player ashe, int skillId, int rank)
    {
        if (skillId == (int)SkillType.QSpell)
            return 0f;

        if (skillId == (int)SkillType.ESpell)
            return Spell(ashe, SkillType.ESpell).Value("BaseCooldown", rank, 90f);

        return null;
    }

    public static void Update(GameRoom room, Player ashe, float deltaTime)
    {
        if (ashe.Status.AsheFocus > 0 && !ashe.Status.AsheQActive &&
            StatusEffects.Now >= ashe.Status.AsheFocusExpiresAt)
        {
            --ashe.Status.AsheFocus;
            ashe.Status.AsheFocusExpiresAt = StatusEffects.After(1.0);
            Buffs.State(room, ashe, BUFF_TYPE.BuffAsheFocus, (int)SkillType.QSpell, ashe.Status.AsheFocus,
                        ashe.Status.AsheFocus > 0 ? 1.0 : 0.0);
        }
    }

    private bool RangersFocus(GameRoom room, Player ashe)
    {
        SpellData q = Spell(ashe, SkillType.QSpell);
        int rank = Progression.SkillRank(ashe, SkillType.QSpell);

        if (ashe.Status.AsheQActive || ashe.Status.AsheFocus < (int)q.Value("MaxStacks", rank, 4f))
            return false;

        double seconds = q.Value("BuffDuration", rank, 6f);
        ashe.Status.AsheFocus = 0;
        ashe.Status.AsheQEndsAt = StatusEffects.After(seconds);
        Buffs.AttackSpeed(room, ashe, ashe, q.Value("BonusAS", rank, 20f) / 100f, seconds, (int)SkillType.QSpell);
        Buffs.State(room, ashe, BUFF_TYPE.BuffAsheFocus, (int)SkillType.QSpell, 0, 0);
        Buffs.State(room, ashe, BUFF_TYPE.BuffAsheQ, (int)SkillType.QSpell, 1, seconds);
        room.Broadcast(new S_SkillResult { CasterId = ashe.Info.ObjectId, SkillId = (int)SkillType.QSpell });

        return true;
    }

    // ---- W, E, R ----

    private static bool Aim(Player caster, C_SkillCast packet, out Vec3 origin, out Vec3 direction, out float distance)
    {
        origin = caster.Info.Position.ToNumericsVector3();
        direction = Vec3.UnitX;
        distance = 0f;

        if (packet.TargetPos == null)
            return false;
        Vec3 target = packet.TargetPos.ToNumericsVector3();
        direction = target - origin;
        direction.Y = 0f;
        distance = direction.Length();

        if (!float.IsFinite(distance) || distance < 0.01f)
            return false;
        direction /= distance;

        return true;
    }

    private static void Announce(GameRoom room, Player caster, SkillType skill, Vec3 origin, Vec3 direction,
                                 Vec3? center = null, float radius = 0f)
    {
        room.Broadcast(new S_SkillResult {
            CasterId = caster.Info.ObjectId,
            SkillId = (int)skill,
            CastOrigin = origin.ToProtoVector3(),
            CastDirection = direction.ToProtoVector3(),
            IsAreaSkill = center.HasValue,
            CenterPos = (center ?? origin).ToProtoVector3(),
            AreaRadius = radius,
        });
    }

    private bool Volley(GameRoom room, Player ashe, C_SkillCast packet)
    {
        if (!Aim(ashe, packet, out Vec3 origin, out Vec3 direction, out _) ||
            !CombatActionMovement.TryBegin(room, ashe, packet.SkillId))
            return false;
        SpellData w = Spell(ashe, SkillType.WSpell);
        int rank = Progression.SkillRank(ashe, SkillType.WSpell);
        int arrows = Math.Max(1, (int)w.Value("NumberOfArrowsTooltip", rank, 7f));
        float damage =
            w.Value("BaseDamage", rank, 60f) + w.Value("ADRatio", rank, 1.0f) * CombatMath.BonusAttackDamage(ashe);
        Announce(room, ashe, SkillType.WSpell, origin, direction);
        var struck = new HashSet<ulong>();
        float step = arrows > 1 ? VolleyConeDegrees / (arrows - 1) : 0f;

        for (int i = 0; i < arrows; ++i)
        {
            float angle = (-VolleyConeDegrees * 0.5f + step * i) * MathF.PI / 180f;
            Vec3 arrowDirection = new Vec3(direction.X * MathF.Cos(angle) - direction.Z * MathF.Sin(angle), 0f,
                                           direction.X * MathF.Sin(angle) + direction.Z * MathF.Cos(angle));
            var arrow = new Skillshot(room.GenerateProjectileId(), ashe, (int)SkillType.WSpell, origin, arrowDirection,
                                      VolleySpeed) {
                Range = VolleyRange,
                Width = VolleyWidth,
                StopAtFirstHit = true,
                CanHit = unit => unit.Info.Hp > 0 && unit.Info.TeamId != ashe.Info.TeamId && !(unit is Ward) &&
                                 !(unit is MapUnit structure && structure.IsStructure) &&
                                 !struck.Contains(unit.Info.ObjectId),
                OnHit =
                    (shot, unit) =>
                {
                    struck.Add(unit.Info.ObjectId);
                    CombatMath.ApplyDamage(room, ashe, unit, damage, DamageTypes.Physical, (int)SkillType.WSpell);

                    if (unit is Player victim)
                        Buffs.Slow(room, ashe, victim, FrostSlow, FrostSeconds, (int)SkillType.WSpell);
                },
            };
            room.AddSkillshot(arrow);
        }

        return true;
    }

    private bool Hawkshot(GameRoom room, Player ashe, C_SkillCast packet)
    {
        if (!Aim(ashe, packet, out Vec3 origin, out Vec3 direction, out float distance) ||
            !CombatActionMovement.TryBegin(room, ashe, packet.SkillId))
            return false;
        SpellData e = Spell(ashe, SkillType.ESpell);
        int rank = Progression.SkillRank(ashe, SkillType.ESpell);

        double vision = e.Value("VisionDuration", rank, 5f);
        float range = Math.Min(distance, HawkMaxRange);
        Vec3 landing = origin + direction * range;
        Announce(room, ashe, SkillType.ESpell, origin, direction, landing, HawkSightRadius);
        var hawk =
            new Skillshot(room.GenerateProjectileId(), ashe, (int)SkillType.ESpell, origin, direction, HawkSpeed) {
                Range = range,
                Width = 0f,
                CanHit = unit => false,

                OnEnd = shot => room.Vision.AddSight(ashe.Info.TeamId, landing, HawkSightRadius, vision),
            };
        room.AddSkillshot(hawk);

        return true;
    }

    private bool CrystalArrow(GameRoom room, Player ashe, C_SkillCast packet)
    {
        if (!Aim(ashe, packet, out Vec3 origin, out Vec3 direction, out _) ||
            !CombatActionMovement.TryBegin(room, ashe, packet.SkillId))
            return false;
        SpellData r = Spell(ashe, SkillType.RSpell);
        int rank = Progression.SkillRank(ashe, SkillType.RSpell);
        float damage =
            r.Value("RBaseDamage", rank, 200f) + r.Value("APRatio", rank, 1.2f) * CombatMath.AbilityPower(ashe);

        double minStun = r.Value("MinStunDuration", rank, 1f), maxStun = r.Value("MaxStunDuration", rank, 3.5f);
        float splash = r.Value("SlowRadius", rank, 400f) * ChampionData.UnitScale;
        Announce(room, ashe, SkillType.RSpell, origin, direction);
        var arrow =
            new Skillshot(room.GenerateProjectileId(), ashe, (int)SkillType.RSpell, origin, direction, ArrowSpeed) {
                Range = ArrowRange,
                Width = ArrowWidth,
                StopAtFirstHit = true,
                ChampionsOnly = true,
            };
        arrow.OnHit = (shot, unit) =>
        {
            if (!(unit is Player victim))
                return;
            CombatMath.ApplyDamage(room, ashe, victim, damage, DamageTypes.Magic, (int)SkillType.RSpell);

            double stun = minStun + (maxStun - minStun) * Math.Clamp(shot.Traveled / ArrowFullStunDistance, 0f, 1f);
            Buffs.Stun(room, ashe, victim, stun, (int)SkillType.RSpell);

            foreach (GameObject near in room.GetObjectsInRange(victim.Info.Position.ToNumericsVector3(), splash))
            {
                if (ReferenceEquals(near, victim) || !(near is Player other) || other.Info == null ||
                    other.Info.Hp <= 0 || other.Info.TeamId == ashe.Info.TeamId || other is Ward ||
                    (other is MapUnit structure && structure.IsStructure))
                    continue;
                CombatMath.ApplyDamage(room, ashe, other, damage * 0.5f, DamageTypes.Magic, (int)SkillType.RSpell);
                Buffs.Slow(room, ashe, other, ArrowSplashSlow, ArrowSplashSlowSeconds, (int)SkillType.RSpell);
            }
        };
        room.AddSkillshot(arrow);

        return true;
    }
}
