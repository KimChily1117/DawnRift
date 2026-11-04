using Google.Protobuf.Protocol;
using Server.Game.Objects;
using Server.Game.Room;
using System;
using Vec3 = System.Numerics.Vector3;

public static class BasicAttackHandler
{
    private const float RangeTolerance = 0.75f;
    private const double ReadyToleranceSeconds = 0.1;

    public static bool Handle(GameRoom room, GameObject caster, C_SkillCast skillPacket)
    {
        if (!(caster is Player player) || player.Info == null || room == null)
            return false;
        GameObject target = room.FindObject(skillPacket.TargetId);

        if (!IsValidTarget(player, target))
        {
            Log(player, "rejected: invalid target " + skillPacket.TargetId +
                            (target?.Info == null ? " (not in room)"
                                                  : " hp " + target.Info.Hp + " team " + target.Info.TeamId) +
                            " casterTeam " + player.Info.TeamId + " casterHp " + player.Info.Hp);

            return false;
        }

        ChampionCombatStats stats = ChampionCombatStats.For(player.Info.ChampType);
        float distance = DistanceXZ(player, target);

        float targetRadius = target is MapUnit mapUnit ? mapUnit.Radius : 0f;

        if (distance > stats.AttackRange + RangeTolerance + targetRadius)
        {
            Log(player, "rejected: out of range " + distance.ToString("F2"));

            return false;
        }

        if (player.Status.IsDisabled || player.Status.IsDashing)
        {
            Log(player, "rejected: disabled");

            return false;
        }

        if (ChampionRegistry.Find(player.Info.ChampType)?.CanBasicAttack?.Invoke(player) == false)
        {
            Log(player, "rejected: reloading");

            return false;
        }

        double speed = Math.Max(0.2, 1.0 + player.Status.AttackSpeedBonus +
                                         Server.Game.Items.ItemEffects.AttackSpeedBonus(room, player));

        if (!player.TryBeginBasicAttack(stats.WindupSeconds / speed, stats.AttackCycleSeconds / speed,
                                        ReadyToleranceSeconds))
        {
            Log(player, "rejected: attack not ready or busy");

            return false;
        }

        Log(player, "started on " + target.Info.ObjectId + " distance " + distance.ToString("F2"));
        Recall.Cancel(room, player);
        room.CancelPlayerMovement(player);

        room.Broadcast(new S_SkillResult { CasterId = player.Info.ObjectId, SkillId = (int)SkillType.Gerneral,
                                           HitObjects = { target.Info.ObjectId } });

        int sequence = player.BasicAttackSequence;
        room.PushAfter((int)Math.Round(stats.WindupSeconds / speed * 1000.0),
                       () => LandAttack(room, player, target, stats, sequence));

        return true;
    }

    private static void LandAttack(GameRoom room, Player player, GameObject target, ChampionCombatStats stats,
                                   int sequence)
    {
        if (player.BasicAttackSequence != sequence || player.Info == null || player.Info.Hp <= 0)
        {
            Log(player, "windup cancelled");

            return;
        }

        if (!ReferenceEquals(room.FindObject(player.Info.ObjectId), player) || !IsValidTarget(player, target) ||
            !ReferenceEquals(room.FindObject(target.Info.ObjectId), target))
            return;

        ChampionKit kit = ChampionRegistry.Find(player.Info.ChampType);
        float damage = CombatMath.AttackDamage(player) * (kit?.BasicAttackDamage?.Invoke(player, target) ?? 1f);

        bool crit = Server.Game.Items.ItemEffects.RollCrit(player);

        if (crit && player.Info.ChampType == PLAYER_CHAMPION_TYPE.PlayerTypeAshe)
            player.Status.AsheCritPending = true;
        else if (crit)
            damage *= Server.Game.Items.ItemEffects.CritMultiplier(player);

        if (player.Status.TakeOnHitBonus(out float bonus, out ulong bonusSource))
            CombatMath.ApplyDamage(room, room.FindObject(bonusSource) ?? player, target, bonus, DamageTypes.Magic,
                                   Buffs.PassiveSkillId);

        if (kit?.FireBasicAttack != null)
        {
            kit.FireBasicAttack(room, player, target, damage);

            return;
        }

        if (!stats.IsRanged)
        {
            int dealt = CombatMath.ApplyDamage(room, player, target, damage, DamageTypes.Physical,
                                               (int)SkillType.Gerneral, crit: crit);
            kit?.OnBasicAttackHit?.Invoke(room, player, target);
            Server.Game.Items.ItemEffects.OnBasicAttackHit(room, player, target, dealt, crit);
            Log(player, "hit " + target.Info.ObjectId + " hp " + target.Info.Hp);

            return;
        }

        Log(player, "projectile launched at " + target.Info.ObjectId);

        ulong projectileId = room.GenerateProjectileId();
        var missile = new Projectile(projectileId, player, target, stats.ProjectileSpeed, damage, DamageTypes.Physical,
                                     (int)SkillType.Gerneral) { Crit = crit };
        missile.OnHit = hit =>
        {
            kit?.OnBasicAttackHit?.Invoke(room, player, hit);
            Server.Game.Items.ItemEffects.OnBasicAttackHit(room, player, hit, missile.Dealt, crit);
        };
        room.AddProjectile(missile);
        room.Broadcast(new S_ProjectileSpawn {
            ProjectileId = projectileId,
            CasterId = player.Info.ObjectId,
            TargetId = target.Info.ObjectId,
            StartPos = player.Info.Position,
            EndPos = target.Info.Position,
            Speed = stats.ProjectileSpeed,
            SkillId = (int)SkillType.Gerneral,
            Variant = kit?.BasicAttackVariant?.Invoke(player) ?? 0,
        });
    }

    private static void Log(Player player, string message)
    {
        Console.WriteLine("[BasicAttack] Player=" + player.Info?.ObjectId + " " + message);
    }

    private static float DistanceXZ(GameObject a, GameObject b)
    {
        Vec3 delta = a.Info.Position.ToNumericsVector3() - b.Info.Position.ToNumericsVector3();
        delta.Y = 0.0f;

        return delta.Length();
    }

    private static bool IsValidTarget(GameObject caster, GameObject target)
    {
        return target?.Info != null && caster?.Info != null && target.Info.Hp > 0 &&
               target.Info.TeamId != caster.Info.TeamId;
    }
}
