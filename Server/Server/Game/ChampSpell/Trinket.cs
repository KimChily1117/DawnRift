using Google.Protobuf.Protocol;
using Server.Game.Movement;
using Server.Game.Objects;
using Server.Game.Room;
using System;
using System.Collections.Generic;
using System.Linq;
using Vec3 = System.Numerics.Vector3;

public sealed class TrinketState
{
    public int Charges = Trinket.MaxCharges;
    public long NextChargeAt;
    public double RechargeDuration;
    public readonly List<Ward> Wards = new List<Ward>();
}

public static class Trinket
{
    public const int MaxCharges = 2;
    public const int MaxWards = 3;
    public const float PlaceRange = 6.0f;
    public const int SkillId = 7;

    public static double RechargeSeconds(int level) => 210.0 - 90.0 * (Math.Clamp(level, 1, 18) - 1) / 17.0;
    public static double WardSeconds(int level) => 90.0 + 30.0 * (Math.Clamp(level, 1, 18) - 1) / 17.0;

    public static SKILL_REJECT_REASON Place(GameRoom room, Player champion, Vec3 position)
    {
        SKILL_REJECT_REASON reason = TryPlace(room, champion, position);

        if (reason != SKILL_REJECT_REASON.SkillRejectNone)
            champion.Session?.Send(new S_SkillRejected { SkillId = SkillId, Reason = reason });

        return reason;
    }

    private static SKILL_REJECT_REASON TryPlace(GameRoom room, Player champion, Vec3 position)
    {
        if (champion.Info == null || !Progression.IsChampion(champion))
            return SKILL_REJECT_REASON.SkillRejectInvalid;

        if (champion.Info.Hp <= 0)
            return SKILL_REJECT_REASON.SkillRejectDead;

        if (champion.Status.IsStunned)
            return SKILL_REJECT_REASON.SkillRejectStunned;

        if (champion.HeraldEyeEndsAt > StatusEffects.Now && room.Map != null)
            return room.Map.SummonHerald(champion, position) ? SKILL_REJECT_REASON.SkillRejectNone
                                                             : SKILL_REJECT_REASON.SkillRejectInvalid;
        TrinketState state = champion.Trinket;
        Recharge(champion);

        if (state.Charges <= 0)
            return SKILL_REJECT_REASON.SkillRejectCooldown;

        Vec3 start = champion.Info.Position.ToNumericsVector3();
        Vec3 delta = position - start;
        delta.Y = 0f;

        if (delta.Length() > PlaceRange)
            position = start + delta / delta.Length() * PlaceRange;
        var navigation = room.Navigation;

        if (!navigation.TryWorldToCell(position.X, navigation.Origin.Y, position.Z, out var cell) ||
            !AuthoritativeMoveService.TryResolveDestination(navigation, cell, out var walkable, out bool adjusted) ||
            !navigation.TryGetCellWorldCenter(walkable.X, walkable.Z, out Vec3 center))
            return SKILL_REJECT_REASON.SkillRejectInvalid;
        Vec3 spot = adjusted ? new Vec3(center.X, start.Y, center.Z) : new Vec3(position.X, start.Y, position.Z);

        state.Wards.RemoveAll(w => w.Room == null || !w.IsAlive);

        if (state.Wards.Count >= MaxWards)
        {
            state.Wards[0].Expire();
            state.Wards.RemoveAt(0);
        }

        int level = champion.Progress.Level;
        Ward ward = Ward.Create(room, champion, spot, WardSeconds(level));
        ward.Spawn();
        state.Wards.Add(ward);
        --state.Charges;

        if (state.NextChargeAt == 0)
        {
            state.RechargeDuration = RechargeSeconds(level);
            state.NextChargeAt = StatusEffects.After(state.RechargeDuration);
        }

        Send(champion);

        return SKILL_REJECT_REASON.SkillRejectNone;
    }

    public static void Recharge(Player champion)
    {
        TrinketState state = champion.Trinket;

        if (state.NextChargeAt == 0 || StatusEffects.Now < state.NextChargeAt)
            return;
        ++state.Charges;

        if (state.Charges < MaxCharges)
        {
            state.RechargeDuration = RechargeSeconds(champion.Progress.Level);
            state.NextChargeAt = StatusEffects.After(state.RechargeDuration);
        }
        else
            state.NextChargeAt = 0;
        Send(champion);
    }

    public static void Send(Player champion)
    {
        TrinketState state = champion.Trinket;
        champion.Session?.Send(new S_Trinket {
            Charges = state.Charges,
            Recharge = state.NextChargeAt != 0
                           ? (float)Math.Max(0.0, (state.NextChargeAt - StatusEffects.Now) /
                                                      (double)System.Diagnostics.Stopwatch.Frequency)
                           : 0f,
            RechargeDuration = (float)state.RechargeDuration,
            Wards = state.Wards.Count(w => w.Room != null && w.IsAlive),
        });
    }
}
