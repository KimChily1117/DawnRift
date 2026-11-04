using Google.Protobuf;
using Google.Protobuf.Protocol;
using Server.Game.Objects;
using Server.Game.Room;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using Vec3 = System.Numerics.Vector3;

namespace Server.Game.Vision
{
    public sealed class VisionSystem
    {
        public const float SightRange = 13.5f;
        public const double RevealSeconds = 1.0;
        public const float NearsightRange = 2.5f;
        private const float UpdateInterval = 0.1f;

        private readonly GameRoom _room;
        private readonly Dictionary<int, HashSet<ulong>> _hidden = new Dictionary<int, HashSet<ulong>>();
        private readonly Dictionary<(ulong unit, int team), long> _revealedUntil = new Dictionary<(ulong, int), long>();
        private float _timer;

        public VisionGrid Grid { get; set; }

        public VisionSystem(GameRoom room, VisionGrid grid)
        {
            _room = room;
            Grid = grid;
        }

        public bool Enabled => Grid != null;

        public static bool FogEnabled =>
            !string.Equals(Environment.GetEnvironmentVariable("DAWN_FOG"), "off", StringComparison.OrdinalIgnoreCase);

        public bool IsHiddenFrom(int team, ulong unitId) =>
            Enabled && _hidden.TryGetValue(team, out HashSet<ulong> hidden) && hidden.Contains(unitId);

        private readonly List<(int Team, Vec3 Position, float Radius, long Until)> _sight =
            new List<(int, Vec3, float, long)>();

        public void AddSight(int team, Vec3 position, float radius, double seconds)
        {
            long now = Server.Game.GameClock.Now;
            _sight.RemoveAll(s => s.Until <= now);
            _sight.Add((team, position, radius, now + (long)(seconds * Stopwatch.Frequency)));
        }

        public void Reveal(GameObject unit, int toTeam)
        {
            if (unit?.Info == null)
                return;
            _revealedUntil[(unit.Info.ObjectId, toTeam)] =
                Server.Game.GameClock.Now + (long)(RevealSeconds * Stopwatch.Frequency);
        }

        // Current visibility, computed now.
        public bool CanSee(int team, GameObject target)
        {
            if (!Enabled || target?.Info == null || target.Info.TeamId == team)
                return true;

            if (target is MapUnit unit && unit.IsStructure)
                return true;

            if (target is Ward)
                return false;

            if (_revealedUntil.TryGetValue((target.Info.ObjectId, team), out long until) &&
                Server.Game.GameClock.Now < until)
                return true;
            Vec3 targetPosition = target.Info.Position.ToNumericsVector3();
            int targetBush = Grid.BushAt(targetPosition.X, targetPosition.Z);
            long now = Server.Game.GameClock.Now;

            foreach (var sight in _sight)
            {
                if (sight.Team != team || sight.Until <= now)
                    continue;
                float sx = targetPosition.X - sight.Position.X, sz = targetPosition.Z - sight.Position.Z;

                if (sx * sx + sz * sz > sight.Radius * sight.Radius)
                    continue;

                if (targetBush != 0 && Grid.BushAt(sight.Position.X, sight.Position.Z) != targetBush)
                    continue;

                if (Grid.HasLineOfSight(sight.Position, targetPosition))
                    return true;
            }

            foreach (GameObject observer in _room.Units)
            {
                if (observer.Info == null || observer.Info.TeamId != team || observer.Info.Hp <= 0)
                    continue;
                Vec3 eye = observer.Info.Position.ToNumericsVector3();
                float dx = targetPosition.X - eye.X, dz = targetPosition.Z - eye.Z;
                float sight = observer is Ward ? Ward.SightRange : SightRange;

                if (observer is Player blinded && blinded.Status.IsNearsighted)
                    sight = NearsightRange;

                if (dx * dx + dz * dz > sight * sight)
                    continue;

                if (targetBush != 0 && Grid.BushAt(eye.X, eye.Z) != targetBush)
                    continue;

                if (Grid.HasLineOfSight(eye, targetPosition))
                    return true;
            }

            return false;
        }

        public void Update(float deltaTime)
        {
            if (!Enabled)
                return;
            _timer -= deltaTime;

            if (_timer > 0f)
                return;
            _timer = UpdateInterval;

            foreach (int team in new[] { 1, 2 })
            {
                if (!_hidden.TryGetValue(team, out HashSet<ulong> hidden))
                    _hidden[team] = hidden = new HashSet<ulong>();

                foreach (GameObject unit in _room.Units)
                {
                    if (unit.Info == null || unit.Info.TeamId == team)
                        continue;
                    bool visible = CanSee(team, unit);

                    if (visible == !hidden.Contains(unit.Info.ObjectId))
                        continue;

                    if (visible)
                        hidden.Remove(unit.Info.ObjectId);
                    else
                        hidden.Add(unit.Info.ObjectId);
                    _room.SendToTeam(team, new S_Vision {
                        ObjectId = unit.Info.ObjectId,
                        Visible = visible,
                        Position = unit.Info.Position,
                        Hp = unit.Info.Hp,
                        MaxHp = unit.Info.MaxHp,
                    });
                }

                hidden.RemoveWhere(id => _room.FindObject(id) == null);
            }
        }

        public void SendHiddenTo(Server.ClientSession session, int team)
        {
            if (!Enabled || session == null || !_hidden.TryGetValue(team, out HashSet<ulong> hidden))
                return;

            foreach (ulong id in hidden)
                if (_room.FindObject(id) is GameObject unit && unit.Info != null)
                    session.Send(new S_Vision { ObjectId = id, Visible = false, Position = unit.Info.Position,
                                                Hp = unit.Info.Hp, MaxHp = unit.Info.MaxHp });
        }

        public static ulong SubjectOf(IMessage packet)
        {
            switch (packet)
            {
            case S_SkillResult result:
                return result.CasterId;
            case S_SummonerSpell spell:
                return spell.CasterId;
            case S_ProjectileSpawn spawn:
                return spawn.CasterId;
            case S_Buff buff:
                return buff.TargetId;
            case S_HpUpdate hp:
                return hp.TargetId;
            case S_MovementSnapshot snapshot:
                return snapshot.ObjectId;
            case S_UnitStats stats:
                return stats.TargetId;
            default:
                return 0;
            }
        }
    }
}
