using Google.Protobuf.Protocol;
using Server.Game.Objects;
using Server.Game.Room;
using System;
using System.Collections.Generic;
using Vec3 = System.Numerics.Vector3;

public sealed class PingState
{
    public readonly Queue<long> Recent = new Queue<long>();
}

public static class Pings
{
    public const int MaxPerWindow = 6;
    public const double WindowSeconds = 6.0;
    public const double MinIntervalSeconds = 0.2;

    public static bool Send(GameRoom room, Player champion, PING_TYPE type, Vec3 position, ulong targetId)
    {
        if (room == null || champion?.Info == null || !Progression.IsChampion(champion))
            return false;

        if (!Enum.IsDefined(typeof(PING_TYPE), type))
            return false;
        PingState state = champion.Pings;
        long now = StatusEffects.Now;
        long window = (long)(WindowSeconds * System.Diagnostics.Stopwatch.Frequency);

        while (state.Recent.Count > 0 && now - state.Recent.Peek() > window)
            state.Recent.Dequeue();

        if (state.Recent.Count >= MaxPerWindow)
            return false;

        if (state.Recent.Count > 0 &&
            now - LastOf(state) < (long)(MinIntervalSeconds * System.Diagnostics.Stopwatch.Frequency))
            return false;
        state.Recent.Enqueue(now);

        GameObject target = targetId != 0 ? room.FindObject(targetId) : null;

        if (target?.Info != null && target.Info.TeamId != champion.Info.TeamId &&
            room.Vision.IsHiddenFrom(champion.Info.TeamId, targetId))
            target = null;

        if (target?.Info != null)
            position = target.Info.Position.ToNumericsVector3();
        position.X = Math.Clamp(position.X, 0f, 160f);
        position.Z = Math.Clamp(position.Z, 0f, 160f);
        room.SendToTeam(champion.Info.TeamId, new S_Ping {
            SourceId = champion.Info.ObjectId,
            Type = type,
            Position = position.ToProtoVector3(),
            TargetId = target?.Info?.ObjectId ?? 0,
        });

        return true;
    }

    private static long LastOf(PingState state)
    {
        long last = 0;

        foreach (long at in state.Recent)
            last = at;

        return last;
    }
}
