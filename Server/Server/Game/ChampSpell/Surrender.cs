using Google.Protobuf.Protocol;
using Server.Game.Objects;
using Server.Game.Room;
using System;
using System.Collections.Generic;
using System.Linq;

public sealed class SurrenderVote
{
    public int Team;
    public long StartedAt, EndsAt;
    public readonly Dictionary<ulong, bool> Votes = new Dictionary<ulong, bool>();
    public ulong StarterId;
}

public static class Surrender
{
    public static double AllowedAfterSeconds =
        double.TryParse(Environment.GetEnvironmentVariable("DAWN_SURRENDER_SECONDS"), out double s) ? s : 15.0 * 60.0;
    public const double VoteSeconds = 60.0;
    public const double CooldownSeconds = 180.0;

    public static int Needed(int teamSize) => Math.Max(1, (int)Math.Ceiling(teamSize * 0.7));

    public static bool Vote(GameRoom room, Player player, bool agree)
    {
        if (room == null || player?.Info == null || !Progression.IsChampion(player) || (room.Map?.IsOver ?? false))
            return false;
        int team = player.Info.TeamId;
        SurrenderVote vote =
            room.SurrenderVotes.TryGetValue(team, out SurrenderVote running) && StatusEffects.Now < running.EndsAt
                ? running
                : null;

        if (vote == null)
        {
            if (!agree)
                return false;
            float elapsed = room.Map?.Elapsed ?? 0f;
            long cooldownUntil = room.SurrenderCooldown.TryGetValue(team, out long until) ? until : 0;

            if (elapsed < AllowedAfterSeconds || StatusEffects.Now < cooldownUntil)
            {
                player.Session?.Send(
                    new S_Surrender { Team = team, State = 3,
                                      SecondsLeft = (float)Math.Max(0.0, AllowedAfterSeconds - elapsed),
                                      VoterId = player.Info.ObjectId });

                return false;
            }

            vote = new SurrenderVote { Team = team, StartedAt = StatusEffects.Now,
                                       EndsAt = StatusEffects.After(VoteSeconds), StarterId = player.Info.ObjectId };
            room.SurrenderVotes[team] = vote;
        }

        if (vote.Votes.ContainsKey(player.Info.ObjectId))
            return false;
        vote.Votes[player.Info.ObjectId] = agree;
        Resolve(room, vote, player.Info.ObjectId, agree);

        return true;
    }

    private static void Resolve(GameRoom room, SurrenderVote vote, ulong voterId, bool agreed)
    {
        int teamSize = Math.Max(
            1, room.Units.OfType<Player>().Count(p => Progression.IsChampion(p) && p.Info.TeamId == vote.Team));
        int yes = vote.Votes.Values.Count(v => v), no = vote.Votes.Count - yes, needed = Needed(teamSize);
        int state = 0;

        if (yes >= needed)
            state = 1;
        else if (no > teamSize - needed || StatusEffects.Now >= vote.EndsAt)
            state = 2;
        room.SendToTeam(vote.Team, new S_Surrender {
            Team = vote.Team,
            Yes = yes,
            No = no,
            Needed = needed,
            State = state,
            VoterId = voterId,
            LastVoteAgreed = agreed,
            SecondsLeft = (float)Math.Max(0.0, (vote.EndsAt - StatusEffects.Now) /
                                                   (double)System.Diagnostics.Stopwatch.Frequency),
        });

        if (state == 0)
            return;
        room.SurrenderVotes.Remove(vote.Team);

        if (state == 1)
        {
            Console.WriteLine($"[Surrender] Team {vote.Team} surrendered ({yes}/{needed}).");
            room.Map?.EndGame(3 - vote.Team, surrendered: true);
        }
        else
            room.SurrenderCooldown[vote.Team] = StatusEffects.After(CooldownSeconds);
    }

    // Per Room tick: a vote that ran out of time fails.
    public static void Update(GameRoom room)
    {
        if (room.SurrenderVotes.Count == 0)
            return;

        foreach (SurrenderVote vote in room.SurrenderVotes.Values.ToList())
            if (StatusEffects.Now >= vote.EndsAt)
                Resolve(room, vote, 0, false);
    }
}
