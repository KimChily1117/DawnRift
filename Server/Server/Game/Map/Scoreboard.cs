using Google.Protobuf.Protocol;
using Server.Game.Objects;
using Server.Game.Room;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;

namespace Server.Game.Map
{
    public sealed class Scoreboard
    {
        public const double AssistSeconds = 10.0;

        public sealed class Score
        {
            public int Kills, Deaths, Assists, CreepScore;
            public int GoldEarned, DamageToChampions, DamageTaken;

            internal readonly List<(ulong source, int skillId, int damage, int type, long at)> Recent =
                new List<(ulong, int, int, int, long)>();
            internal readonly Dictionary<ulong, long> DamagedBy = new Dictionary<ulong, long>();
            // Tests: pretend attacker's last damage was this long ago.
            public void DamagedByForTests(ulong attacker, double secondsAgo) => DamagedBy[attacker] =
                Server.Game.GameClock.Now - (long)(secondsAgo * Stopwatch.Frequency);
        }

        private readonly GameRoom _room;
        private readonly Dictionary<ulong, Score> _scores = new Dictionary<ulong, Score>();
        private long _startedAt;

        public Scoreboard(GameRoom room) => _room = room;

        public bool Started => _startedAt != 0;
        public float GameTime =>
            Started ? (float)((Server.Game.GameClock.Now - _startedAt) / (double)Stopwatch.Frequency) : 0f;

        public void Start()
        {
            if (!Started)
                _startedAt = Server.Game.GameClock.Now;
        }

        public void Reset()
        {
            _startedAt = 0;
            _scores.Clear();
        }

        public Score Of(ulong championId)
        {
            if (!_scores.TryGetValue(championId, out Score score))
                _scores[championId] = score = new Score();

            return score;
        }

        public const double RecapSeconds = 10.0;
        public bool FirstBloodTaken { get; set; }

        public void OnDamaged(GameObject source, GameObject target, int skillId = -1, int dealt = 0, int damageType = 0)
        {
            if (!Progression.IsChampion(target) || target.Info == null)
                return;
            Score victim = Of(target.Info.ObjectId);
            victim.DamageTaken += dealt;
            long now = Server.Game.GameClock.Now;
            victim.Recent.Add((source?.Info?.ObjectId ?? 0, skillId, dealt, damageType, now));
            long since = now - (long)(RecapSeconds * Stopwatch.Frequency);
            victim.Recent.RemoveAll(e => e.at < since);
            Player attacker = MapUnit.ChampionOf(_room, source);

            if (attacker?.Info == null || attacker.Info.TeamId == target.Info.TeamId)
                return;
            victim.DamagedBy[attacker.Info.ObjectId] = now;
            Of(attacker.Info.ObjectId).DamageToChampions += dealt;
        }

        public S_DeathRecap BuildRecap(Player victim, ulong killerId)
        {
            Score score = Of(victim.Info.ObjectId);
            var recap = new S_DeathRecap { VictimId = victim.Info.ObjectId, KillerId = killerId,
                                           Seconds = (float)RecapSeconds };

            foreach (var group in score.Recent.GroupBy(e => (e.source, e.skillId))
                         .OrderByDescending(g => g.Sum(e => e.damage)))
            {
                GameObject source = _room.FindObject(group.Key.source);
                int total = group.Sum(e => e.damage);
                recap.Sources.Add(new DamageSource {
                    SourceId = group.Key.source,
                    SourceName = source?.Info?.Name ?? "",
                    SourceType = source?.Info?.ChampType ?? PLAYER_CHAMPION_TYPE.PlayerTypeNone,
                    SkillId = group.Key.skillId,
                    Damage = total,
                    DamageType = group.GroupBy(e => e.type).OrderByDescending(t => t.Sum(e => e.damage)).First().Key,
                });
                recap.Total += total;
            }

            score.Recent.Clear();

            return recap;
        }

        // The end screen: every champion's line (items included).
        public S_MatchSummary BuildSummary(int winningTeam, bool surrendered)
        {
            var summary =
                new S_MatchSummary { WinningTeam = winningTeam, GameTime = GameTime, Surrendered = surrendered };

            foreach (Player champion in _room.Units.OfType<Player>()
                         .Where(Progression.IsChampion)
                         .OrderBy(p => p.Info.TeamId))
            {
                Score score = Of(champion.Info.ObjectId);
                var entry = new MatchEntry {
                    ObjectId = champion.Info.ObjectId,
                    Name = champion.Info.Name ?? "",
                    Champion = champion.Info.ChampType,
                    Team = champion.Info.TeamId,
                    Level = champion.Progress.Level,
                    Kills = score.Kills,
                    Deaths = score.Deaths,
                    Assists = score.Assists,
                    Cs = score.CreepScore,
                    GoldEarned = score.GoldEarned,
                    DamageToChampions = score.DamageToChampions,
                    DamageTaken = score.DamageTaken,
                };

                foreach (var slot in champion.Items.Slots)
                    entry.Items.Add(slot.ItemId);
                summary.Entries.Add(entry);
            }

            return summary;
        }

        public List<ulong> RecentAttackers(Player victim, double seconds)
        {
            long since = Server.Game.GameClock.Now - (long)(seconds * Stopwatch.Frequency);

            return Of(victim.Info.ObjectId)
                .DamagedBy.Where(entry => entry.Value >= since)
                .Select(entry => entry.Key)
                .ToList();
        }

        public void OnChampionKilled(Player victim, ulong killerId)
        {
            Score score = Of(victim.Info.ObjectId);
            ++score.Deaths;

            if (killerId != 0)
                ++Of(killerId).Kills;
            long since = Server.Game.GameClock.Now - (long)(AssistSeconds * Stopwatch.Frequency);

            foreach (var (attacker, at) in score.DamagedBy)
                if (attacker != killerId && at >= since)
                    ++Of(attacker).Assists;
            score.DamagedBy.Clear();
            Send();
        }

        // MapDirector: a champion last-hit a minion or a monster.
        public void OnCreepKilled(Player champion)
        {
            if (champion?.Info == null)
                return;
            ++Of(champion.Info.ObjectId).CreepScore;
            Send();
        }

        public S_Scoreboard Build()
        {
            var packet = new S_Scoreboard { GameTime = GameTime };

            foreach (Player champion in _room.Units.OfType<Player>().Where(Progression.IsChampion))
            {
                Score score = Of(champion.Info.ObjectId);
                packet.Entries.Add(new ScoreEntry {
                    ObjectId = champion.Info.ObjectId,
                    Kills = score.Kills,
                    Deaths = score.Deaths,
                    Assists = score.Assists,
                    Cs = score.CreepScore,
                });
            }

            return packet;
        }

        public void Send() => _room.Broadcast(Build());
    }
}
