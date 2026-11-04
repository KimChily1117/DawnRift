// 가렌 스킬의 서버 판정.
using Google.Protobuf.Protocol;
using Server.Game.Objects;
using Server.Game.Movement;
using Server.Game.Room;
using System;
using System.Collections.Generic;

public class GarenSkillHandler : IChampionSkillHandler
{
    private const float RangeTolerance = 0.75f;

    private const float JudgmentRadius = 3.25f;
    private const float DemacianJusticeRange = 4.0f;

    public bool HandleSkill(GameRoom room, GameObject caster, C_SkillCast skillPacket)
    {
        switch (skillPacket.SkillId)
        {
        case (int)SkillType.Gerneral:
            return BasicAttackHandler.Handle(room, caster, skillPacket);
        case (int)SkillType.QSpell:
            return HandleGarenQ(room, caster, skillPacket);
        case (int)SkillType.WSpell:
            return HandleGarenW(room, caster);
        case (int)SkillType.ESpell:
            return HandleGarenE(room, caster);
        case (int)SkillType.RSpell:
            return HandleGarenR(room, caster, skillPacket);
        default:
            Console.WriteLine("[Server] Unknown Garen skill.");

            return false;
        }
    }

    private bool HandleGarenQ(GameRoom room, GameObject caster, C_SkillCast skillPacket)
    {
        GameObject target = room.FindObject(skillPacket.TargetId);
        SpellData q = Spell(caster, SkillType.QSpell);
        int rank = Progression.SkillRank(caster, SkillType.QSpell);
        float range = Math.Max(q.CastRangeUnits, ChampionCombatStats.For(caster.Info.ChampType).AttackRange);

        if (!IsValidTarget(caster, target) || DistanceXZ(caster, target) > range + RangeTolerance)
            return false;

        if (!CombatActionMovement.TryBegin(room, caster, skillPacket.SkillId))
            return false;

        if (caster is Player garen)
            Buffs.MoveSpeed(room, garen, garen, q.Value("MovementSpeedAmount", rank, 0.35f),
                            q.Value("MovementSpeedDuration", rank, 1.4f), (int)SkillType.QSpell);
        float damage =
            q.Value("BaseDamage", rank, 30f) + q.Value("tADRatio", rank, 1.5f) * CombatMath.AttackDamage(caster);
        BroadcastHit(room, caster, target, SkillType.QSpell);
        CombatMath.ApplyDamage(room, caster, target, damage, DamageTypes.Physical, (int)SkillType.QSpell);

        if (target is Player victim)
            Buffs.Silence(room, caster, victim, q.Value("SilenceDuration", rank, 1.5f), (int)SkillType.QSpell);

        return true;
    }

    private bool HandleGarenW(GameRoom room, GameObject caster)
    {
        if (!(caster is Player garen) || garen.Info == null || garen.Info.Hp <= 0)
            return false;
        SpellData w = Spell(caster, SkillType.WSpell);
        int rank = Progression.SkillRank(caster, SkillType.WSpell);

        const float bonusHealth = 0f;
        float upfront = w.Value("UpfrontDuration", rank, 0.75f);
        room.Broadcast(new S_SkillResult { CasterId = garen.Info.ObjectId, SkillId = (int)SkillType.WSpell });
        Buffs.DamageReduction(room, garen, garen, w.Value("DRPercent", rank, 0.25f), w.Value("DRDuration", rank, 4f),
                              (int)SkillType.WSpell);
        Buffs.Shield(room, garen, garen,
                     w.Value("BaseShield", rank, 65f) + w.Value("ShieldHealthRatio", rank, 0.18f) * bonusHealth,
                     upfront, (int)SkillType.WSpell);
        Buffs.Tenacity(room, garen, garen, w.Value("UpfrontTenacity", rank, 0.6f), upfront, (int)SkillType.WSpell);

        return true;
    }

    private bool HandleGarenE(GameRoom room, GameObject caster)
    {
        if (!(caster is Player garen) || !CombatActionMovement.TryBegin(room, caster, (int)SkillType.ESpell))
            return false;
        SpellData e = Spell(caster, SkillType.ESpell);
        int rank = Progression.SkillRank(caster, SkillType.ESpell);
        int tickCount = Math.Max(1, (int)e.Value("NumTicks", rank, 7f));
        float duration = e.Value("Duration", rank, 3f);
        int tickIntervalMs = (int)Math.Round(duration * 1000f / tickCount);
        float damagePerTick = e.Value("BaseDamagePerTick", rank, 4f) +
                              e.Value("ADRatioPerTick", rank, 0.4f) * CombatMath.AttackDamage(caster);
        float nearestBonus = e.Value("NearestEnemyBonus", rank, 0.25f);
        int stacksToShred = (int)e.Value("StacksToShred", rank, 6f);
        float shredAmount = e.Value("ShredAmount", rank, 0.25f);
        float shredDuration = e.Value("ShredDuration", rank, 6f);
        garen.Status.JudgmentHits.Clear();

        room.Broadcast(new S_SkillResult { CasterId = caster.Info.ObjectId, SkillId = (int)SkillType.ESpell });

        int remainingTicks = tickCount;
        Action applyTick = null;
        applyTick = () =>
        {
            if (caster.Info == null || caster.Info.Hp <= 0 ||
                !ReferenceEquals(room.FindObject(caster.Info.ObjectId), caster))
                return;

            var targets = new List<GameObject>();

            foreach (GameObject obj in room.GetObjectsInRange(caster.Info.Position.ToNumericsVector3(), JudgmentRadius))
                if (IsValidTarget(caster, obj))
                    targets.Add(obj);
            GameObject nearest = null;
            float nearestDistance = float.MaxValue;

            foreach (GameObject obj in targets)
            {
                float distance = DistanceXZ(caster, obj);

                if (distance < nearestDistance)
                {
                    nearest = obj;
                    nearestDistance = distance;
                }
            }

            foreach (GameObject obj in targets)
            {
                float damage = ReferenceEquals(obj, nearest) && targets.Count > 1 ? damagePerTick * (1f + nearestBonus)
                                                                                  : damagePerTick;
                CombatMath.ApplyDamage(room, caster, obj, damage, DamageTypes.Physical, (int)SkillType.ESpell);
                garen.Status.JudgmentHits.TryGetValue(obj.Info.ObjectId, out int hits);
                garen.Status.JudgmentHits[obj.Info.ObjectId] = ++hits;

                if (hits == stacksToShred && obj is Player shredded)
                    Buffs.ArmorShred(room, caster, shredded, shredAmount, shredDuration, (int)SkillType.ESpell);
            }

            remainingTicks--;

            if (remainingTicks > 0)
                room.PushAfter(tickIntervalMs, applyTick);
        };

        applyTick();

        return true;
    }

    private bool HandleGarenR(GameRoom room, GameObject caster, C_SkillCast skillPacket)
    {
        GameObject target = room.FindObject(skillPacket.TargetId);

        if (!IsValidTarget(caster, target) || DistanceXZ(caster, target) > DemacianJusticeRange + RangeTolerance)
            return false;

        if (!CombatActionMovement.TryBegin(room, caster, skillPacket.SkillId))
            return false;
        SpellData r = Spell(caster, SkillType.RSpell);
        int rank = Progression.SkillRank(caster, SkillType.RSpell);
        float missingHealth = Math.Max(0, target.Info.MaxHp - target.Info.Hp);
        float damage = r.Value("BaseDamage", rank, 150f) + r.Value("ExecuteDamage", rank, 0.25f) * missingHealth;
        BroadcastHit(room, caster, target, SkillType.RSpell);
        CombatMath.ApplyDamage(room, caster, target, damage, DamageTypes.True, (int)SkillType.RSpell);

        return true;
    }

    private static SpellData Spell(GameObject caster,
                                   SkillType slot) => CombatMath.DataOf(caster)?.Spell(slot) ?? SpellData.Empty;

    private static void BroadcastHit(GameRoom room, GameObject caster, GameObject target, SkillType skill)
    {
        room.Broadcast(new S_SkillResult { CasterId = caster.Info.ObjectId, SkillId = (int)skill,
                                           HitObjects = { target.Info.ObjectId } });
    }

    private static float DistanceXZ(GameObject a, GameObject b)
    {
        var delta = a.Info.Position.ToNumericsVector3() - b.Info.Position.ToNumericsVector3();
        delta.Y = 0f;

        return delta.Length();
    }

    private static bool IsValidTarget(GameObject caster, GameObject target)
    {
        return target?.Info != null && caster?.Info != null && target.Info.Hp > 0 &&
               target.Info.TeamId != caster.Info.TeamId;
    }
}
