using Google.Protobuf.Protocol;
using Server.Game.Items;
using Server.Game.Movement;
using Server.Game.Navigation;
using System;
using System.Diagnostics;
using NumericsVector3 = System.Numerics.Vector3;

namespace Server.Game.Objects
{
    public readonly struct PlayerMovementStep
    {
        private PlayerMovementStep(bool hasSnapshot, ulong serverMoveId, uint clientMoveSequence,
                                   NumericsVector3 position, MOVEMENT_SNAPSHOT_STATE snapshotState,
                                   int currentWaypointIndex)
        {
            HasSnapshot = hasSnapshot;
            ServerMoveId = serverMoveId;
            ClientMoveSequence = clientMoveSequence;
            Position = position;
            SnapshotState = snapshotState;
            CurrentWaypointIndex = currentWaypointIndex;
        }

        public bool HasSnapshot { get; }
        public ulong ServerMoveId { get; }
        public uint ClientMoveSequence { get; }
        public NumericsVector3 Position { get; }
        public MOVEMENT_SNAPSHOT_STATE SnapshotState { get; }
        public int CurrentWaypointIndex { get; }

        public static PlayerMovementStep None => default;

        internal static PlayerMovementStep Create(PlayerMovementState state, NumericsVector3 position,
                                                  MOVEMENT_SNAPSHOT_STATE snapshotState)
        {
            return new PlayerMovementStep(true, state.ServerMoveId, state.ClientMoveSequence, position, snapshotState,
                                          state.CurrentWaypointIndex);
        }
    }

    public class Player : GameObject
    {
        private long _combatActionEndsAt;
        private int _combatActionSkillId = -1;
        public bool IsPerformingCombatAction => Server.Game.GameClock.Now < _combatActionEndsAt;
        public bool IsMovementLockedByCombat =>
            IsPerformingCombatAction &&
            !(Info != null && ChampionRegistry.Find(Info.ChampType)?.MovesWhileCasting(_combatActionSkillId) == true);

        internal float CharmRepathTimer { get; set; }

        internal bool TryBeginCombatAction(int skillId, double durationSeconds)
        {
            if (Info == null || Info.Hp <= 0 || IsPerformingCombatAction)
                return false;
            _combatActionSkillId = skillId;
            _combatActionEndsAt = Server.Game.GameClock.Now + (long)(durationSeconds * Stopwatch.Frequency);

            return true;
        }

        private long _nextBasicAttackAt;
        public int BasicAttackSequence { get; private set; }
        public bool IsInBasicAttackWindup => _combatActionSkillId == 0 && IsPerformingCombatAction;

        internal bool TryBeginBasicAttack(double windupSeconds, double cycleSeconds, double readyToleranceSeconds)
        {
            long now = Server.Game.GameClock.Now;

            if (now + (long)(readyToleranceSeconds * Stopwatch.Frequency) < _nextBasicAttackAt)
                return false;

            if (!TryBeginCombatAction(0, windupSeconds))
                return false;
            _nextBasicAttackAt = now + (long)(cycleSeconds * Stopwatch.Frequency);
            BasicAttackSequence++;

            return true;
        }

        public bool CancelBasicAttackWindup()
        {
            if (!IsInBasicAttackWindup)
                return false;
            _combatActionEndsAt = 0;
            _nextBasicAttackAt = Server.Game.GameClock.Now;
            BasicAttackSequence++;

            return true;
        }

        private readonly long[] _skillReadyAt = new long[5];

        public bool IsSkillReady(int skillId, double toleranceSeconds)
        {
            if (skillId < 1 || skillId >= _skillReadyAt.Length)
                return true;

            return Server.Game.GameClock.Now + (long)(toleranceSeconds * Stopwatch.Frequency) >= _skillReadyAt[skillId];
        }

        public double CooldownRemaining(int skillId) =>
            skillId < 1 || skillId >= _skillReadyAt.Length
                ? 0.0
                : Math.Max(0.0, (_skillReadyAt[skillId] - Server.Game.GameClock.Now) / (double)Stopwatch.Frequency);

        public double ReduceSkillCooldown(int skillId, double seconds)
        {
            if (skillId < 1 || skillId >= _skillReadyAt.Length)
                return 0.0;
            _skillReadyAt[skillId] -= (long)(seconds * Stopwatch.Frequency);

            return CooldownRemaining(skillId);
        }

        public void StartSkillCooldown(int skillId, double cooldownSeconds)
        {
            if (skillId < 1 || skillId >= _skillReadyAt.Length)
                return;
            _skillReadyAt[skillId] = Server.Game.GameClock.Now + (long)(cooldownSeconds * Stopwatch.Frequency);
        }

        public ClientSession Session { get; set; }
        public StatusEffects Status { get; } = new StatusEffects();
        // Champions' summoner spells, D and F (SummonerSpells.Assign).
        public SummonerSlot[] SummonerSlots { get; } = new SummonerSlot[SummonerSpells.SlotCount];
        // Champions' Stealth Ward trinket (Trinket).
        public TrinketState Trinket { get; } = new TrinketState();
        public Progress Progress { get; } = new Progress();

        public Inventory Items { get; } = new Inventory();
        public PingState Pings { get; } = new PingState();
        public ChatState ChatRate { get; } = new ChatState();
        public long RecallEndsAt { get; set; }

        public long HeraldEyeEndsAt { get; set; }
        public ItemEffectState ItemState { get; } = new ItemEffectState();
        public ItemStats ItemStats => ItemEffects.TotalStats(this);
        internal float RegenCarry { get; set; }

        public ulong LastHitTargetId { get; set; }
        public long LastHitAt { get; set; }
        // Move speed after buffs; stunned units do not move.
        public float EffectiveMovementSpeed =>
            Status.IsStunned ? 0f
            : Status.IsDashing
                ? Status.DashSpeed
                : ItemMovementSpeed * Status.MoveSpeedMultiplier * (Room?.EpicBuffs?.MoveSpeedMultiplier(this) ?? 1f);

        private float ItemMovementSpeed
        {
            get {
                var (flat, percent) = ItemEffects.MoveSpeed(this);

                return (MovementSpeed + flat * 0.01f) * (1f + percent);
            }
        }

        public PlayerMovementState MovementState { get; private set; }
        public float MovementSpeed { get; private set; } = ServerMovementSettings.DefaultMovementSpeed;

        public bool TrySetMovementSpeed(float movementSpeed)
        {
            if (!ServerMovementSettings.IsValidSpeed(movementSpeed))
                return false;
            MovementSpeed = movementSpeed;

            return true;
        }

        public void ReplaceMovementState(PlayerMovementState movementState)
        {
            MovementState?.Deactivate();
            MovementState = movementState ?? throw new ArgumentNullException(nameof(movementState));

            if (Info != null && Info.State != OBJECT_STATE_TYPE.Skill)
                Info.State = OBJECT_STATE_TYPE.Move;
        }

        public PlayerMovementStep CancelMovement()
        {
            PlayerMovementState state = MovementState;

            if (state == null || !state.IsActive)
                return PlayerMovementStep.None;

            state.Deactivate();

            if (Info != null && Info.State == OBJECT_STATE_TYPE.Move)
                Info.State = OBJECT_STATE_TYPE.Idle;

            return PlayerMovementStep.Create(state, GetPosition(), MOVEMENT_SNAPSHOT_STATE.Cancelled);
        }

        public void DeactivateMovement()
        {
            CancelMovement();
        }

        public PlayerMovementStep Update(float deltaTime)
        {
            PlayerMovementState state = MovementState;

            if (state == null || !state.IsActive)
                return PlayerMovementStep.None;

            if (Info == null || Info.Position == null || Room == null || Info.Hp <= 0 || IsMovementLockedByCombat ||
                Info.State == OBJECT_STATE_TYPE.Skill)
            {
                return CancelInvalidMovement(state, "Player state or Room is not valid for movement.");
            }

            if (!ServerMovementSettings.IsValidSpeed(MovementSpeed))
                return CancelInvalidMovement(state, "Server movement speed is invalid.");

            NavGridPath path = state.Path;

            if (path == null || path.Cells.Count == 0 || !path.GoalCell.Equals(state.DestinationCell) ||
                state.CurrentWaypointIndex < 0 || state.CurrentWaypointIndex > path.Cells.Count)
            {
                return CancelInvalidMovement(state, "Path metadata or Waypoint index is invalid.");
            }

            NumericsVector3 current = GetPosition();

            if (!IsFinite(current))
                return CancelInvalidMovement(state, "Authoritative position is not finite.");

            if (path.Cells.Count == 1 || state.CurrentWaypointIndex >= path.Cells.Count)
                return CompleteMovement(state);

            if (deltaTime <= 0.0f || MovementSpeed <= 0.0f)
                return PlayerMovementStep.None;

            float remainingTime = deltaTime;
            bool positionChanged = false;

            while (remainingTime > 0.0f && state.CurrentWaypointIndex < path.Cells.Count)
            {
                NavGridCoordinate waypointCell = path.Cells[state.CurrentWaypointIndex];

                if (!Room.Navigation.IsValidCell(waypointCell.X, waypointCell.Z) ||
                    !Room.Navigation.IsWalkable(waypointCell.X, waypointCell.Z) ||
                    !Room.Navigation.TryGetCellWorldCenter(waypointCell.X, waypointCell.Z,
                                                           out NumericsVector3 waypoint))
                {
                    return CancelInvalidMovement(state, "Waypoint is outside Navigation or became Blocked.");
                }

                waypoint.Y = current.Y;
                float dx = waypoint.X - current.X;
                float dz = waypoint.Z - current.Z;
                float distance = (float)Math.Sqrt(dx * dx + dz * dz);

                if (!IsFinite(distance))
                    return CancelInvalidMovement(state, "Waypoint distance is not finite.");

                if (distance <= ServerMovementSettings.WaypointEpsilon)
                {
                    current.X = waypoint.X;
                    current.Z = waypoint.Z;
                    state.AdvanceWaypoint();
                    positionChanged = true;
                    continue;
                }

                float speedMultiplier = Room.Navigation.GetCellType(waypointCell.X, waypointCell.Z) == NavCellType.Slow
                                            ? ServerMovementSettings.SlowCellSpeedMultiplier
                                            : 1.0f;
                float segmentSpeed = EffectiveMovementSpeed * speedMultiplier;

                if (segmentSpeed <= 0.0f)
                    break;

                float timeToWaypoint = distance / segmentSpeed;

                if (timeToWaypoint <= remainingTime)
                {
                    current.X = waypoint.X;
                    current.Z = waypoint.Z;
                    remainingTime -= timeToWaypoint;
                    state.AdvanceWaypoint();
                    positionChanged = true;
                }
                else
                {
                    float moveDistance = segmentSpeed * remainingTime;
                    float inverseDistance = 1.0f / distance;
                    current.X += dx * inverseDistance * moveDistance;
                    current.Z += dz * inverseDistance * moveDistance;
                    remainingTime = 0.0f;
                    positionChanged = true;
                }
            }

            if (!IsFinite(current) || !Room.Navigation.TryWorldToCell(current, out NavGridCoordinate currentCell) ||
                !Room.Navigation.IsWalkable(currentCell.X, currentCell.Z))
            {
                return CancelInvalidMovement(state,
                                             "Movement produced a position outside the authoritative Navigation path.");
            }

            if (positionChanged)
            {
                SetPosition(current);
                SynchronizeTile(currentCell);
            }

            if (state.CurrentWaypointIndex >= path.Cells.Count)
                return CompleteMovement(state);

            return positionChanged ? PlayerMovementStep.Create(state, current, MOVEMENT_SNAPSHOT_STATE.Moving)
                                   : PlayerMovementStep.None;
        }

        private PlayerMovementStep CompleteMovement(PlayerMovementState state)
        {
            NumericsVector3 destination = state.Destination;

            if (!IsFinite(destination) ||
                !Room.Navigation.TryWorldToCell(destination, out NavGridCoordinate destinationCell) ||
                !destinationCell.Equals(state.DestinationCell))
            {
                return CancelInvalidMovement(state, "Final destination is inconsistent with the Path goal.");
            }

            SetPosition(destination);
            SynchronizeTile(destinationCell);
            state.Complete();

            if (Info.State == OBJECT_STATE_TYPE.Move)
                Info.State = OBJECT_STATE_TYPE.Idle;

            return PlayerMovementStep.Create(state, destination, MOVEMENT_SNAPSHOT_STATE.Arrived);
        }

        private PlayerMovementStep CancelInvalidMovement(PlayerMovementState state, string reason)
        {
            Console.WriteLine("[Movement] Cancelled invalid movement. Player=" + (Info?.ObjectId ?? 0) +
                              " Reason=" + reason);
            state.Deactivate();

            if (Info != null && Info.State == OBJECT_STATE_TYPE.Move)
                Info.State = OBJECT_STATE_TYPE.Idle;

            return PlayerMovementStep.Create(state, GetPosition(), MOVEMENT_SNAPSHOT_STATE.Cancelled);
        }

        private void SynchronizeTile(NavGridCoordinate cell)
        {
            if (TileX == cell.X && TileZ == cell.Z)
                return;
            int previousX = TileX;
            int previousZ = TileZ;
            TileX = cell.X;
            TileZ = cell.Z;
            Room.UpdatePlayerTilePosition(this, previousX, previousZ, TileX, TileZ);
        }

        private NumericsVector3 GetPosition()
        {
            return Info?.Position == null ? NumericsVector3.Zero
                                          : new NumericsVector3(Info.Position.X, Info.Position.Y, Info.Position.Z);
        }

        private void SetPosition(NumericsVector3 position)
        {
            Info.Position.X = position.X;
            Info.Position.Y = position.Y;
            Info.Position.Z = position.Z;
        }

        private static bool IsFinite(float value)
        {
            return !float.IsNaN(value) && !float.IsInfinity(value);
        }

        private static bool IsFinite(NumericsVector3 value)
        {
            return IsFinite(value.X) && IsFinite(value.Y) && IsFinite(value.Z);
        }
    }
}
