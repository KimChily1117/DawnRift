using Google.Protobuf.Protocol;
using Server.Game.Movement;
using Server.Game.Objects;
using Server.Game.Room;
using System;
using System.Collections.Generic;
using System.Linq;
using Vec3 = System.Numerics.Vector3;

public class SonaSkillHandler : IChampionSkillHandler
{
    public const float QMissileSpeed = 15f;
    public const float CrescendoRange = 10f;
    public const float CrescendoWidth = 1.4f;
    public const float CrescendoSpeed = 24f;
    public const float ChordMin = 20f, ChordMax = 240f;
    public const float StaccatoBonus = 0.5f;
    public const float DiminuendoAmount = 0.25f;
    public const float TempoSlow = 0.4f;
    public const double TempoSlowSeconds = 2.0;
    public const int ChordSpells = 3;

    public static double? CastLockSeconds(int skillId) => skillId == 4 ? 0.25 : (double?)null;

    public bool HandleSkill(GameRoom room, GameObject caster, C_SkillCast skillPacket)
    {
        if (!(caster is Player sona) || sona.Info == null || sona.Info.Hp <= 0)
            return false;
        bool cast;

        switch (skillPacket.SkillId)
        {
        case (int)SkillType.Gerneral:
            return BasicAttackHandler.Handle(room, caster, skillPacket);
        case (int)SkillType.QSpell:
            cast = HymnOfValor(room, sona);
            break;
        case (int)SkillType.WSpell:
            cast = AriaOfPerseverance(room, sona);
            break;
        case (int)SkillType.ESpell:
            cast = SongOfCelerity(room, sona);
            break;
        case (int)SkillType.RSpell:
            cast = Crescendo(room, sona, skillPacket);
            break;
        default:
            return false;
        }

        if (cast)
            CountSpell(room, sona);

        return cast;
    }

    private static SpellData Spell(GameObject caster,
                                   SkillType slot) => CombatMath.DataOf(caster)?.Spell(slot) ?? SpellData.Empty;

    private static bool Enemy(Player sona,
                              GameObject unit) => unit?.Info != null
                                                  && unit.Info.Hp > 0 && unit.Info.TeamId != sona.Info.TeamId &&
                                                  !(unit is Ward) &&
                                                  !(unit is MapUnit structure && structure.IsStructure);

    private static IEnumerable<Player> AlliedChampions(GameRoom room, Player sona, float range) =>
        room.GetObjectsInRange(sona.Info.Position.ToNumericsVector3(), range)
            .OfType<Player>()
            .Where(p => Progression.IsChampion(p) && p.Info != null && p.Info.Hp > 0 &&
                        p.Info.TeamId == sona.Info.TeamId);

    // ---- melodies ----

    private static void StartSong(GameRoom room, Player sona, SkillType skill)
    {
        SpellData spell = Spell(sona, skill);
        int rank = Math.Max(1, Progression.SkillRank(sona, skill));

        double seconds = spell.Value("AuraDuration", rank, 3f);
        float range = spell.Value("AuraRange", rank, 400f) * ChampionData.UnitScale;
        sona.Status.SonaSong = (int)skill;
        sona.Status.SonaAuraEndsAt = StatusEffects.After(seconds);
        Buffs.State(room, sona, BUFF_TYPE.BuffSonaAura, (int)skill, (int)skill, seconds);

        foreach (Player ally in AlliedChampions(room, sona, range))
        {
            switch (skill)
            {
            case SkillType.QSpell:
                ally.Status.SetOnHitBonus(spell.Value("BaseOnHitDamage", rank, 10f),
                                          spell.Value("OnHitDuration", rank, 5f), sona.Info.ObjectId);
                break;
            case SkillType.WSpell:
                Buffs.Shield(room, sona, ally, spell.Value("BaseShield", rank, 25f),
                             spell.Value("ShieldDuration", rank, 1.5f), (int)skill);
                break;
            case SkillType.ESpell:
                if (!ReferenceEquals(ally, sona))
                    Buffs.MoveSpeed(room, sona, ally, spell.Value("AllyBaseMovementSpeed", rank, 0.1f),
                                    spell.Value("AllyMovementSpeedDuration", rank, 3f), (int)skill);
                break;
            }
        }
    }

    private static void CountSpell(GameRoom room, Player sona)
    {
        sona.Status.SonaChord = Math.Min(ChordSpells, sona.Status.SonaChord + 1);
        Buffs.State(room, sona, BUFF_TYPE.BuffSonaChord, Buffs.PassiveSkillId, sona.Status.SonaChord, 30.0);
    }

    public static int BasicAttackVariant(Player sona) => sona.Status.SonaChord >= ChordSpells
                                                             ? Math.Clamp(sona.Status.SonaSong, 1, 3)
                                                             : 0;

    public static void OnBasicAttackHit(GameRoom room, Player sona, GameObject target)
    {
        if (sona.Status.SonaChord < ChordSpells || target?.Info == null)
            return;
        sona.Status.SonaChord = 0;
        Buffs.State(room, sona, BUFF_TYPE.BuffSonaChord, Buffs.PassiveSkillId, 0, 0.0);
        float damage = ChordMin + (ChordMax - ChordMin) * (Math.Clamp(sona.Progress.Level, 1, 18) - 1) / 17f +
                       0.2f * CombatMath.AbilityPower(sona);

        if (sona.Status.SonaSong == (int)SkillType.QSpell)
            damage *= 1f + StaccatoBonus;
        CombatMath.ApplyDamage(room, sona, target, damage, DamageTypes.Magic, Buffs.PassiveSkillId);

        if (!(target is Player victim) || victim.Info.Hp <= 0)
            return;

        if (sona.Status.SonaSong == (int)SkillType.WSpell)
        {
            double seconds = Spell(sona, SkillType.WSpell).Value("DiminuendoDuration", 1, 3f);
            victim.Status.Weaken(DiminuendoAmount, seconds);
            room.Broadcast(new S_Buff { TargetId = victim.Info.ObjectId, SourceId = sona.Info.ObjectId,
                                        BuffType = BUFF_TYPE.BuffWeakened, SkillId = Buffs.PassiveSkillId,
                                        Amount = DiminuendoAmount, Duration = (float)seconds });
        }
        else if (sona.Status.SonaSong == (int)SkillType.ESpell)
            Buffs.Slow(room, sona, victim, TempoSlow, TempoSlowSeconds, Buffs.PassiveSkillId);
    }

    public static void Update(GameRoom room, Player sona, float deltaTime)
    {
        if (sona.Status.SonaSong != 0 && StatusEffects.Now >= sona.Status.SonaAuraEndsAt)
            sona.Status.SonaAuraEndsAt = 0;
    }

    // ---- Q, W, E, R ----

    private bool HymnOfValor(GameRoom room, Player sona)
    {
        SpellData q = Spell(sona, SkillType.QSpell);
        int rank = Progression.SkillRank(sona, SkillType.QSpell);
        float range = (q.CastRangeUnits > 0f ? q.CastRangeUnits : 8.25f);
        float damage = q.Value("BaseDamage", rank, 50f) + 0.4f * CombatMath.AbilityPower(sona);
        int count = (int)q.Value("EnemiesToHit", rank, 2f);
        Vec3 at = sona.Info.Position.ToNumericsVector3();
        List<GameObject> targets =
            room.GetObjectsInRange(at, range)
                .Where(unit => Enemy(sona, unit) && !room.Vision.IsHiddenFrom(sona.Info.TeamId, unit.Info.ObjectId))
                .OrderBy(unit => Progression.IsChampion(unit) ? 0 : 1)
                .ThenBy(unit => Vec3.DistanceSquared(unit.Info.Position.ToNumericsVector3(), at))
                .Take(count)
                .ToList();
        room.Broadcast(new S_SkillResult { CasterId = sona.Info.ObjectId, SkillId = (int)SkillType.QSpell });

        foreach (GameObject target in targets)
        {
            ulong id = room.GenerateProjectileId();
            room.AddProjectile(
                new Projectile(id, sona, target, QMissileSpeed, damage, DamageTypes.Magic, (int)SkillType.QSpell));
            room.Broadcast(new S_ProjectileSpawn {
                ProjectileId = id,
                CasterId = sona.Info.ObjectId,
                TargetId = target.Info.ObjectId,
                StartPos = sona.Info.Position,
                EndPos = target.Info.Position,
                Speed = QMissileSpeed,
                SkillId = (int)SkillType.QSpell,
            });
        }

        StartSong(room, sona, SkillType.QSpell);

        return true;
    }

    private bool AriaOfPerseverance(GameRoom room, Player sona)
    {
        SpellData w = Spell(sona, SkillType.WSpell);
        int rank = Progression.SkillRank(sona, SkillType.WSpell);
        float heal = w.Value("BaseHeal", rank, 30f) + 0.3f * CombatMath.AbilityPower(sona);
        float range = w.Value("HealRange", rank, 1000f) * ChampionData.UnitScale;
        Player ally = AlliedChampions(room, sona, range)
                          .Where(p => !ReferenceEquals(p, sona) && p.Info.Hp < p.Info.MaxHp)
                          .OrderBy(p => p.Info.Hp / (float)Math.Max(1, p.Info.MaxHp))
                          .FirstOrDefault();
        room.Broadcast(new S_SkillResult {
            CasterId = sona.Info.ObjectId,
            SkillId = (int)SkillType.WSpell,
            HitObjects = { ally != null ? new[] { sona.Info.ObjectId, ally.Info.ObjectId }
                                        : new[] { sona.Info.ObjectId } },
        });

        foreach (Player healed in ally != null ? new[] { sona, ally } : new[] { sona })
            Buffs.Heal(room, sona, healed, heal, (int)SkillType.WSpell);
        StartSong(room, sona, SkillType.WSpell);

        return true;
    }

    private bool SongOfCelerity(GameRoom room, Player sona)
    {
        SpellData e = Spell(sona, SkillType.ESpell);
        int rank = Progression.SkillRank(sona, SkillType.ESpell);
        room.Broadcast(new S_SkillResult { CasterId = sona.Info.ObjectId, SkillId = (int)SkillType.ESpell });

        Buffs.MoveSpeed(room, sona, sona, e.Value("SelfBaseMovementSpeed", rank, 0.2f),
                        e.Value("SelfMovementSpeedDurationMin", rank, 3f), (int)SkillType.ESpell);
        StartSong(room, sona, SkillType.ESpell);

        return true;
    }

    private bool Crescendo(GameRoom room, Player sona, C_SkillCast packet)
    {
        if (packet.TargetPos == null)
            return false;
        Vec3 origin = sona.Info.Position.ToNumericsVector3();
        Vec3 direction = packet.TargetPos.ToNumericsVector3() - origin;
        direction.Y = 0f;

        if (direction.LengthSquared() < 1e-4f || !CombatActionMovement.TryBegin(room, sona, packet.SkillId))
            return false;
        direction = Vec3.Normalize(direction);
        SpellData r = Spell(sona, SkillType.RSpell);
        int rank = Progression.SkillRank(sona, SkillType.RSpell);
        float damage = r.Value("BaseDamage", rank, 150f) + 0.5f * CombatMath.AbilityPower(sona);

        double stun = r.Value("StunDuration", rank, 1.5f);
        room.Broadcast(new S_SkillResult {
            CasterId = sona.Info.ObjectId,
            SkillId = (int)SkillType.RSpell,
            CastOrigin = origin.ToProtoVector3(),
            CastDirection = direction.ToProtoVector3(),
        });
        room.AddSkillshot(
            new Skillshot(room.GenerateProjectileId(), sona, (int)SkillType.RSpell, origin, direction, CrescendoSpeed) {
                Range = CrescendoRange,
                Width = CrescendoWidth,
                CanHit = unit => Enemy(sona, unit),
                OnHit =
                    (shot, unit) =>
                {
                    CombatMath.ApplyDamage(room, sona, unit, damage, DamageTypes.Magic, (int)SkillType.RSpell);

                    if (unit is Player victim)
                        Buffs.Stun(room, sona, victim, stun, (int)SkillType.RSpell);
                },
            });

        return true;
    }
}
