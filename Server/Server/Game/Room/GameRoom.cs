// 게임 방. 입장, 이동, 스킬 메시지를 처리한다.
using Google.Protobuf;
using Google.Protobuf.Protocol;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Diagnostics;
using System.Numerics;
using Server.Game.Objects;
using Server.Game.Navigation;
using Server.Game.Movement;
using Server.Game.Vision;
using Server.Game.Map;
using System.Threading;
using ServerCore;

using Vec3 = System.Numerics.Vector3;
using System.Xml.Linq;
using System.IO;

public static class Vector3Extensions
{
    public static Google.Protobuf.Protocol.Vector3 ToProtoVector3(this System.Numerics.Vector3 vec)
    {
        return new Google.Protobuf.Protocol.Vector3 { X = vec.X, Y = vec.Y, Z = vec.Z };
    }

    public static System.Numerics.Vector3 ToNumericsVector3(this Google.Protobuf.Protocol.Vector3 vec)
    {
        return vec != null ? new System.Numerics.Vector3(vec.X, vec.Y, vec.Z) : System.Numerics.Vector3.Zero;
    }
}

namespace Server.Game.Room
{
    public class GameRoom : JobSerializer
    {
        private Dictionary<ulong, Player> _players = new Dictionary<ulong, Player>();
        private List<Projectile> _projectiles = new List<Projectile>();
        private object _lock = new object();
        public Tilemap _tilemap = new Tilemap();

        public GameRoom(NavigationRegistration navigationRegistration, int? roomId = null)
        {
            if (navigationRegistration == null)
                throw new ArgumentNullException(nameof(navigationRegistration));

            RoomId = roomId ?? navigationRegistration.RoomId;
            _clockBase = Stopwatch.GetTimestamp();
            SetTimerClock(
                () => (int)((Stopwatch.GetTimestamp() - ClockOffset - _clockBase) * 1000 / Stopwatch.Frequency));
            SceneId = navigationRegistration.SceneId;
            NavigationMapId = navigationRegistration.NavigationMapId;
            Navigation =
                navigationRegistration.Asset ??
                throw new ArgumentException("Navigation registration has no asset.", nameof(navigationRegistration));
            Vision = new VisionSystem(this, null);
        }

        public VisionSystem Vision { get; }

        public MapDirector Map { get; set; }

        public Dictionary<int, SurrenderVote> SurrenderVotes { get; } = new Dictionary<int, SurrenderVote>();

        public bool Practice { get; set; } = Environment.GetEnvironmentVariable("DAWN_PRACTICE") == "1";
        public List<TrainingDummy> Dummies { get; } = new List<TrainingDummy>();
        public Dictionary<int, long> SurrenderCooldown { get; } = new Dictionary<int, long>();

        public MapLayout Layout { get; set; }

        public Server.Game.Demo.DemoDirector Demo { get; set; }

        public Server.Game.Bots.BotDirector Bots { get; set; }

        public EpicBuffs EpicBuffs { get; } = new EpicBuffs();
        // K/D/A, creep score and the game clock (S_Scoreboard).
        private Scoreboard _scoreboard;
        public Scoreboard Scoreboard => _scoreboard ??= new Scoreboard(this);

        private Announcer _announcer;
        public Announcer Announcer => _announcer ??= new Announcer(this);
        public IEnumerable<GameObject> Units => _players.Values;
        // Tests: receives what SendToTeam would send.
        internal Action<int, IMessage> TeamPacketSink { get; set; }

        public void SendToTeam(int team, IMessage packet)
        {
            if (TeamPacketSink != null)
            {
                TeamPacketSink(team, packet);

                return;
            }

            foreach (Player player in _players.Values)
                if (player.Info?.TeamId == team && player.Session != null && !player.Session.IsDisconnected)
                    player.Session.Send(packet);
        }

        private bool Withheld(Player recipient,
                              ulong subject) => subject != 0 && recipient.Info != null
                                                && Vision.IsHiddenFrom(recipient.Info.TeamId, subject);

        public int RoomId { get; }
        public int SceneId { get; }
        public string NavigationMapId { get; }
        public string NavigationContentHash => Navigation.ContentHashHex;
        public NavGridAsset Navigation { get; }

        private ulong _projectileIdCounter = 1;
        private ulong _moveIdCounter = 1;
        private ulong _serverTick;

        internal Action<S_MovementSnapshot> MovementSnapshotSink { get; set; }
        internal Action<IMessage> PacketBroadcastSink { get; set; }
        public ulong ServerTick => _serverTick;
        public float LastMovementDeltaTime { get; private set; }
        public MovementDeltaTimeStatus LastMovementDeltaTimeStatus { get; private set; }
        public int LastMovementSnapshotCount { get; private set; }
        public long LastMovementSnapshotPayloadBytes { get; private set; }
        public int LastPathfindingExpandedNodes { get; private set; }
        public double LastPathfindingElapsedMilliseconds { get; private set; }

        public ulong GenerateProjectileId() => _projectileIdCounter++;

        private ulong _unitIdCounter = 1_000_000;
        public ulong GenerateUnitId() => _unitIdCounter++;

        public ulong GenerateMoveId()
        {
            if (_moveIdCounter == ulong.MaxValue)
                throw new InvalidOperationException("ServerMoveId space exhausted for Room " + RoomId + ".");

            return _moveIdCounter++;
        }

        public void Init(string contentRoot)
        {
            if (!NavigationContentPath.TryResolveAssetPath(contentRoot, "Legacy/TilemapData.txt",
                                                           out string tilemapPath, out string pathError))
            {
                throw new InvalidOperationException("Legacy Tilemap path is invalid: " + pathError);
            }

            if (!_tilemap.LoadFile(tilemapPath))
                throw new InvalidOperationException("Required legacy Tilemap failed to load for Room " + RoomId + ".");

            Console.WriteLine("[Navigation] Room " + RoomId + ": " + Navigation.Width + " x " + Navigation.Height +
                              " cells of " + Navigation.CellSize + " from (" + Navigation.Origin.X + ", " +
                              Navigation.Origin.Z + ").");
        }

        public void Update()
        {
            Update(0.1f);
        }

        private readonly long _clockBase;
        private long _pausedTicks, _pauseStartedAt;
        public bool Paused => _pauseStartedAt != 0;
        public long ClockOffset =>
            _pausedTicks + (_pauseStartedAt != 0 ? Stopwatch.GetTimestamp() - _pauseStartedAt : 0);

        private void Pause()
        {
            if (_pauseStartedAt == 0)
                _pauseStartedAt = Stopwatch.GetTimestamp();
        }

        private void Resume()
        {
            if (_pauseStartedAt == 0)
                return;
            _pausedTicks += Stopwatch.GetTimestamp() - _pauseStartedAt;
            _pauseStartedAt = 0;
            BroadcastNow(new S_GamePause { Paused = false });
            Console.WriteLine($"[Room {RoomId}] Resumed.");
        }

        public sealed class HumanSlot
        {
            public string Name = "";
            public Player Player;
            public bool Connected;
            public long DisconnectedAt;
        }

        public static float ReconnectWaitSeconds =
            float.TryParse(Environment.GetEnvironmentVariable("DAWN_RECONNECT_SECONDS"), out float wait) && wait >= 0f
                ? wait
                : 90f;
        private readonly Dictionary<string, HumanSlot> _humans =
            new Dictionary<string, HumanSlot>(StringComparer.OrdinalIgnoreCase);
        private long _waitEndsAt;
        private float _pauseAnnounceTimer;
        public IReadOnlyCollection<HumanSlot> Humans => _humans.Values;
        public bool IsLobbyGame => _humans.Count > 0;
        public int ConnectedHumans => _humans.Values.Count(h => h.Connected);

        public bool WaitingForReconnect =>
            _humans.Values.Any(h => !h.Connected && h.DisconnectedAt != 0 && h.Player?.Room == this);

        public bool Closed { get; private set; }
        public string ClosedReason { get; private set; } = "";

        private string _hostName = "";
        public string HostName
        {
            get => _hostName;
            set {
                if (string.Equals(_hostName, value ?? "", StringComparison.Ordinal))
                    return;
                _hostName = value ?? "";
                BroadcastNow(new S_RoomInfo { HostName = _hostName, RoomId = RoomId, RoomName = RoomName });
            }
        }

        public string RoomName { get; set; } = "";

        public event Action<IReadOnlyList<string>> WaitExpired;

        private volatile string[] _rejoinable = Array.Empty<string>();
        private void RefreshRejoinable() => _rejoinable =
            _humans.Values.Where(h => !h.Connected && h.Player?.Room == this).Select(h => h.Name).ToArray();
        public bool IsWaitingFor(string name) =>
            !Closed && !(Map?.IsOver ?? false) &&
            _rejoinable.Any(n => string.Equals(n, name ?? "", StringComparison.OrdinalIgnoreCase));

        private void BroadcastNow(IMessage packet)
        {
            if (PacketBroadcastSink != null)
            {
                PacketBroadcastSink(packet);

                return;
            }

            foreach (var player in _players.Values)
                if (player.Session != null && !player.Session.IsDisconnected)
                    player.Session.Send(packet);
        }

        private S_GamePause PauseState()
        {
            var state = new S_GamePause {
                Paused = Paused,
                SecondsLeft =
                    (float)Math.Max(0.0, (_waitEndsAt - Stopwatch.GetTimestamp()) / (double)Stopwatch.Frequency),
            };

            foreach (HumanSlot slot in _humans.Values.Where(h => !h.Connected && h.DisconnectedAt != 0 &&
                                                                 h.Player?.Room == this))
                state.WaitingFor.Add(slot.Name);

            return state;
        }

        public void OnSessionDisconnected(ClientSession session)
        {
            Push(() =>
                 {
                     Player player = session.Player;

                     if (player?.Info == null || !ReferenceEquals(player.Session, session))
                         return;

                     if (!_humans.TryGetValue(player.Info.Name, out HumanSlot slot) || Closed)
                     {
                         RemoveObject(player.Info.ObjectId);

                         return;
                     }

                     slot.Connected = false;
                     RefreshRejoinable();
                     player.Session = null;
                     session.Player = null;
                     session.GameRoom = null;
                     PlayerMovementStep cancellation = player.CancelMovement();

                     if (cancellation.HasSnapshot)
                         BroadcastMovementSnapshot(player, cancellation);
                     Recall.Cancel(this, player);

                     if (Map?.IsOver ?? false)
                         return;
                     slot.DisconnectedAt = Stopwatch.GetTimestamp();
                     _waitEndsAt = slot.DisconnectedAt + (long)(ReconnectWaitSeconds * Stopwatch.Frequency);
                     Console.WriteLine($"[Room {RoomId}] {slot.Name} disconnected: waiting {ReconnectWaitSeconds:0}s.");

                     if (ReconnectWaitSeconds > 0f)
                     {
                         Pause();
                         _pauseAnnounceTimer = 0f;
                         BroadcastNow(PauseState());
                     }
                 });
        }

        private void UpdatePause(float deltaTime)
        {
            bool waiting = _humans.Values.Any(h => !h.Connected && h.DisconnectedAt != 0 && h.Player?.Room == this);

            if (!waiting)
            {
                Resume();

                return;
            }

            if (Stopwatch.GetTimestamp() >= _waitEndsAt)
            {
                var gone =
                    _humans.Values.Where(h => !h.Connected && h.DisconnectedAt != 0).Select(h => h.Name).ToList();

                foreach (HumanSlot slot in _humans.Values)
                    if (!slot.Connected)
                        slot.DisconnectedAt = 0;
                Console.WriteLine($"[Room {RoomId}] The wait for {string.Join(", ", gone)} is over.");
                Resume();
                WaitExpired?.Invoke(gone);

                return;
            }

            _pauseAnnounceTimer -= deltaTime;

            if (_pauseAnnounceTimer <= 0f)
            {
                _pauseAnnounceTimer = 1f;
                BroadcastNow(PauseState());
            }
        }

        public void ForceEnd(string reason)
        {
            Push(() =>
                 {
                     if (Closed)
                         return;
                     Closed = true;
                     ClosedReason = reason ?? "";

                     if (Paused)
                         Resume();
                     BroadcastNow(new S_RoomClosed { Reason = ClosedReason });
                     Console.WriteLine($"[Room {RoomId}] Closed: {ClosedReason}");
                 });
        }

        private void Rejoin(ClientSession session, Server.Game.Lobbies.GameReservation reservation)
        {
            if (!_humans.TryGetValue(reservation.Name, out HumanSlot slot) || slot.Player?.Room != this ||
                slot.Connected)
            {
                Console.WriteLine($"[Room {RoomId}] Rejoin of {reservation.Name} refused (no waiting champion).");
                session.Disconnect();

                return;
            }

            Player player = slot.Player;
            slot.Connected = true;
            slot.DisconnectedAt = 0;
            RefreshRejoinable();
            player.Session = session;
            session.ResetMoveSequence();
            session.GameRoom = this;
            session.Player = player;

            foreach (Player other in _players.Values)
                if (Progression.IsChampion(other))
                    other.Items.Dirty = true;
            SendWorldTo(session, player);

            for (int skillId = 1; skillId <= 4; ++skillId)
            {
                double left = player.CooldownRemaining(skillId);

                if (left > 0.05)
                    session.Send(new S_SkillCooldown { SkillId = skillId, Duration = (float)left });
            }

            Console.WriteLine($"[Room {RoomId}] {slot.Name} is back.");

            if (Paused)
                BroadcastNow(PauseState());
        }

        private void SendWorldTo(ClientSession session, Player player)
        {
            if (player.Room == this && Practice)
                session.Send(new S_EnterGame {
                    Success = true,
                    AccountId = (ulong)session.SessionId,
                    FogOfWar = Server.Game.Vision.VisionSystem.FogEnabled,
                    Practice = true,
                    ProtocolVersion = ProtocolInfo.Version,
                });
            session.Send(new S_MyPlayer { Info = player.Info });

            foreach (Turret turret in Map?.Turrets ?? Array.Empty<Turret>() as IReadOnlyList<Turret>)
                if (turret.Plates > 0 && turret.IsAlive)
                    session.Send(new S_Buff { TargetId = turret.Info.ObjectId, BuffType = BUFF_TYPE.BuffTurretPlates,
                                              SkillId = Buffs.PassiveSkillId, Amount = turret.Plates,
                                              Duration = 3600f });
            session.Send(new S_NavigationInfo {
                RoomId = RoomId,
                FormatVersion = Navigation.FormatVersion,
                NavigationMapId = NavigationMapId,
                NavigationContentHash = NavigationContentHash,
            });
            S_AddObject existing = new S_AddObject();

            foreach (var p in _players.Values)
                if (p.Info.ObjectId != player.Info.ObjectId)
                    existing.Objects.Add(p.Info);
            session.Send(existing);
            Vision.SendHiddenTo(session, player.Info.TeamId);
            session.Send(Progression.BuildStats(player, true));

            foreach (var p in _players.Values)
                if (p != player && Progression.IsChampion(p))
                    session.Send(Progression.BuildStats(p, false));
            EpicBuffs.SendAll(this);
            SummonerSpells.SendCooldowns(this, player);
            Trinket.Send(player);

            if (IsLobbyGame)
                session.Send(new S_RoomInfo { HostName = _hostName, RoomId = RoomId, RoomName = RoomName });
        }

        public void Update(float rawDeltaTime)
        {
            long previous = GameClock.Swap(ClockOffset);
            try
            {
                UpdateTick(rawDeltaTime);
            }
            finally
            {
                GameClock.Restore(previous);
            }
        }

        private void UpdateTick(float rawDeltaTime)
        {
            LastMovementDeltaTime =
                ServerMovementSettings.SanitizeDeltaTime(rawDeltaTime, out MovementDeltaTimeStatus deltaStatus);
            LastMovementDeltaTimeStatus = deltaStatus;
            LastMovementSnapshotCount = 0;
            LastMovementSnapshotPayloadBytes = 0;
            LastPathfindingExpandedNodes = 0;
            LastPathfindingElapsedMilliseconds = 0.0;
            _serverTick++;

            if (deltaStatus == MovementDeltaTimeStatus.Clamped)
                Console.WriteLine("[Movement] Delta time clamped. Room=" + RoomId + " Raw=" + rawDeltaTime +
                                  " Applied=" + LastMovementDeltaTime);
            else if (deltaStatus == MovementDeltaTimeStatus.Invalid)
                Console.WriteLine("[Movement] Invalid delta time ignored. Room=" + RoomId + " Raw=" + rawDeltaTime);

            Flush();

            if (Paused)
            {
                UpdatePause(rawDeltaTime);

                return;
            }

            foreach (var player in _players.Values.ToList())
            {
                if (player.Room != this)
                    continue;
                try
                {
                    Progression.Update(this, player, LastMovementDeltaTime);
                    Buffs.Update(this, player, LastMovementDeltaTime);
                    SummonerSpells.Update(this, player);

                    if (Progression.IsChampion(player))
                    {
                        Trinket.Recharge(player);
                        Server.Game.Items.ItemEffects.Update(this, player, LastMovementDeltaTime);
                        Recall.Update(this, player);
                        Server.Game.Map.EpicBuffs.UpdateSoul(this, player);
                    }

                    if (player is Tibbers tibbers)
                        tibbers.Think(LastMovementDeltaTime);
                    else if (player is MapUnit unit && !(Map?.IsOver ?? false))
                        unit.Think(LastMovementDeltaTime);
                    PlayerMovementStep step = player.Update(LastMovementDeltaTime);

                    if (step.HasSnapshot)
                        BroadcastMovementSnapshot(player, step);
                }
                catch (Exception exception)
                {
                    Console.WriteLine(
                        $"[Room] Update of {player.Info?.ObjectId} ({player.Info?.ChampType}) failed: {exception}");
                }
            }

            UpdateProjectiles();
            Surrender.Update(this);
            Map?.Update(LastMovementDeltaTime);
            Demo?.Update(LastMovementDeltaTime);
            Bots?.Update(LastMovementDeltaTime);
            Vision.Update(LastMovementDeltaTime);

            if (LastPathfindingElapsedMilliseconds >= 10.0)
                Console.WriteLine("[Movement] Slow pathfinding in Room tick. Room=" + RoomId +
                                  " Expanded=" + LastPathfindingExpandedNodes +
                                  " ElapsedMs=" + LastPathfindingElapsedMilliseconds.ToString("F3"));
        }

        internal void RecordPathfindingMetrics(int expandedNodes, double elapsedMilliseconds)
        {
            if (expandedNodes > 0)
                LastPathfindingExpandedNodes += expandedNodes;

            if (!double.IsNaN(elapsedMilliseconds) && !double.IsInfinity(elapsedMilliseconds) &&
                elapsedMilliseconds > 0.0)
                LastPathfindingElapsedMilliseconds += elapsedMilliseconds;
        }

        public bool TryGatherPlayersForCombatTest(Player requester, out string resultMessage)
        {
            resultMessage = string.Empty;

            if (requester?.Info?.Position == null || !ContainsPlayer(requester))
            {
                resultMessage = "Combat gather rejected: requester is not registered in this Room.";

                return false;
            }

            if (_players.Values.Count(Progression.IsChampion) < 2)
            {
                resultMessage = "Combat gather waiting: at least two connected players are required.";

                return false;
            }

            if (!Navigation.TryWorldToCell(requester.Info.Position.ToNumericsVector3(),
                                           out NavGridCoordinate anchorCell) ||
                !Navigation.IsWalkable(anchorCell.X, anchorCell.Z))
            {
                resultMessage = "Combat gather rejected: requester is outside a Walkable NavGrid Cell.";

                return false;
            }

            List<Player> orderedPlayers =
                _players.Values.Where(Progression.IsChampion).OrderBy(player => player.Info.ObjectId).ToList();
            orderedPlayers.Remove(requester);
            orderedPlayers.Insert(0, requester);

            if (!TryFindCombatTestCells(anchorCell, orderedPlayers.Count, out List<NavGridCoordinate> targetCells))
            {
                resultMessage = "Combat gather rejected: not enough nearby Walkable Cells.";

                return false;
            }

            var targetPositions = new List<Vec3>(targetCells.Count);

            for (int index = 0; index < targetCells.Count; ++index)
            {
                if (!Navigation.TryGetCellWorldCenter(targetCells[index].X, targetCells[index].Z,
                                                      out Vec3 targetPosition))
                {
                    resultMessage = "Combat gather rejected: failed to resolve a target Cell center.";

                    return false;
                }

                float authoritativeY = orderedPlayers[index].Info.Position?.Y ?? Navigation.Origin.Y;
                targetPosition.Y = authoritativeY;
                targetPositions.Add(targetPosition);
            }

            for (int index = 0; index < orderedPlayers.Count; ++index)
            {
                Player player = orderedPlayers[index];
                NavGridCoordinate cell = targetCells[index];
                Vec3 position = targetPositions[index];
                player.CancelMovement();

                int previousTileX = player.TileX;
                int previousTileZ = player.TileZ;
                player.Info.Position = position.ToProtoVector3();
                player.TileX = cell.X;
                player.TileZ = cell.Z;
                UpdatePlayerTilePosition(player, previousTileX, previousTileZ, cell.X, cell.Z);
                player.Info.State = OBJECT_STATE_TYPE.Idle;

                BroadcastCombatTestTeleportSnapshot(player, GenerateMoveId());
            }

            resultMessage =
                "Combat gather complete: " + orderedPlayers.Count + " players placed near Cell " + anchorCell + ".";
            Console.WriteLine("[CombatTest] Room=" + RoomId + " Requester=" + requester.Info.ObjectId +
                              " Players=" + orderedPlayers.Count + " Anchor=" + anchorCell);

            return true;
        }

        private bool TryFindCombatTestCells(NavGridCoordinate anchorCell, int requiredCount,
                                            out List<NavGridCoordinate> result)
        {
            const int maxVisitedCells = 512;
            result = new List<NavGridCoordinate>(requiredCount);

            if (requiredCount <= 0 || requiredCount > maxVisitedCells)
                return false;

            var queue = new Queue<NavGridCoordinate>();
            var visited = new HashSet<NavGridCoordinate>();
            queue.Enqueue(anchorCell);
            visited.Add(anchorCell);
            var neighborOffsets = new[] {
                new NavGridCoordinate(1, 0),
                new NavGridCoordinate(-1, 0),
                new NavGridCoordinate(0, 1),
                new NavGridCoordinate(0, -1),
            };

            while (queue.Count > 0 && visited.Count <= maxVisitedCells)
            {
                NavGridCoordinate current = queue.Dequeue();

                if (Navigation.IsWalkable(current.X, current.Z))
                {
                    result.Add(current);

                    if (result.Count == requiredCount)
                        return true;
                }

                foreach (NavGridCoordinate offset in neighborOffsets)
                {
                    var neighbor = new NavGridCoordinate(current.X + offset.X, current.Z + offset.Z);

                    if (Navigation.IsValidCell(neighbor.X, neighbor.Z) && visited.Add(neighbor))
                        queue.Enqueue(neighbor);
                }
            }

            result.Clear();

            return false;
        }

        private void BroadcastCombatTestTeleportSnapshot(Player player, ulong serverMoveId)
        {
            var snapshot = new S_MovementSnapshot {
                ObjectId = player.Info.ObjectId,
                ServerMoveId = serverMoveId,
                ServerTick = _serverTick,
                Position = player.Info.Position.Clone(),
                MovementState = MOVEMENT_SNAPSHOT_STATE.Arrived,
                CurrentWaypointIndex = 0,
                ClientMoveSequence = player.Session?.LastProcessedMoveSequence ?? 0,
                RoomId = RoomId,
            };

            LastMovementSnapshotCount++;
            LastMovementSnapshotPayloadBytes += snapshot.CalculateSize() + 4L;

            if (MovementSnapshotSink != null)
            {
                MovementSnapshotSink(snapshot);

                return;
            }

            foreach (Player roomPlayer in _players.Values)
            {
                if (roomPlayer.Session != null && !roomPlayer.Session.IsDisconnected &&
                    !Withheld(roomPlayer, snapshot.ObjectId))
                    roomPlayer.Session.Send(snapshot);
            }
        }

        public void TeleportPlayer(Player player, Vec3 position)
        {
            CancelPlayerMovement(player);
            player.Info.Position = position.ToProtoVector3();

            if (Navigation.TryWorldToCell(position, out NavGridCoordinate cell))
            {
                int previousX = player.TileX, previousZ = player.TileZ;
                player.TileX = cell.X;
                player.TileZ = cell.Z;
                UpdatePlayerTilePosition(player, previousX, previousZ, cell.X, cell.Z);
            }
        }

        public void CancelPlayerMovement(Player player)
        {
            if (player == null || !ContainsPlayer(player))
                return;
            PlayerMovementStep step = player.CancelMovement();

            if (step.HasSnapshot)
                BroadcastMovementSnapshot(player, step);
        }

        private void BroadcastMovementSnapshot(Player player, PlayerMovementStep step)
        {
            var snapshot = new S_MovementSnapshot {
                ObjectId = player.Info.ObjectId,
                ServerMoveId = step.ServerMoveId,
                ServerTick = _serverTick,
                Position = step.Position.ToProtoVector3(),
                MovementState = step.SnapshotState,
                CurrentWaypointIndex = checked((uint)Math.Max(0, step.CurrentWaypointIndex)),
                ClientMoveSequence = step.ClientMoveSequence,
                RoomId = RoomId,
            };

            LastMovementSnapshotCount++;
            LastMovementSnapshotPayloadBytes += snapshot.CalculateSize() + 4L;

            if (MovementSnapshotSink != null)
            {
                MovementSnapshotSink(snapshot);

                return;
            }

            foreach (var roomPlayer in _players.Values)
            {
                if (roomPlayer.Session != null && !roomPlayer.Session.IsDisconnected &&
                    !Withheld(roomPlayer, snapshot.ObjectId))
                    roomPlayer.Session.Send(snapshot);
            }
        }

        private void UpdateProjectiles()
        {
            List<Projectile> toRemove = new List<Projectile>();

            foreach (var projectile in _projectiles)
            {
                projectile.Update(LastMovementDeltaTime);
            }

            foreach (Skillshot skillshot in _skillshots.ToArray())
                skillshot.Update(LastMovementDeltaTime);
        }

        public void EnterRoom(ClientSession session, Server.Game.Lobbies.GameReservation reservation = null)
        {
            Push(
                () =>
                {
                    if (reservation?.Rejoin == true)
                    {
                        Rejoin(session, reservation);

                        return;
                    }

                    if (Closed)
                    {
                        session.Disconnect();

                        return;
                    }

                    int teamId =
                        reservation?.Team ?? ((_players.Values.Count(Progression.IsChampion) % 2 == 0) ? 1 : 2);
                    Random _random = new Random();
                    Vec3 spawnPos = teamId == 1 ? new Vec3(_random.Next(6, 21), 2, _random.Next(3, 11))
                                                : new Vec3(_random.Next(120, 124), 2, _random.Next(120, 124));

                    if (Layout != null && Layout.Spawns.TryGetValue(teamId, out Vec3 fountain))
                    {
                        double angle = _random.NextDouble() * Math.PI * 2.0, distance = _random.NextDouble() * 1.5;
                        spawnPos = MapUnit.Walkable(this, new Vec3(fountain.X + (float)(Math.Cos(angle) * distance), 2f,
                                                                   fountain.Z + (float)(Math.Sin(angle) * distance)));
                    }

                    PLAYER_CHAMPION_TYPE champType = reservation?.Champion ?? ChampionRegistry.DefaultFor(teamId);
                    ChampionData champion = ChampionDatabase.Get(champType);
                    Player player = new Player {
                        Info = new ObjectInfo { ObjectId = (ulong)session.SessionId, ChampType = champType,
                                                State = OBJECT_STATE_TYPE.Idle, ObjType = OBJECT_TYPE.Player,
                                                Name = reservation?.Name ?? $"Client_Yeop_{session.SessionId}",
                                                SkinId = reservation?.SkinId ?? ChampionRegistry.DefaultSkinFor(teamId),
                                                Position = spawnPos.ToProtoVector3(), TeamId = teamId },
                        Session = session, Room = this
                    };
                    player.TrySetMovementSpeed(champion.MoveSpeedUnits);
                    Progression.Init(player, spawnPos);
                    SummonerSpells.Assign(player, reservation?.Spells);
                    Console.WriteLine(
                        $"[Spells] {player.Info.Name}: D {player.Info.SummonerSpells[0]}, F {player.Info.SummonerSpells[1]}");

                    session.ResetMoveSequence();
                    session.GameRoom = this;
                    session.Player = player;

                    _players[player.Info.ObjectId] = player;

                    foreach (Player other in _players.Values)
                        if (Progression.IsChampion(other))
                            other.Items.Dirty = true;

                    if (Navigation.TryWorldToCell(spawnPos, out NavGridCoordinate spawnCell))
                    {
                        player.TileX = spawnCell.X;
                        player.TileZ = spawnCell.Z;
                        UpdatePlayerTilePosition(player, spawnCell.X, spawnCell.Z, spawnCell.X, spawnCell.Z);
                    }

                    if (reservation?.Practice == true)
                        Practice = true;

                    if (reservation != null && !string.IsNullOrEmpty(reservation.Name))
                        _humans[reservation.Name] =
                            new HumanSlot { Name = reservation.Name, Player = player, Connected = true };

                    if (Bots == null && reservation?.Bots != null)
                        Bots = new Server.Game.Bots.BotDirector(this, reservation.Bots);
                    // 개별 ?�송
                    session.Send(new S_MyPlayer { Info = player.Info });
                    // Outer turrets' plates, for the HUD.
                    foreach (Turret turret in Map?.Turrets ?? Array.Empty<Turret>() as IReadOnlyList<Turret>)
                        if (turret.Plates > 0 && turret.IsAlive)
                            session.Send(new S_Buff { TargetId = turret.Info.ObjectId,
                                                      BuffType = BUFF_TYPE.BuffTurretPlates,
                                                      SkillId = Buffs.PassiveSkillId, Amount = turret.Plates,
                                                      Duration = 3600f });
                    session.Send(new S_NavigationInfo {
                        RoomId = RoomId,
                        FormatVersion = Navigation.FormatVersion,
                        NavigationMapId = NavigationMapId,
                        NavigationContentHash = NavigationContentHash,
                    });

                    S_AddObject existingPlayersPacket = new S_AddObject();

                    foreach (var p in _players.Values)
                    {
                        if (p.Info.ObjectId != player.Info.ObjectId)
                            existingPlayersPacket.Objects.Add(p.Info);
                    }

                    session.Send(existingPlayersPacket);
                    Vision.SendHiddenTo(session, player.Info.TeamId);

                    if (IsLobbyGame)
                        session.Send(new S_RoomInfo { HostName = _hostName, RoomId = RoomId, RoomName = RoomName });

                    if (reservation?.Practice == true)
                        session.Send(new S_EnterGame {
                            Success = true,
                            AccountId = (ulong)session.SessionId,
                            FogOfWar = Server.Game.Vision.VisionSystem.FogEnabled,
                            Practice = true,
                            ProtocolVersion = ProtocolInfo.Version,
                        });
                    Map?.Start();

                    S_AddObject newPlayerPacket = new S_AddObject();
                    newPlayerPacket.Objects.Add(player.Info);
                    Broadcast(newPlayerPacket, player.Info.ObjectId);

                    session.Send(Progression.BuildStats(player, true));

                    foreach (var p in _players.Values)
                        if (p != player && Progression.IsChampion(p))
                            session.Send(Progression.BuildStats(p, false));
                    EpicBuffs.SendAll(this);
                    SummonerSpells.SendCooldowns(this, player);
                    Trinket.Send(player);
                    Scoreboard.Start();
                    Scoreboard.Send();
                });
        }

        public Player SpawnBotChampion(PLAYER_CHAMPION_TYPE champType, int skinId, int teamId, string name,
                                       IReadOnlyList<SUMMONER_SPELL> spells = null)
        {
            Vec3 spawnPos = Layout != null && Layout.Spawns.TryGetValue(teamId, out Vec3 fountain)
                                ? MapUnit.Walkable(this, new Vec3(fountain.X, 2f, fountain.Z))
                                : new Vec3(10f, 2f, 10f);
            ChampionData champion = ChampionDatabase.Get(champType);
            Player player =
                new Player { Info = new ObjectInfo { ObjectId = GenerateUnitId(), ChampType = champType,
                                                     State = OBJECT_STATE_TYPE.Idle, ObjType = OBJECT_TYPE.Player,
                                                     Name = name, SkinId = skinId, Position = spawnPos.ToProtoVector3(),
                                                     TeamId = teamId },
                             Room = this };
            player.TrySetMovementSpeed(champion.MoveSpeedUnits);
            Progression.Init(player, spawnPos);
            SummonerSpells.Assign(player, spells);
            _players[player.Info.ObjectId] = player;

            foreach (Player other in _players.Values)
                if (Progression.IsChampion(other))
                    other.Items.Dirty = true;

            if (Navigation.TryWorldToCell(spawnPos, out NavGridCoordinate spawnCell))
            {
                player.TileX = spawnCell.X;
                player.TileZ = spawnCell.Z;
                UpdatePlayerTilePosition(player, spawnCell.X, spawnCell.Z, spawnCell.X, spawnCell.Z);
            }

            S_AddObject added = new S_AddObject();
            added.Objects.Add(player.Info);
            Broadcast(added);

            foreach (var p in _players.Values)
                if (p.Session != null)
                    p.Session.Send(Progression.BuildStats(player, false));
            Scoreboard.Send();

            return player;
        }

        public bool CanGo(Vec3 cellPos)
        {
            Tile? tile = _tilemap.GetTileAt(cellPos);

            if (tile == null)
            {
                Console.WriteLine($"[Server] CanGo Failed: No tile at position {cellPos}");

                return false;
            }

            return tile.IsWalkable;
        }

        public Vec3 GetRandomEmptyCellPos()
        {
            Vector2 size = _tilemap._mapSize;

            while (true)
            {
                int x = new Random().Next(0, (int)size.X);
                int z = new Random().Next(0, (int)size.Y);
                Vec3 cellPos = new Vec3(x, 2, z);

                if (CanGo(cellPos))
                    return cellPos;
            }
        }

        public void AddObject(GameObject obj)
        {
            Push(() =>
                 {
                     ulong id = obj.Info.ObjectId;

                     if (_players.ContainsKey(id))
                         return;

                     if (obj is Player player)
                         _players[id] = player;

                     obj.Room = this;
                 });
        }

        public void RemoveObject(ulong id)
        {
            Push(() =>
                 {
                     if (_players.Remove(id, out Player player))
                     {
                         PlayerMovementStep cancellation = player.CancelMovement();

                         if (cancellation.HasSnapshot)
                             BroadcastMovementSnapshot(player, cancellation);
                         player.Room = null;

                         if (player.Session != null && ReferenceEquals(player.Session.Player, player))
                         {
                             player.Session.Player = null;

                             if (ReferenceEquals(player.Session.GameRoom, this))
                                 player.Session.GameRoom = null;
                         }

                         S_RemoveObject removePacket = new S_RemoveObject();
                         removePacket.Ids.Add(id);
                         Broadcast(removePacket);

                         if (Progression.IsChampion(player) && !_players.Values.Any(Progression.IsChampion))
                         {
                             Map?.Reset();
                             Scoreboard.Reset();
                             Announcer.Reset();
                         }
                     }
                 });
        }

        public bool ContainsPlayer(Player player)
        {
            if (player?.Info == null)
                return false;

            return _players.TryGetValue(player.Info.ObjectId, out Player registered) &&
                   ReferenceEquals(registered, player);
        }

        public GameObject FindObject(ulong id)
        {
            _players.TryGetValue(id, out var go);

            return go;
        }

        public void Broadcast(IMessage packet)
        {
            Push(() =>
                 {
                     if (PacketBroadcastSink != null)
                     {
                         PacketBroadcastSink(packet);

                         return;
                     }

                     ulong subject = VisionSystem.SubjectOf(packet);

                     foreach (var player in _players.Values)
                         if (player.Session != null && !player.Session.IsDisconnected && !Withheld(player, subject))
                             player.Session.Send(packet);
                 });
        }

        public void Broadcast(IMessage packet, ulong excludeId)
        {
            Push(() =>
                 {
                     ulong subject = VisionSystem.SubjectOf(packet);

                     foreach (var player in _players.Values)
                     {
                         if (player.Info.ObjectId == excludeId || player.Session == null ||
                             player.Session.IsDisconnected || Withheld(player, subject))
                             continue;
                         player.Session.Send(packet);
                     }
                 });
        }

        public void UpdatePlayerTilePosition(GameObject player, int prevX, int prevZ, int newX, int newZ)
        {
            if (player is MapUnit)
                return;
            Tile prevTile = _tilemap.GetTileAt(new Vec3(prevX, 0, prevZ));
            prevTile?.RemovePlayer(player);

            Tile newTile = _tilemap.GetTileAt(new Vec3(newX, 0, newZ));
            newTile?.AddPlayer(player);
        }

        public List<GameObject> GetObjectsInRange(Vec3 center, float range)
        {
            List<GameObject> objectsInRange = new List<GameObject>();

            foreach (var obj in _players)
            {
                Vec3 position = obj.Value.Info.Position.ToNumericsVector3();
                float dx = position.X - center.X;
                float dz = position.Z - center.Z;
                float distanceSquared = dx * dx + dz * dz;

                if (distanceSquared <= range * range)
                    objectsInRange.Add(obj.Value);
            }

            return objectsInRange;
        }

        public void AddProjectile(Projectile projectile)
        {
            _projectiles.Add(projectile);
            projectile.Room = this;
        }

        private readonly List<Skillshot> _skillshots = new List<Skillshot>();
        public IReadOnlyList<Skillshot> Skillshots => _skillshots;

        public Skillshot AddSkillshot(Skillshot skillshot)
        {
            _skillshots.Add(skillshot);
            skillshot.Room = this;
            skillshot.Announce();

            return skillshot;
        }

        public void RemoveSkillshot(Skillshot skillshot)
        {
            _skillshots.Remove(skillshot);
        }

        public void RemoveProjectile(Projectile projectile)
        {
            _projectiles.Remove(projectile);
        }
    }
}
