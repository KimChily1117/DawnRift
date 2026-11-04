using Google.Protobuf.Protocol;
using Server.Game.Items;
using Server.Game.Map;
using Server.Game.Objects;
using Server.Game.Room;
using System;

public static class Buffs
{
    public const int PassiveSkillId = -1;

    private const double PerseveranceDelaySeconds = 8.0;
    private const float PerseverancePercentPerSecond = 0.015f / 5f;

    public static void Shield(GameRoom room, GameObject source, Player target, float amount, double seconds,
                              int skillId, float reflectDamage = 0f)
    {
        amount = ItemEffects.ShieldAmount(source, target, amount);
        target.Status.SetShield(amount, seconds, source?.Info?.ObjectId ?? 0, skillId, reflectDamage);
        Send(room, source, target, BUFF_TYPE.BuffShield, skillId, amount, seconds);
        SendHp(room, target);

        if (source is Player caster && !ReferenceEquals(caster, target) && caster.Info != null &&
            caster.Info.TeamId == target.Info.TeamId && Progression.IsChampion(caster) &&
            Progression.IsChampion(target))
            ItemEffects.OnHealOrShieldAlly(room, caster, target, amount, isHeal: false);
    }

    public static int Heal(GameRoom room, GameObject source, Player target, float amount,
                           int skillId) => ItemEffects.Heal(room, source, target, amount, skillId);

    public static void DamageReduction(GameRoom room, GameObject source, Player target, float fraction, double seconds,
                                       int skillId)
    {
        target.Status.SetDamageReduction(fraction, seconds);
        Send(room, source, target, BUFF_TYPE.BuffDamageReduction, skillId, fraction, seconds);
    }

    public static void Tenacity(GameRoom room, GameObject source, Player target, float fraction, double seconds,
                                int skillId)
    {
        target.Status.SetTenacity(fraction, seconds);
        Send(room, source, target, BUFF_TYPE.BuffTenacity, skillId, fraction, seconds);
    }

    public static void MoveSpeed(GameRoom room, GameObject source, Player target, float amount, double seconds,
                                 int skillId, bool decays = false)
    {
        target.Status.AddMoveSpeed(amount, seconds, decays);
        Send(room, source, target, BUFF_TYPE.BuffMoveSpeed, skillId, amount, seconds);
    }

    public static void ArmorShred(GameRoom room, GameObject source, Player target, float fraction, double seconds,
                                  int skillId)
    {
        target.Status.SetArmorShred(fraction, seconds);
        Send(room, source, target, BUFF_TYPE.BuffArmorShred, skillId, fraction, seconds);
    }

    public static void Stun(GameRoom room, GameObject source, Player target, double seconds, int skillId)
    {
        if (target.Info == null || target.Info.Hp <= 0)
            return;
        seconds *= (1.0 - target.Status.Tenacity) * (1.0 - target.ItemStats.Tenacity);
        target.Status.Stun(seconds);
        target.CancelBasicAttackWindup();
        room.CancelPlayerMovement(target);
        Send(room, source, target, BUFF_TYPE.BuffStun, skillId, 0f, seconds);
        ItemEffects.OnCrowdControl(room, source, target, true, seconds);
    }

    public static void Slow(GameRoom room, GameObject source, Player target, float fraction, double seconds,
                            int skillId, bool decays = false)
    {
        if (target.Info == null || target.Info.Hp <= 0 || fraction <= 0f)
            return;
        target.Status.AddSlow(fraction, seconds, decays);
        Send(room, source, target, BUFF_TYPE.BuffSlow, skillId, fraction, seconds);
        ItemEffects.OnCrowdControl(room, source, target, false, seconds);
    }

    public static void Charm(GameRoom room, GameObject source, Player target, double seconds, float slow, int skillId)
    {
        if (target.Info == null || target.Info.Hp <= 0 || source?.Info == null)
            return;
        seconds *= 1.0 - target.Status.Tenacity;
        target.Status.Charm(seconds, source.Info.ObjectId, slow);
        target.CancelBasicAttackWindup();
        room.CancelPlayerMovement(target);
        Send(room, source, target, BUFF_TYPE.BuffCharm, skillId, slow, seconds);
        WalkTowardCharmer(room, target);
        ItemEffects.OnCrowdControl(room, source, target, true, seconds);
    }

    public static void AttackSpeed(GameRoom room, GameObject source, Player target, float fraction, double seconds,
                                   int skillId)
    {
        target.Status.AddAttackSpeed(fraction, seconds);
        Send(room, source, target, BUFF_TYPE.BuffAttackSpeed, skillId, fraction, seconds);
    }

    public static void State(GameRoom room, Player owner, BUFF_TYPE type, int skillId, float amount, double seconds)
    {
        Send(room, owner, owner, type, skillId, amount, seconds);
    }

    private static void WalkTowardCharmer(GameRoom room, Player target)
    {
        if (room.FindObject(target.Status.CharmSourceId) is GameObject charmer && charmer.Info != null)
            Server.Game.Movement.AuthoritativeMoveService.TryStartServerMove(room, target,
                                                                             charmer.Info.Position.ToNumericsVector3());
    }

    public static void Silence(GameRoom room, GameObject source, Player target, double seconds, int skillId)
    {
        if (target.Info == null || target.Info.Hp <= 0)
            return;
        seconds *= 1.0 - target.Status.Tenacity;
        target.Status.Silence(seconds);
        Send(room, source, target, BUFF_TYPE.BuffSilence, skillId, 0f, seconds);
    }

    // Baron's buff on one champion (EpicBuffs); death ends it.
    public static void HandOfBaron(GameRoom room, GameObject source, Player target, double seconds)
    {
        target.Status.HandOfBaronEndsAt = StatusEffects.After(seconds);
        Send(room, source, target, BUFF_TYPE.BuffHandOfBaron, PassiveSkillId, EpicBuffs.HandOfBaronAttackDamage,
             seconds);
    }

    public static void Stacks(GameRoom room, Player owner, GameObject target, BUFF_TYPE type, int skillId, int stacks)
    {
        Send(room, owner, target, type, skillId, stacks, stacks > 0 ? 1f : 0f);
    }

    public static void Update(GameRoom room, Player player, float deltaTime)
    {
        if (player.Info == null || player.Info.Hp <= 0)
            return;
        // A dash ends where its path does.
        if (player.Status.IsDashing && !(player.MovementState?.IsActive ?? false))
            player.Status.DashEndsAt = 0;

        if (player.Status.IsCharmed)
        {
            player.CharmRepathTimer -= deltaTime;

            if (player.CharmRepathTimer <= 0f)
            {
                player.CharmRepathTimer = 0.25f;
                WalkTowardCharmer(room, player);
            }
        }

        ChampionRegistry.Find(player.Info.ChampType)?.Update?.Invoke(room, player, deltaTime);

        if (player.Status.ExpireShield())
        {
            Send(room, null, player, BUFF_TYPE.BuffShield, player.Status.ShieldSkillId, 0f, 0f);
            SendHp(room, player);
        }

        if (player.Info.ChampType == PLAYER_CHAMPION_TYPE.PlayerTypeGaren && player.Info.Hp < player.Info.MaxHp &&
            StatusEffects.Now - player.Status.LastDamagedAt >
                (long)(PerseveranceDelaySeconds * System.Diagnostics.Stopwatch.Frequency))
        {
            player.RegenCarry +=
                player.Info.MaxHp * PerseverancePercentPerSecond * player.Status.HealingMultiplier * deltaTime;

            if (player.RegenCarry >= 1f)
            {
                int heal = (int)player.RegenCarry;
                player.RegenCarry -= heal;
                player.Info.Hp = Math.Min(player.Info.MaxHp, player.Info.Hp + heal);
                SendHp(room, player);
            }
        }
    }

    public static void SendHp(GameRoom room, Player target)
    {
        room.Broadcast(new S_HpUpdate {
            TargetId = target.Info.ObjectId,
            Hp = target.Info.Hp,
            MaxHp = target.Info.MaxHp,
            Shield = (int)Math.Ceiling(target.Status.Shield),
        });
    }

    private static void Send(GameRoom room, GameObject source, GameObject target, BUFF_TYPE type, int skillId,
                             float amount, double seconds)
    {
        room.Broadcast(new S_Buff {
            TargetId = target.Info.ObjectId,
            SourceId = source?.Info?.ObjectId ?? 0,
            BuffType = type,
            SkillId = skillId,
            Amount = amount,
            Duration = (float)Math.Max(0.0, seconds),
        });
    }
}
