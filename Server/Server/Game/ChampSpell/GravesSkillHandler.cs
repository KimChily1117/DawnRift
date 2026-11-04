using Google.Protobuf.Protocol;
using Server.Game.Movement;
using Server.Game.Objects;
using Server.Game.Room;
using System;
using System.Collections.Generic;
using System.Linq;
using Vec3 = System.Numerics.Vector3;

public class GravesSkillHandler : IChampionSkillHandler
{
    public const int Pellets = 4;
    public const float PelletCone = 30f;
    public const float PelletSpeed = 30f;
    public const float PelletRangeBonus = 1.5f;
    public const float ExtraPelletDamage = 0.3f;
    public const double ReloadSeconds = 2.0;
    public const float QSpeed = 30f;
    public const float QWidth = 0.4f;
    public const double QBlastDelay = 1.0;
    public const float QBlastWidth = 1.0f;
    public const float SmokeSpeed = 16.5f;
    public const float SmokeSlowRefresh = 0.5f;
    public const float RRange = 10f;
    public const float RSpeed = 21f;
    public const float RWidth = 1.0f;
    public const float RConeRange = 8f;
    public const float RConeDegrees = 60f;
    public const float RecoilSpeed = 16f;

    public static double? CastLockSeconds(int skillId) => skillId == 3 || skillId == 4 ? (double?)null : 0.25;

    public bool HandleSkill(GameRoom room, GameObject caster, C_SkillCast skillPacket)
    {
        if (!(caster is Player graves) || graves.Info == null || graves.Info.Hp <= 0)
            return false;

        switch (skillPacket.SkillId)
        {
        case (int)SkillType.Gerneral:
            return BasicAttackHandler.Handle(room, caster, skillPacket);
        case (int)SkillType.QSpell:
            return EndOfTheLine(room, graves, skillPacket);
        case (int)SkillType.WSpell:
            return SmokeScreen(room, graves, skillPacket);
        case (int)SkillType.ESpell:
            return Quickdraw(room, graves, skillPacket);
        case (int)SkillType.RSpell:
            return CollateralDamage(room, graves, skillPacket);
        default:
            return false;
        }
    }

    private static SpellData Spell(GameObject caster,
                                   SkillType slot) => CombatMath.DataOf(caster)?.Spell(slot) ?? SpellData.Empty;

    private static bool Enemy(Player graves,
                              GameObject unit) => unit?.Info != null
                                                  && unit.Info.Hp > 0 && unit.Info.TeamId != graves.Info.TeamId &&
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

    private static Vec3 Rotate(Vec3 direction, float degrees)
    {
        float angle = degrees * MathF.PI / 180f;

        return new Vec3(direction.X * MathF.Cos(angle) - direction.Z * MathF.Sin(angle), 0f,
                        direction.X * MathF.Sin(angle) + direction.Z * MathF.Cos(angle));
    }

    private static void Announce(GameRoom room, Player graves, SkillType skill, Vec3 origin, Vec3 direction,
                                 Vec3? center = null, float radius = 0f)
    {
        room.Broadcast(new S_SkillResult {
            CasterId = graves.Info.ObjectId,
            SkillId = (int)skill,
            CastOrigin = origin.ToProtoVector3(),
            CastDirection = direction.ToProtoVector3(),
            IsAreaSkill = center.HasValue,
            CenterPos = (center ?? origin).ToProtoVector3(),
            AreaRadius = radius,
        });
    }

    // ---- passive: the shotgun ----

    private static void SendShells(GameRoom room, Player graves) => Buffs.State(
        room, graves, BUFF_TYPE.BuffGravesShells, Buffs.PassiveSkillId, graves.Status.GravesShells,
        graves.Status.GravesShells == 0 ? Math.Max(0.0, (graves.Status.GravesReloadEndsAt - StatusEffects.Now) /
                                                            (double)System.Diagnostics.Stopwatch.Frequency)
                                        : 0.0);

    public static void FireShotgun(GameRoom room, Player graves, GameObject target, float damage)
    {
        Vec3 origin = graves.Info.Position.ToNumericsVector3();
        Vec3 direction = target.Info.Position.ToNumericsVector3() - origin;
        direction.Y = 0f;
        direction = direction.LengthSquared() > 1e-4f ? Vec3.Normalize(direction) : Vec3.UnitX;
        float range = ChampionCombatStats.For(graves.Info.ChampType).AttackRange + PelletRangeBonus;
        var hits = new Dictionary<ulong, int>();
        float step = PelletCone / (Pellets - 1);

        for (int i = 0; i < Pellets; ++i)
        {
            var pellet = new Skillshot(room.GenerateProjectileId(), graves, (int)SkillType.Gerneral, origin,
                                       Rotate(direction, -PelletCone * 0.5f + step * i), PelletSpeed) {
                Range = range,
                Width = 0.1f,
                StopAtFirstHit = true,
                CanHit = unit => Enemy(graves, unit),
                OnHit =
                    (shot, unit) =>
                {
                    hits.TryGetValue(unit.Info.ObjectId, out int count);
                    hits[unit.Info.ObjectId] = count + 1;
                    CombatMath.ApplyDamage(room, graves, unit, count == 0 ? damage : damage * ExtraPelletDamage,
                                           DamageTypes.Physical, (int)SkillType.Gerneral);
                    // Every pellet on an enemy speeds up Quickdraw.
                    if (Progression.SkillRank(graves, SkillType.ESpell) > 0)
                    {
                        double left = graves.ReduceSkillCooldown(
                            (int)SkillType.ESpell, Spell(graves, SkillType.ESpell).Value("CooldownPerHit", 1, 0.5f));
                        graves.Session?.Send(
                            new S_SkillCooldown { SkillId = (int)SkillType.ESpell, Duration = (float)left });
                    }
                },
            };
            room.AddSkillshot(pellet);
        }

        // Two shells, then the reload (faster with attack speed).
        graves.Status.GravesShells = Math.Max(0, graves.Status.GravesShells - 1);

        if (graves.Status.GravesShells == 0)
            graves.Status.GravesReloadEndsAt =
                StatusEffects.After(ReloadSeconds / (1.0 + graves.Status.AttackSpeedBonus));
        SendShells(room, graves);
    }

    public static void Update(GameRoom room, Player graves, float deltaTime)
    {
        if (graves.Status.GravesShells == 0 && !graves.Status.GravesReloading)
        {
            graves.Status.GravesShells = 2;
            SendShells(room, graves);
        }

        if (graves.Status.GravesStacks > 0 && graves.Status.BonusArmor <= 0f)
        {
            graves.Status.GravesStacks = 0;
            Buffs.State(room, graves, BUFF_TYPE.BuffGravesStacks, (int)SkillType.ESpell, 0, 0.0);
        }
    }

    // ---- Q ----

    private bool EndOfTheLine(GameRoom room, Player graves, C_SkillCast packet)
    {
        if (!Aim(graves, packet, out Vec3 origin, out Vec3 direction, out _) ||
            !CombatActionMovement.TryBegin(room, graves, packet.SkillId))
            return false;
        SpellData q = Spell(graves, SkillType.QSpell);
        int rank = Progression.SkillRank(graves, SkillType.QSpell);
        float damage = q.Value("BaseDamage", rank, 50f) +
                       q.Value("FirstADRatio", rank, 0.8f) * CombatMath.BonusAttackDamage(graves);
        float blast = q.Value("BaseDetonationDamage", rank, 80f);
        float range = q.CastRangeUnits > 0f ? q.CastRangeUnits : 8f;

        double wallDelay = q.Value("TerrainCollisionDelay", rank, 0.2f);
        Announce(room, graves, SkillType.QSpell, origin, direction);
        var bullet =
            new Skillshot(room.GenerateProjectileId(), graves, (int)SkillType.QSpell, origin, direction, QSpeed) {
                Range = range,
                Width = QWidth,
                StopAtWalls = true,
                CanHit = unit => Enemy(graves, unit),
                OnHit = (shot, unit) =>
                    CombatMath.ApplyDamage(room, graves, unit, damage, DamageTypes.Physical, (int)SkillType.QSpell),
            };
        bullet.OnEnd = shot =>
        {
            Vec3 end = shot.Position;

            room.PushAfter((int)Math.Round((shot.HitWall ? wallDelay : QBlastDelay) * 1000.0),
                           () =>
                           {
                               if (graves.Info == null)
                                   return;
                               room.Broadcast(new S_SkillResult {
                                   CasterId = graves.Info.ObjectId,
                                   SkillId = (int)SkillType.QSpell,
                                   IsAreaSkill = true,
                                   AreaRadius = QBlastWidth,
                                   FollowUp = true,
                                   CenterPos = end.ToProtoVector3(),
                                   CastOrigin = origin.ToProtoVector3(),
                                   CastDirection = direction.ToProtoVector3(),
                               });
                               Vec3 middle = (origin + end) * 0.5f;
                               float half = Vec3.Distance(origin, end) * 0.5f;

                               foreach (GameObject unit in room.GetObjectsInRange(middle, half + QBlastWidth + 1f))
                               {
                                   if (!Enemy(graves, unit))
                                       continue;

                                   if (DistanceToSegment(unit.Info.Position.ToNumericsVector3(), origin, end) <=
                                       QBlastWidth + Skillshot.RadiusOf(unit))
                                       CombatMath.ApplyDamage(room, graves, unit, blast, DamageTypes.Physical,
                                                              (int)SkillType.QSpell);
                               }
                           });
        };
        room.AddSkillshot(bullet);

        return true;
    }

    private static float DistanceToSegment(Vec3 p, Vec3 a, Vec3 b)
    {
        Vec3 ab = b - a, ap = p - a;
        ab.Y = ap.Y = 0f;
        float t = ab.LengthSquared() > 1e-6f ? Math.Clamp(Vec3.Dot(ap, ab) / ab.LengthSquared(), 0f, 1f) : 0f;
        Vec3 d = p - (a + ab * t);
        d.Y = 0f;

        return d.Length();
    }

    // ---- W ----

    private bool SmokeScreen(GameRoom room, Player graves, C_SkillCast packet)
    {
        if (!Aim(graves, packet, out Vec3 origin, out Vec3 direction, out float distance) ||
            !CombatActionMovement.TryBegin(room, graves, packet.SkillId))
            return false;
        SpellData w = Spell(graves, SkillType.WSpell);
        int rank = Progression.SkillRank(graves, SkillType.WSpell);
        float range = w.CastRangeUnits > 0f ? w.CastRangeUnits : 9.5f;
        Vec3 landing = origin + direction * Math.Min(distance, range);
        float radius = w.Value("SmokeRadius", rank, 200f) * ChampionData.UnitScale + 0.5f;
        float damage = w.Value("BaseDamage", rank, 60f) + 0.6f * CombatMath.AbilityPower(graves);
        float slow = w.Value("SlowAmount", rank, 50f) / 100f;

        double seconds = w.Value("SmokeDuration", rank, 4f);
        Announce(room, graves, SkillType.WSpell, origin, direction);
        var canister =
            new Skillshot(room.GenerateProjectileId(), graves, (int)SkillType.WSpell, origin, direction, SmokeSpeed) {
                Range = Math.Min(distance, range),
                Width = 0f,
                CanHit = unit => false,
            };
        canister.OnEnd = shot =>
        {
            room.Broadcast(new S_SkillResult {
                CasterId = graves.Info.ObjectId,
                SkillId = (int)SkillType.WSpell,
                IsAreaSkill = true,
                AreaRadius = radius,
                FollowUp = true,
                CenterPos = landing.ToProtoVector3(),
                CastOrigin = landing.ToProtoVector3(),
                CastDirection = direction.ToProtoVector3(),
            });

            foreach (GameObject unit in room.GetObjectsInRange(landing, radius))
                if (Enemy(graves, unit))
                    CombatMath.ApplyDamage(room, graves, unit, damage, DamageTypes.Magic, (int)SkillType.WSpell);
            long until = StatusEffects.After(seconds);
            Action cloud = null;
            cloud = () =>
            {
                foreach (GameObject unit in room.GetObjectsInRange(landing, radius))
                {
                    if (!Enemy(graves, unit) || !(unit is Player victim))
                        continue;
                    Buffs.Slow(room, graves, victim, slow, SmokeSlowRefresh, (int)SkillType.WSpell);

                    if (Progression.IsChampion(victim))
                    {
                        victim.Status.NearsightEndsAt = StatusEffects.After(SmokeSlowRefresh);
                        room.Broadcast(new S_Buff { TargetId = victim.Info.ObjectId, SourceId = graves.Info.ObjectId,
                                                    BuffType = BUFF_TYPE.BuffNearsight, SkillId = (int)SkillType.WSpell,
                                                    Amount = Server.Game.Vision.VisionSystem.NearsightRange,
                                                    Duration = SmokeSlowRefresh });
                    }
                }

                if (StatusEffects.Now < until)
                    room.PushAfter(250, cloud);
            };
            cloud();
        };
        room.AddSkillshot(canister);

        return true;
    }

    // ---- E ----

    private bool Quickdraw(GameRoom room, Player graves, C_SkillCast packet)
    {
        if (!Aim(graves, packet, out Vec3 origin, out Vec3 direction, out float distance))
            return false;
        SpellData e = Spell(graves, SkillType.ESpell);
        int rank = Progression.SkillRank(graves, SkillType.ESpell);
        float min = e.Value("DashMinDistance", rank, 275f) * ChampionData.UnitScale;
        float max = e.Value("DashMaxDistance", rank, 375f) * ChampionData.UnitScale;
        float speed = e.Value("DashSpeed", rank, 750f) * ChampionData.UnitScale;
        Vec3 end = Dash.Clip(room, origin, direction, Math.Clamp(distance, min, max));
        // Toward an enemy champion: two stacks.
        bool towardChampion =
            room.GetObjectsInRange(end, 4f).Any(unit => Enemy(graves, unit) && Progression.IsChampion(unit));
        int maxStacks = (int)e.Value("MaxStacks", rank, 8f);
        graves.Status.GravesStacks = Math.Min(maxStacks, graves.Status.GravesStacks + (towardChampion ? 2 : 1));

        double seconds = e.Value("BuffDuration", rank, 4f);
        float armor = e.Value("ArmorPerStack", rank, 7f) * graves.Status.GravesStacks;

        graves.Status.SetBonusResists(armor, armor, seconds);
        Buffs.State(room, graves, BUFF_TYPE.BuffGravesStacks, (int)SkillType.ESpell, graves.Status.GravesStacks,
                    seconds);
        // A shell back in the gun.
        graves.Status.GravesShells = Math.Min(2, graves.Status.GravesShells + 1);
        graves.Status.GravesReloadEndsAt = 0;
        SendShells(room, graves);
        Announce(room, graves, SkillType.ESpell, origin, direction);
        Dash.Start(room, graves, end, speed);

        return true;
    }

    // ---- R ----

    private bool CollateralDamage(GameRoom room, Player graves, C_SkillCast packet)
    {
        if (!Aim(graves, packet, out Vec3 origin, out Vec3 direction, out _))
            return false;
        SpellData r = Spell(graves, SkillType.RSpell);
        int rank = Progression.SkillRank(graves, SkillType.RSpell);
        float damage = r.Value("RBaseDamage", rank, 275f) + 1.5f * CombatMath.BonusAttackDamage(graves);
        float coneDamage = r.Value("RFalloffDamage", rank, 200f) + 1.2f * CombatMath.BonusAttackDamage(graves);
        float knockback = r.Value("RKnockbackDistance", rank, 400f) * ChampionData.UnitScale;
        Announce(room, graves, SkillType.RSpell, origin, direction);
        ulong primary = 0;
        var shell =
            new Skillshot(room.GenerateProjectileId(), graves, (int)SkillType.RSpell, origin, direction, RSpeed) {
                Range = RRange,
                Width = RWidth,
                StopAtFirstHit = true,
                ChampionsOnly = true,
                OnHit =
                    (shot, unit) =>
                {
                    primary = unit.Info.ObjectId;
                    CombatMath.ApplyDamage(room, graves, unit, damage, DamageTypes.Physical, (int)SkillType.RSpell);
                },
            };
        shell.OnEnd = shot =>
        {
            // The blast fans out behind where it stopped.
            Vec3 at = shot.Position;
            room.Broadcast(new S_SkillResult {
                CasterId = graves.Info.ObjectId,
                SkillId = (int)SkillType.RSpell,
                IsAreaSkill = true,
                AreaRadius = RConeRange,
                FollowUp = true,
                CenterPos = at.ToProtoVector3(),
                CastOrigin = at.ToProtoVector3(),
                CastDirection = direction.ToProtoVector3(),
            });
            float cos = MathF.Cos(RConeDegrees * 0.5f * MathF.PI / 180f);

            foreach (GameObject unit in room.GetObjectsInRange(at, RConeRange))
            {
                if (!Enemy(graves, unit) || unit.Info.ObjectId == primary)
                    continue;
                Vec3 to = unit.Info.Position.ToNumericsVector3() - at;
                to.Y = 0f;

                if (to.LengthSquared() < 0.01f || Vec3.Dot(Vec3.Normalize(to), direction) >= cos)
                    CombatMath.ApplyDamage(room, graves, unit, coneDamage, DamageTypes.Physical, (int)SkillType.RSpell);
            }
        };
        room.AddSkillshot(shell);
        // The recoil.
        Dash.Start(room, graves, Dash.Clip(room, origin, -direction, knockback), RecoilSpeed);

        return true;
    }
}
