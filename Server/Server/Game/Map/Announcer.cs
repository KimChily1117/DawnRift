using Google.Protobuf.Protocol;
using Server.Game.Objects;
using Server.Game.Room;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;

namespace Server.Game.Map
{
    public sealed class Announcer
    {
        public const double MultiKillSeconds = 10.0;
        public const double PentaKillSeconds = 30.0;
        public const int SpreeStart = 3;
        public const float InhibitorWarningSeconds = 15f;

        private sealed class Streak
        {
            public int Spree;
            public int Multi;
            public double LastKillAt = double.NegativeInfinity;
        }

        private readonly GameRoom _room;
        private readonly Dictionary<ulong, Streak> _streaks = new Dictionary<ulong, Streak>();
        private bool _firstBlood, _firstTurret;

        // Seconds on a monotonic clock; tests replace it.
        public Func<double> Clock { get; set; } = () => Server.Game.GameClock.Now / (double)Stopwatch.Frequency;
        // Every announcement made, for tests.
        public List<S_Announcement> Log { get; } = new List<S_Announcement>();

        public Announcer(GameRoom room) => _room = room;

        public void Reset()
        {
            _streaks.Clear();
            _firstBlood = _firstTurret = false;
            Log.Clear();
        }

        public int SpreeOf(ulong championId) => _streaks.TryGetValue(championId, out Streak s) ? s.Spree : 0;

        private Streak Of(ulong id)
        {
            if (!_streaks.TryGetValue(id, out Streak streak))
                _streaks[id] = streak = new Streak();

            return streak;
        }

        private void Say(ANNOUNCEMENT kind, int team, ulong source = 0, ulong target = 0, int count = 0)
        {
            var packet =
                new S_Announcement { Kind = kind, Team = team, SourceId = source, TargetId = target, Count = count };
            Log.Add(packet);
            _room?.Broadcast(packet);
        }

        public void OnChampionKilled(Player victim, ulong killerId, GameObject source)
        {
            ulong victimId = victim.Info.ObjectId;
            int victimTeam = victim.Info.TeamId;
            Streak dead = Of(victimId);
            int victimSpree = dead.Spree;
            dead.Spree = 0;
            dead.Multi = 0;
            dead.LastKillAt = double.NegativeInfinity;

            if (killerId == 0)
            {
                ulong executor = source?.Info?.ObjectId ?? 0;
                Say(ANNOUNCEMENT.AnnounceExecuted, victimTeam, executor, victimId);
            }
            else
            {
                int killerTeam = 3 - victimTeam;
                Streak killer = Of(killerId);

                double now = Clock();

                double window = killer.Multi == 4 ? PentaKillSeconds : MultiKillSeconds;
                killer.Multi = now - killer.LastKillAt <= window && killer.Multi < 5 ? killer.Multi + 1 : 1;
                killer.LastKillAt = now;
                ++killer.Spree;

                if (!_firstBlood)
                    Say(ANNOUNCEMENT.AnnounceFirstBlood, killerTeam, killerId, victimId);
                else if (killer.Multi >= 2)
                    Say(ANNOUNCEMENT.AnnounceMultikill, killerTeam, killerId, victimId, killer.Multi);
                else if (victimSpree >= SpreeStart)
                    Say(ANNOUNCEMENT.AnnounceShutdown, killerTeam, killerId, victimId, victimSpree);
                else if (killer.Spree >= SpreeStart)
                    Say(ANNOUNCEMENT.AnnounceKillingSpree, killerTeam, killerId, victimId, Math.Min(killer.Spree, 8));
                else
                    Say(ANNOUNCEMENT.AnnounceChampionKill, killerTeam, killerId, victimId);
            }

            _firstBlood |= killerId != 0;

            List<Player> team = _room?.Units.OfType<Player>()
                                    .Where(p => Progression.IsChampion(p) && p.Info?.TeamId == victimTeam)
                                    .ToList() ??
                                new List<Player>();

            if (team.Count >= 2 && team.All(p => p == victim || p.Info.Hp <= 0 || p.Progress.IsDead))
                Say(ANNOUNCEMENT.AnnounceAce, 3 - victimTeam, killerId, victimId);
        }

        public void OnTurretDestroyed(Turret turret)
        {
            Say(ANNOUNCEMENT.AnnounceTurretDestroyed, turret.Info.TeamId, 0, turret.Info.ObjectId,
                _firstTurret ? 0 : 1);
            _firstTurret = true;
        }

        public void OnInhibitorDestroyed(Structure inhibitor) => Say(ANNOUNCEMENT.AnnounceInhibitorDestroyed,
                                                                     inhibitor.Info.TeamId, 0, inhibitor.Info.ObjectId);

        public void OnInhibitorRespawnSoon(Structure inhibitor) => Say(ANNOUNCEMENT.AnnounceInhibitorRespawnSoon,
                                                                       inhibitor.Info.TeamId, 0,
                                                                       inhibitor.Info.ObjectId);

        public void OnInhibitorRespawned(Structure inhibitor) => Say(ANNOUNCEMENT.AnnounceInhibitorRespawned,
                                                                     inhibitor.Info.TeamId, 0, inhibitor.Info.ObjectId);

        public void OnEpicKilled(JungleMonster epic, Player killer)
        {
            if (killer?.Info == null)
                return;
            Say(epic.IsDragon ? ANNOUNCEMENT.AnnounceDragonKilled : ANNOUNCEMENT.AnnounceBaronKilled,
                killer.Info.TeamId, killer.Info.ObjectId, epic.Info.ObjectId);
        }

        public void Welcome() => Say(ANNOUNCEMENT.AnnounceWelcome, 0);
        public void MinionsSoon() => Say(ANNOUNCEMENT.AnnounceMinionsSoon, 0);
        public void MinionsSpawned() => Say(ANNOUNCEMENT.AnnounceMinionsSpawned, 0);
    }
}
