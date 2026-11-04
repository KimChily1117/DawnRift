using Google.Protobuf.Protocol;
using Server.Game.Movement;
using Server.Game.Objects;
using Server.Game.Room;
using System;
using System.Collections.Generic;
using System.Linq;
using Vec3 = System.Numerics.Vector3;

public sealed class KatarinaDagger
{
    public ulong Id;
    public Vec3 Position;
    public long LandsAt;
    public long ExpiresAt;
    public bool Landed => StatusEffects.Now >= LandsAt;
}

public class KatarinaSkillHandler : IChampionSkillHandler
{
    public const float PickupRadius = 1.5f;
    public const float SpinRadius = 3.4f;
    public const double DaggerSeconds = 4.0;
    public const double QLandSeconds = 1.0;
    public const double WLandSeconds = 1.25;
    public const float QSpeed = 16f;
    public const float EBlinkRange = 7.25f;
    public const float EDaggerRadius = 1.5f;
    public const float EStrikeRadius = 2.0f;
    public const float RSpeed = 14.5f;
    public const int RTargets = 3;
    public const double TakedownCooldown = 15.0;
    public static readonly float[] PickupCooldownShare = { 0.78f, 0.84f, 0.90f, 0.96f };

    public static double? CastLockSeconds(int skillId) => skillId == 1 || skillId == 0 ? 0.25 : (double?)null;

    public bool HandleSkill(GameRoom room, GameObject caster, C_SkillCast skillPacket)
    {
        if (!(caster is Player katarina) || katarina.Info == null || katarina.Info.Hp <= 0)
            return false;

        if (skillPacket.SkillId == (int)SkillType.Gerneral)
            return BasicAttackHandler.Handle(room, caster, skillPacket);

        if (skillPacket.SkillId == (int)SkillType.RSpell)
            return DeathLotus(room, katarina);
        bool accepted = skillPacket.SkillId switch {
            (int)SkillType.QSpell => BouncingBlade(room, katarina, skillPacket),
            (int)SkillType.WSpell => Preparation(room, katarina),
            (int)SkillType.ESpell => Shunpo(room, katarina, skillPacket),
            _ => false,
        };

        if (accepted)
            StopChannel(room, katarina);

        return accepted;
    }

    private static SpellData Spell(GameObject caster,
                                   SkillType slot) => CombatMath.DataOf(caster)?.Spell(slot) ?? SpellData.Empty;

    private static Vec3 PositionOf(GameObject unit) => unit.Info.Position.ToNumericsVector3();

    private static float DistanceXZ(Vec3 a, Vec3 b) => Vec3.Distance(new Vec3(a.X, 0f, a.Z), new Vec3(b.X, 0f, b.Z));

    private static bool Enemy(Player katarina,
                              GameObject unit) => unit?.Info != null
                                                  && unit.Info.Hp > 0 && unit.Info.TeamId != katarina.Info.TeamId &&
                                                  !(unit is Ward) &&
                                                  !(unit is MapUnit structure && structure.IsStructure);

    private static bool Visible(GameRoom room, Player katarina, GameObject unit) =>
        !(room.Vision?.IsHiddenFrom(katarina.Info.TeamId, unit.Info.ObjectId) ?? false);

    private static void Announce(GameRoom room, Player katarina, SkillType skill, Vec3 origin, Vec3 direction,
                                 Vec3? center = null, float radius = 0f, ulong target = 0)
    {
        var result = new S_SkillResult {
            CasterId = katarina.Info.ObjectId,
            SkillId = (int)skill,
            CastOrigin = origin.ToProtoVector3(),
            CastDirection = direction.ToProtoVector3(),
            IsAreaSkill = center.HasValue,
            CenterPos = (center ?? origin).ToProtoVector3(),
            AreaRadius = radius,
        };

        if (target != 0)
            result.HitObjects.Add(target);
        room.Broadcast(result);
    }

    private static void SendCooldown(Player katarina, SkillType skill, double left) =>
        katarina.Session?.Send(new S_SkillCooldown { SkillId = (int)skill, Duration = (float)left });

    // ---- daggers ----

    public static KatarinaDagger ThrowDagger(GameRoom room, Player katarina, Vec3 from, Vec3 to, double landSeconds)
    {
        var dagger = new KatarinaDagger {
            Id = room.GenerateProjectileId(),
            Position = Dash.Clip(room, from, Direction(from, to), DistanceXZ(from, to)),
            LandsAt = StatusEffects.After(landSeconds),
            ExpiresAt = StatusEffects.After(landSeconds + DaggerSeconds),
        };
        dagger.Position.Y = to.Y;
        katarina.Status.KatarinaDaggers.Add(dagger);
        room.Broadcast(new S_KatarinaDagger {
            DaggerId = dagger.Id,
            OwnerId = katarina.Info.ObjectId,
            Position = dagger.Position.ToProtoVector3(),
            Origin = from.ToProtoVector3(),
            LandIn = (float)landSeconds,
            Lifetime = (float)DaggerSeconds,
            State = 0,
        });

        return dagger;
    }

    private static Vec3 Direction(Vec3 from, Vec3 to)
    {
        Vec3 d = to - from;
        d.Y = 0f;

        return d.LengthSquared() > 1e-6f ? Vec3.Normalize(d) : Vec3.UnitX;
    }

    public static float SpinDamage(Player katarina) => 68f + (308f - 68f) *
                                                                 (Math.Clamp(CombatMath.LevelOf(katarina), 1, 18) - 1) /
                                                                 17f;

    private static void PickUp(GameRoom room, Player katarina, KatarinaDagger dagger)
    {
        katarina.Status.KatarinaDaggers.Remove(dagger);
        room.Broadcast(new S_KatarinaDagger {
            DaggerId = dagger.Id,
            OwnerId = katarina.Info.ObjectId,
            Position = PositionOf(katarina).ToProtoVector3(),
            State = 1,
        });
        float damage = SpinDamage(katarina) + 0.6f * CombatMath.BonusAttackDamage(katarina) +
                       (0.7f + 0.3f * (Math.Clamp(CombatMath.LevelOf(katarina), 1, 18) - 1) / 17f) *
                           CombatMath.AbilityPower(katarina);

        foreach (GameObject unit in room.GetObjectsInRange(PositionOf(katarina), SpinRadius))
            if (Enemy(katarina, unit))
                CombatMath.ApplyDamage(room, katarina, unit, damage, DamageTypes.Magic, Buffs.PassiveSkillId);
        int eRank = Progression.SkillRank(katarina, SkillType.ESpell);

        if (eRank > 0 && katarina.CooldownRemaining((int)SkillType.ESpell) > 0.0)
        {
            int level = CombatMath.LevelOf(katarina);
            float share = PickupCooldownShare[level >= 16 ? 3 : level >= 11 ? 2 : level >= 6 ? 1 : 0];
            float cooldown = Spell(katarina, SkillType.ESpell).CooldownAt(eRank, 10f);
            SendCooldown(katarina, SkillType.ESpell,
                         katarina.ReduceSkillCooldown((int)SkillType.ESpell, share * cooldown));
        }
    }

    public static void OnTakedown(GameRoom room, Player katarina, Player victim)
    {
        if (!Progression.IsChampion(victim))
            return;

        foreach (SkillType skill in new[] { SkillType.QSpell, SkillType.WSpell, SkillType.ESpell })
            if (katarina.CooldownRemaining((int)skill) > 0.0)
                SendCooldown(katarina, skill, katarina.ReduceSkillCooldown((int)skill, TakedownCooldown));
    }

    public static void Update(GameRoom room, Player katarina, float deltaTime)
    {
        List<KatarinaDagger> daggers = katarina.Status.KatarinaDaggers;
        Vec3 at = PositionOf(katarina);

        for (int i = daggers.Count - 1; i >= 0; --i)
        {
            if (i >= daggers.Count)
                continue;
            KatarinaDagger dagger = daggers[i];

            if (StatusEffects.Now >= dagger.ExpiresAt)
            {
                daggers.RemoveAt(i);
                room.Broadcast(new S_KatarinaDagger { DaggerId = dagger.Id, OwnerId = katarina.Info.ObjectId,
                                                      Position = dagger.Position.ToProtoVector3(), State = 2 });
            }
            else if (dagger.Landed && DistanceXZ(at, dagger.Position) <= PickupRadius)
            {
                PickUp(room, katarina, dagger);
            }
        }

        // Death Lotus ends when she moves or is stopped.
        if (katarina.Status.KatarinaChanneling &&
            ((katarina.MovementState?.IsActive ?? false) || katarina.Status.IsDisabled || katarina.Status.IsSilenced))
            StopChannel(room, katarina);
    }

    // ---- Q ----

    private bool BouncingBlade(GameRoom room, Player katarina, C_SkillCast packet)
    {
        SpellData q = Spell(katarina, SkillType.QSpell);
        int rank = Progression.SkillRank(katarina, SkillType.QSpell);
        float range = (q.CastRangeUnits > 0f ? q.CastRangeUnits : 6.25f) + MapUnit.ChampionRadius;
        GameObject first = room.FindObject(packet.TargetId);

        if (!Enemy(katarina, first) || !Visible(room, katarina, first) ||
            DistanceXZ(PositionOf(katarina), PositionOf(first)) > range + Skillshot.RadiusOf(first))
            return false;

        if (!CombatActionMovement.TryBegin(room, katarina, packet.SkillId))
            return false;
        float damage =
            q.Value("BaseDamage", rank, 80f) + q.Value("QAPRatio", rank, 0.35f) * CombatMath.AbilityPower(katarina);
        float bounceRadius = q.Value("BounceRadius", rank, 450f) * ChampionData.UnitScale;
        int bounces = (int)q.Value("MaxBounces", rank, 2f);
        float offset = q.Value("BounceOffset", rank, 350f) * ChampionData.UnitScale;
        Vec3 origin = PositionOf(katarina);
        Vec3 behind = Direction(origin, PositionOf(first));
        Announce(room, katarina, SkillType.QSpell, origin, behind, target: first.Info.ObjectId);
        var hit = new HashSet<ulong>();

        void Throw(Vec3 from, GameObject target, int left)
        {
            var blade = new Skillshot(room.GenerateProjectileId(), katarina, (int)SkillType.QSpell, from,
                                      Direction(from, PositionOf(target)), QSpeed) {
                Homing = target,
                Range = 30f,
                Width = 0.1f,
                CanHit = unit => unit == target,
                StopAtFirstHit = true,
                OnHit =
                    (shot, unit) =>
                {
                    hit.Add(unit.Info.ObjectId);
                    CombatMath.ApplyDamage(room, katarina, unit, damage, DamageTypes.Magic, (int)SkillType.QSpell);
                    Vec3 here = PositionOf(unit);
                    // The first hit throws the dagger behind it, away from her.
                    if (left == bounces)
                        ThrowDagger(room, katarina, here, here + behind * offset, QLandSeconds);

                    if (left <= 0)
                        return;
                    GameObject next =
                        room.GetObjectsInRange(here, bounceRadius)
                            .Where(other => Enemy(katarina, other) && !hit.Contains(other.Info.ObjectId) &&
                                            Visible(room, katarina, other))
                            .OrderBy(other => DistanceXZ(here, PositionOf(other)))
                            .FirstOrDefault();

                    if (next != null)
                        Throw(here, next, left - 1);
                },
            };
            room.AddSkillshot(blade);
        }

        Throw(origin, first, bounces);

        return true;
    }

    // ---- W ----

    private bool Preparation(GameRoom room, Player katarina)
    {
        SpellData w = Spell(katarina, SkillType.WSpell);
        int rank = Progression.SkillRank(katarina, SkillType.WSpell);
        Vec3 at = PositionOf(katarina);
        Announce(room, katarina, SkillType.WSpell, at, Vec3.UnitZ);
        ThrowDagger(room, katarina, at, at, WLandSeconds);
        Buffs.MoveSpeed(room, katarina, katarina, w.Value("MovespeedAmount", rank, 50f) / 100f,
                        w.Value("MovespeedDuration", rank, 1.25f), (int)SkillType.WSpell, decays: true);

        return true;
    }

    // ---- E ----

    private bool Shunpo(GameRoom room, Player katarina, C_SkillCast packet)
    {
        Vec3 origin = PositionOf(katarina);
        Vec3 landing;
        Vec3 cursor = packet.TargetPos?.ToNumericsVector3() ?? origin;

        KatarinaDagger dagger = katarina.Status.KatarinaDaggers
                                    .Where(d => d.Landed && DistanceXZ(d.Position, cursor) <= EDaggerRadius + 0.5f &&
                                                DistanceXZ(origin, d.Position) <= EBlinkRange + EDaggerRadius)
                                    .OrderBy(d => DistanceXZ(d.Position, cursor))
                                    .FirstOrDefault();
        GameObject unit = room.FindObject(packet.TargetId);

        if (dagger != null)
        {
            Vec3 offset = cursor - dagger.Position;
            offset.Y = 0f;

            if (offset.Length() > EDaggerRadius)
                offset = Vec3.Normalize(offset) * EDaggerRadius;
            landing = dagger.Position + offset;
        }
        else if (unit?.Info != null && unit.Info.Hp > 0 && unit != katarina &&
                 !(unit is MapUnit structure && structure.IsStructure) && Visible(room, katarina, unit) &&
                 DistanceXZ(origin, PositionOf(unit)) <= EBlinkRange + Skillshot.RadiusOf(unit))
        {
            // Onto the unit, just past it.
            landing = PositionOf(unit) + Direction(origin, PositionOf(unit)) * 0.5f;
        }
        else
        {
            return false;
        }

        if (!room.Navigation.TryWorldToCell(landing, out var cell) || !room.Navigation.IsWalkable(cell.X, cell.Z))
            landing = unit?.Info != null && dagger == null ? PositionOf(unit) : dagger?.Position ?? landing;
        landing.Y = origin.Y;
        katarina.CancelBasicAttackWindup();
        room.TeleportPlayer(katarina, landing);
        Vec3 facing = Direction(origin, landing);
        Announce(room, katarina, SkillType.ESpell, origin, facing, landing, EStrikeRadius);
        SpellData e = Spell(katarina, SkillType.ESpell);
        int rank = Progression.SkillRank(katarina, SkillType.ESpell);
        float damage =
            e.Value("BaseDamage", rank, 20f) + e.Value("EADRatio", rank, 0.4f) * CombatMath.AttackDamage(katarina);
        GameObject struck = room.GetObjectsInRange(landing, EStrikeRadius)
                                .Where(other => Enemy(katarina, other) && Visible(room, katarina, other))
                                .OrderBy(other => DistanceXZ(landing, PositionOf(other)))
                                .FirstOrDefault();

        if (struck != null)
            CombatMath.ApplyDamage(room, katarina, struck, damage, DamageTypes.Magic, (int)SkillType.ESpell);

        return true;
    }

    // ---- R ----

    private bool DeathLotus(GameRoom room, Player katarina)
    {
        SpellData r = Spell(katarina, SkillType.RSpell);
        int rank = Progression.SkillRank(katarina, SkillType.RSpell);
        float radius = r.CastRangeUnits > 0f ? r.CastRangeUnits : 5.5f;

        double seconds = r.Value("Duration", rank, 2.5f);
        int perSecond = Math.Max(1, (int)r.Value("TicksPerSecond", rank, 6f));
        float damage = r.Value("DamagePerTick", rank, 25f) + 0.16f * CombatMath.BonusAttackDamage(katarina) +
                       0.19f * CombatMath.AbilityPower(katarina);
        float grievous = r.Value("GrievousAmount", rank, 0.4f);

        double grievousSeconds = r.Value("GrievousDuration", rank, 3f);
        Vec3 at = PositionOf(katarina);
        room.CancelPlayerMovement(katarina);
        katarina.CancelBasicAttackWindup();
        katarina.Status.KatarinaREndsAt = StatusEffects.After(seconds);
        Announce(room, katarina, SkillType.RSpell, at, Vec3.UnitZ, at, radius);
        Buffs.State(room, katarina, BUFF_TYPE.BuffKatarinaR, (int)SkillType.RSpell, radius, seconds);
        int ticks = (int)Math.Round(seconds * perSecond);
        int interval = (int)Math.Round(1000.0 / perSecond);
        int thrown = 0;
        Action volley = null;
        volley = () =>
        {
            if (katarina.Info == null || katarina.Info.Hp <= 0 || !katarina.Status.KatarinaChanneling)
                return;
            Vec3 here = PositionOf(katarina);

            foreach (GameObject target in room.GetObjectsInRange(here, radius)
                         .Where(unit => Enemy(katarina, unit) && Progression.IsChampion(unit) &&
                                        Visible(room, katarina, unit))
                         .OrderBy(unit => DistanceXZ(here, PositionOf(unit)))
                         .Take(RTargets)
                         .ToList())
            {
                var blade = new Skillshot(room.GenerateProjectileId(), katarina, (int)SkillType.RSpell, here,
                                          Direction(here, PositionOf(target)), RSpeed) {
                    Homing = target,
                    Range = 30f,
                    Width = 0.1f,
                    CanHit = unit => unit == target,
                    StopAtFirstHit = true,
                    OnHit =
                        (shot, unit) =>
                    {
                        CombatMath.ApplyDamage(room, katarina, unit, damage, DamageTypes.Physical,
                                               (int)SkillType.RSpell);

                        if (unit is Player victim && victim.Info.Hp > 0)
                        {
                            victim.Status.SetGrievousWounds(grievous, grievousSeconds);
                            room.Broadcast(
                                new S_Buff { TargetId = victim.Info.ObjectId, SourceId = katarina.Info.ObjectId,
                                             BuffType = BUFF_TYPE.BuffGrievousWounds, SkillId = (int)SkillType.RSpell,
                                             Amount = grievous, Duration = (float)grievousSeconds });
                        }
                    },
                };
                room.AddSkillshot(blade);
            }

            if (++thrown < ticks)
                room.PushAfter(interval, volley);
        };
        volley();

        return true;
    }

    public static void StopChannel(GameRoom room, Player katarina)
    {
        if (!katarina.Status.KatarinaChanneling)
            return;
        katarina.Status.KatarinaREndsAt = 0;
        Buffs.State(room, katarina, BUFF_TYPE.BuffKatarinaR, (int)SkillType.RSpell, 0f, 0.0);
    }
}
