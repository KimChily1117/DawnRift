// 애니 스킬의 서버 판정.
using Google.Protobuf.Protocol;
using Server.Game;
using Server.Game.Movement;
using Server.Game.Objects;
using Server.Game.Room;
using System;
using Vec3 = System.Numerics.Vector3;

public class AnnieSkillHandler : IChampionSkillHandler
{
    private const float RangeTolerance = 0.75f;
    private const float WHalfAngleDegrees = 25.0f;

    private const int PyromaniaStacks = 4;
    private const double PyromaniaStunSeconds = 1.25;
    private const float ECastRange = 8.0f;

    public bool HandleSkill(GameRoom room, GameObject caster, C_SkillCast skillPacket)
    {
        switch (skillPacket.SkillId)
        {
        case (int)SkillType.Gerneral:
            return BasicAttackHandler.Handle(room, caster, skillPacket);
        case (int)SkillType.QSpell:
            return HandleAnnieQ(room, caster, skillPacket);
        case (int)SkillType.WSpell:
            return HandleAnnieW(room, caster, skillPacket);
        case (int)SkillType.ESpell:
            return HandleAnnieE(room, caster, skillPacket);
        case (int)SkillType.RSpell:
            return HandleAnnieR(room, caster, skillPacket);
        default:
            return false;
        }
    }

    private bool HandleAnnieQ(GameRoom room, GameObject caster, C_SkillCast skillPacket)
    {
        GameObject target = room.FindObject(skillPacket.TargetId);

        if (!IsValidTarget(caster, target))
            return false;
        SpellData q = Spell(caster, SkillType.QSpell);
        int rank = Progression.SkillRank(caster, SkillType.QSpell);
        float range = q.CastRangeUnits > 0f ? q.CastRangeUnits : 6.25f;

        if (DistanceXZ(caster.Info.Position.ToNumericsVector3(), target.Info.Position.ToNumericsVector3()) >
            range + RangeTolerance)
            return false;

        if (!CombatActionMovement.TryBegin(room, caster, skillPacket.SkillId))
            return false;

        bool stun = Pyromania(room, caster, true);
        float damage =
            q.Value("BaseDamage", rank, 80f) + q.Value("APRatio", rank, 0.8f) * CombatMath.AbilityPower(caster);
        float speed = q.MissileSpeedUnits > 0f ? q.MissileSpeedUnits : 14f;
        ulong projectileId = room.GenerateProjectileId();
        var missile =
            new Projectile(projectileId, caster, target, speed, damage, DamageTypes.Magic, (int)SkillType.QSpell);

        if (stun)
            missile.OnHit = hit => Stun(room, caster, hit);
        room.AddProjectile(missile);

        room.Broadcast(new S_SkillResult { CasterId = caster.Info.ObjectId, SkillId = (int)SkillType.QSpell });
        room.Broadcast(new S_ProjectileSpawn { ProjectileId = projectileId, CasterId = caster.Info.ObjectId,
                                               TargetId = target.Info.ObjectId, StartPos = caster.Info.Position,
                                               EndPos = target.Info.Position, Speed = speed,
                                               SkillId = (int)SkillType.QSpell });

        return true;
    }

    private bool HandleAnnieW(GameRoom room, GameObject caster, C_SkillCast skillPacket)
    {
        if (caster == null || caster.Info == null || skillPacket.TargetPos == null)
            return false;
        SpellData w = Spell(caster, SkillType.WSpell);
        int rank = Progression.SkillRank(caster, SkillType.WSpell);
        float range = w.CastRangeUnits > 0f ? w.CastRangeUnits : 6.0f;

        Vec3 castOrigin = caster.Info.Position.ToNumericsVector3();
        Vec3 targetPosition = new Vec3(skillPacket.TargetPos.X, skillPacket.TargetPos.Y, skillPacket.TargetPos.Z);

        if (!IsFinite(castOrigin) || !IsFinite(targetPosition))
            return false;

        Vec3 castDirection = targetPosition - castOrigin;
        castDirection.Y = 0.0f;
        float requestedDistance = castDirection.Length();

        if (!IsFinite(requestedDistance) || requestedDistance <= 0.01f || requestedDistance > range + 0.10f)
            return false;
        castDirection /= requestedDistance;

        if (!CombatActionMovement.TryBegin(room, caster, skillPacket.SkillId))
            return false;

        room.Broadcast(new S_SkillResult { CasterId = caster.Info.ObjectId, SkillId = (int)SkillType.WSpell,
                                           IsAreaSkill = true, AreaRadius = range,
                                           CenterPos = ToProtocolVector(castOrigin),
                                           CastOrigin = ToProtocolVector(castOrigin),
                                           CastDirection = ToProtocolVector(castDirection) });

        bool stun = Pyromania(room, caster, true);
        float damage =
            w.Value("BaseDamage", rank, 70f) + w.Value("APRatio", rank, 0.85f) * CombatMath.AbilityPower(caster);

        foreach (GameObject target in room.GetObjectsInRange(castOrigin, range))
        {
            if (!IsValidTarget(caster, target) ||
                !IsInsideCone(castOrigin, castDirection, target.Info.Position.ToNumericsVector3(), range))
                continue;
            CombatMath.ApplyDamage(room, caster, target, damage, DamageTypes.Magic, (int)SkillType.WSpell);

            if (stun)
                Stun(room, caster, target);
        }

        return true;
    }

    private bool HandleAnnieE(GameRoom room, GameObject caster, C_SkillCast skillPacket)
    {
        if (!(caster is Player annie) || annie.Info == null)
            return false;
        SpellData e = Spell(caster, SkillType.ESpell);
        int rank = Progression.SkillRank(caster, SkillType.ESpell);
        Player target = annie;

        if (room.FindObject(skillPacket.TargetId) is Player ally && ally.Info != null && ally.Info.Hp > 0 &&
            ally.Info.TeamId == annie.Info.TeamId && !(ally is Tibbers) &&
            DistanceXZ(annie.Info.Position.ToNumericsVector3(), ally.Info.Position.ToNumericsVector3()) <=
                ECastRange + e.Value("CastRangeGenerosity", rank, 225f) * ChampionData.UnitScale)
            target = ally;

        if (!CombatActionMovement.PlayPlaceholder(room, caster, skillPacket.SkillId))
            return false;
        Pyromania(room, caster, false);
        float duration = e.Value("ShieldDuration", rank, 3f);
        Buffs.Shield(room, annie, target, e.Value("ShieldAmount", rank, 60f), duration, (int)SkillType.ESpell,
                     e.Value("DamageReflection", rank, 25f));
        Buffs.MoveSpeed(room, annie, target, e.Value("MovementSpeed", rank, 0.15f),
                        e.Value("MovementSpeedDuration", rank, 1.5f), (int)SkillType.ESpell, decays: true);

        return true;
    }

    private static bool Pyromania(GameRoom room, GameObject caster, bool damaging)
    {
        if (!(caster is Player annie))
            return false;
        StatusEffects status = annie.Status;

        if (damaging && status.StunReady)
        {
            status.StunReady = false;
            status.PyromaniaStacks = 0;
            Buffs.Stacks(room, annie, annie, BUFF_TYPE.BuffStunReady, Buffs.PassiveSkillId, 0);
            Buffs.Stacks(room, annie, annie, BUFF_TYPE.BuffPyromania, Buffs.PassiveSkillId, 0);

            return true;
        }

        if (status.StunReady)
            return false;
        status.PyromaniaStacks = Math.Min(PyromaniaStacks, status.PyromaniaStacks + 1);
        Buffs.Stacks(room, annie, annie, BUFF_TYPE.BuffPyromania, Buffs.PassiveSkillId, status.PyromaniaStacks);

        if (status.PyromaniaStacks >= PyromaniaStacks)
        {
            status.StunReady = true;
            Buffs.Stacks(room, annie, annie, BUFF_TYPE.BuffStunReady, Buffs.PassiveSkillId, 1);
        }

        return false;
    }

    private static void Stun(GameRoom room, GameObject caster, GameObject target)
    {
        if (target is Player victim)
            Buffs.Stun(room, caster, victim, PyromaniaStunSeconds, Buffs.PassiveSkillId);
    }

    private bool HandleAnnieR(GameRoom room, GameObject caster, C_SkillCast skillPacket)
    {
        if (caster == null || caster.Info == null)
            return false;
        SpellData r = Spell(caster, SkillType.RSpell);
        int rank = Progression.SkillRank(caster, SkillType.RSpell);
        float range = r.CastRangeUnits > 0f ? r.CastRangeUnits : 6.0f;
        float radius = r.CastRadiusUnits > 0f ? r.CastRadiusUnits : 2.5f;

        Vec3 castOrigin = caster.Info.Position.ToNumericsVector3();
        Vec3 center = skillPacket.TargetPos != null
                          ? new Vec3(skillPacket.TargetPos.X, castOrigin.Y, skillPacket.TargetPos.Z)
                          : castOrigin;

        if (!IsFinite(castOrigin) || !IsFinite(center))
            return false;

        Vec3 offset = center - castOrigin;
        float distance = offset.Length();

        if (distance > range)
            center = castOrigin + offset / distance * range;

        if (!CombatActionMovement.TryBegin(room, caster, skillPacket.SkillId))
            return false;

        room.Broadcast(new S_SkillResult { CasterId = caster.Info.ObjectId, SkillId = (int)SkillType.RSpell,
                                           IsAreaSkill = true, AreaRadius = radius,
                                           CenterPos = ToProtocolVector(center),
                                           CastOrigin = ToProtocolVector(castOrigin) });

        bool stun = Pyromania(room, caster, true);
        float damage =
            r.Value("InitialDamage", rank, 150f) + r.Value("APRatio", rank, 0.75f) * CombatMath.AbilityPower(caster);

        foreach (GameObject target in room.GetObjectsInRange(center, radius))
        {
            if (!IsValidTarget(caster, target))
                continue;
            CombatMath.ApplyDamage(room, caster, target, damage, DamageTypes.Magic, (int)SkillType.RSpell);

            if (stun)
                Stun(room, caster, target);
        }

        if (caster is Player annie)
            Tibbers.Spawn(room, annie, center, rank);

        return true;
    }

    private static SpellData Spell(GameObject caster,
                                   SkillType slot) => CombatMath.DataOf(caster)?.Spell(slot) ?? SpellData.Empty;

    private static float DistanceXZ(Vec3 a, Vec3 b)
    {
        Vec3 delta = a - b;
        delta.Y = 0f;

        return delta.Length();
    }

    private static bool IsInsideCone(Vec3 origin, Vec3 direction, Vec3 targetPosition, float range)
    {
        Vec3 offset = targetPosition - origin;
        offset.Y = 0.0f;
        float distanceSquared = offset.LengthSquared();

        if (!IsFinite(distanceSquared) || distanceSquared > range * range)
            return false;

        if (distanceSquared <= 0.0001f)
            return true;

        offset /= (float)Math.Sqrt(distanceSquared);
        float minimumDot = (float)Math.Cos(WHalfAngleDegrees * Math.PI / 180.0);

        return Vec3.Dot(direction, offset) >= minimumDot;
    }

    private static bool IsValidTarget(GameObject caster, GameObject target)
    {
        return target != null && target != caster && target.Info != null && target.Info.Hp > 0 && caster.Info != null &&
               target.Info.TeamId != caster.Info.TeamId;
    }

    private static bool IsFinite(Vec3 value)
    {
        return IsFinite(value.X) && IsFinite(value.Y) && IsFinite(value.Z);
    }

    private static bool IsFinite(float value)
    {
        return !float.IsNaN(value) && !float.IsInfinity(value);
    }

    private static Google.Protobuf.Protocol.Vector3 ToProtocolVector(Vec3 value)
    {
        return new Google.Protobuf.Protocol.Vector3 { X = value.X, Y = value.Y, Z = value.Z };
    }
}
