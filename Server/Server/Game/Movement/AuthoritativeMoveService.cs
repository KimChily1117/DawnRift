// 서버 권한 이동 처리.
using Google.Protobuf.Protocol;
using Server.Game.Navigation;
using Server.Game.Objects;
using Server.Game.Room;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using NumericsVector3 = System.Numerics.Vector3;

namespace Server.Game.Movement
{
    public sealed class MoveRequestDecision
    {
        private MoveRequestDecision() {}

        public bool Accepted { get; private set; }
        public MOVE_REJECT_REASON RejectReason { get; private set; }
        public uint ClientMoveSequence { get; private set; }
        public ulong ServerMoveId { get; private set; }
        public NumericsVector3 RequestedDestination { get; private set; }
        public NumericsVector3 AcceptedDestination { get; private set; }
        public NumericsVector3 ServerStartPosition { get; private set; }
        public bool WasDestinationAdjusted { get; private set; }
        public int PathExpandedNodeCount { get; private set; }
        public double PathfindingElapsedMilliseconds { get; private set; }

        public static MoveRequestDecision Reject(uint sequence, MOVE_REJECT_REASON reason,
                                                 NumericsVector3 serverPosition)
        {
            return new MoveRequestDecision {
                Accepted = false,
                RejectReason = reason,
                ClientMoveSequence = sequence,
                ServerStartPosition = serverPosition,
            };
        }

        public static MoveRequestDecision Accept(uint sequence, ulong serverMoveId, NumericsVector3 requested,
                                                 NumericsVector3 accepted, NumericsVector3 serverStart, bool adjusted,
                                                 int pathExpandedNodeCount, double pathfindingElapsedMilliseconds)
        {
            return new MoveRequestDecision {
                Accepted = true,
                RejectReason = MOVE_REJECT_REASON.None,
                ClientMoveSequence = sequence,
                ServerMoveId = serverMoveId,
                RequestedDestination = requested,
                AcceptedDestination = accepted,
                ServerStartPosition = serverStart,
                WasDestinationAdjusted = adjusted,
                PathExpandedNodeCount = pathExpandedNodeCount,
                PathfindingElapsedMilliseconds = pathfindingElapsedMilliseconds,
            };
        }
    }

    public static class AuthoritativeMoveService
    {
        public const int NearbyWalkableRadius = 12;
        public const int NearbyWalkableVisitLimit = 1024;
        // Base to base across the 295 x 296 grid.
        public const int MaxExpandedNodes = 150000;
        public const float MaximumAbsoluteCoordinate = 1000000.0f;

        private static readonly NavGridPathfinder Pathfinder = new NavGridPathfinder();
        private static readonly NavGridPathOptions PathOptions = new NavGridPathOptions {
            MaxExpandedNodes = MaxExpandedNodes,
            MaxPathCells = NavGridPathOptions.DefaultMaxPathCells,
        };

        private static readonly NavGridCoordinate[] NeighborOrder = {
            new NavGridCoordinate(0, -1), new NavGridCoordinate(-1, 0),  new NavGridCoordinate(1, 0),
            new NavGridCoordinate(0, 1),  new NavGridCoordinate(-1, -1), new NavGridCoordinate(1, -1),
            new NavGridCoordinate(-1, 1), new NavGridCoordinate(1, 1),
        };

        public static MoveRequestDecision Process(ClientSession session, GameRoom expectedRoom, Player expectedPlayer,
                                                  C_MoveRequest request, NavGridPathOptions pathOptions = null)
        {
            if (request == null)
                return MoveRequestDecision.Reject(0, MOVE_REJECT_REASON.ServerError, GetServerPosition(expectedPlayer));

            if (session == null || expectedPlayer == null || expectedPlayer.Info == null)
                return MoveRequestDecision.Reject(request.ClientMoveSequence, MOVE_REJECT_REASON.NoPlayer,
                                                  NumericsVector3.Zero);

            if (expectedRoom == null)
                return MoveRequestDecision.Reject(request.ClientMoveSequence, MOVE_REJECT_REASON.NoRoom,
                                                  GetServerPosition(expectedPlayer));

            NumericsVector3 serverStart = GetServerPosition(expectedPlayer);

            if (!IsFinite(serverStart.X) || !IsFinite(serverStart.Y) || !IsFinite(serverStart.Z))
                return MoveRequestDecision.Reject(request.ClientMoveSequence, MOVE_REJECT_REASON.InvalidPlayerState,
                                                  NumericsVector3.Zero);

            if (session.IsDisconnected)
                return MoveRequestDecision.Reject(request.ClientMoveSequence, MOVE_REJECT_REASON.MovementNotAllowed,
                                                  serverStart);

            if (!ReferenceEquals(expectedPlayer.Session, session) || !ReferenceEquals(session.Player, expectedPlayer) ||
                !ReferenceEquals(session.GameRoom, expectedRoom) ||
                !ReferenceEquals(expectedPlayer.Room, expectedRoom) || !expectedRoom.ContainsPlayer(expectedPlayer))
            {
                return MoveRequestDecision.Reject(request.ClientMoveSequence, MOVE_REJECT_REASON.RoomMismatch,
                                                  serverStart);
            }

            if (!session.TryConsumeMoveSequence(request.ClientMoveSequence))
                return MoveRequestDecision.Reject(request.ClientMoveSequence, MOVE_REJECT_REASON.InvalidSequence,
                                                  serverStart);

            if (!session.TryConsumeMoveRequestBudget(Stopwatch.GetTimestamp()))
                return MoveRequestDecision.Reject(request.ClientMoveSequence, MOVE_REJECT_REASON.RateLimited,
                                                  serverStart);

            expectedPlayer.CancelBasicAttackWindup();

            if (expectedPlayer.Info.Hp <= 0 || expectedPlayer.IsMovementLockedByCombat ||
                expectedPlayer.Status.IsDisabled || expectedPlayer.Status.IsDashing)
                return MoveRequestDecision.Reject(request.ClientMoveSequence, MOVE_REJECT_REASON.MovementNotAllowed,
                                                  serverStart);

            if (expectedPlayer.Info.State == OBJECT_STATE_TYPE.Skill)
                return MoveRequestDecision.Reject(request.ClientMoveSequence, MOVE_REJECT_REASON.InvalidPlayerState,
                                                  serverStart);

            if (!string.Equals(request.NavigationMapId, expectedRoom.NavigationMapId, StringComparison.Ordinal))
                return MoveRequestDecision.Reject(request.ClientMoveSequence, MOVE_REJECT_REASON.NavigationMapMismatch,
                                                  serverStart);

            if (!IsExpectedHash(request.NavigationContentHash, expectedRoom.NavigationContentHash))
                return MoveRequestDecision.Reject(request.ClientMoveSequence, MOVE_REJECT_REASON.NavigationHashMismatch,
                                                  serverStart);

            float requestedX = request.RequestedDestinationX;
            float requestedZ = request.RequestedDestinationZ;

            if (!IsFinite(requestedX) || !IsFinite(requestedZ) || Math.Abs(requestedX) > MaximumAbsoluteCoordinate ||
                Math.Abs(requestedZ) > MaximumAbsoluteCoordinate)
            {
                return MoveRequestDecision.Reject(request.ClientMoveSequence, MOVE_REJECT_REASON.InvalidCoordinate,
                                                  serverStart);
            }

            NavGridAsset navigation = expectedRoom.Navigation;

            if (!navigation.TryWorldToCell(requestedX, navigation.Origin.Y, requestedZ,
                                           out NavGridCoordinate requestedCell))
                return MoveRequestDecision.Reject(request.ClientMoveSequence,
                                                  MOVE_REJECT_REASON.OutsideNavigationBounds, serverStart);

            if (!TryResolveDestination(navigation, requestedCell, out NavGridCoordinate acceptedCell,
                                       out bool adjusted))
                return MoveRequestDecision.Reject(request.ClientMoveSequence, MOVE_REJECT_REASON.NoNearbyWalkableCell,
                                                  serverStart);

            if (!navigation.TryGetCellWorldCenter(acceptedCell.X, acceptedCell.Z,
                                                  out NumericsVector3 acceptedDestination))
                return MoveRequestDecision.Reject(request.ClientMoveSequence, MOVE_REJECT_REASON.ServerError,
                                                  serverStart);
            acceptedDestination.Y = serverStart.Y;

            if (!navigation.TryWorldToCell(serverStart, out NavGridCoordinate startCell) ||
                !navigation.IsWalkable(startCell.X, startCell.Z))
            {
                Console.WriteLine("[Movement] Invalid authoritative Navigation start. Room=" + expectedRoom.RoomId +
                                  " Player=" + expectedPlayer.Info.ObjectId + " Position=(" + serverStart.X + "," +
                                  serverStart.Y + "," + serverStart.Z + ")");

                return MoveRequestDecision.Reject(request.ClientMoveSequence, MOVE_REJECT_REASON.InvalidNavigationStart,
                                                  serverStart);
            }

            NavGridPathResult pathResult;
            var pathfindingStopwatch = Stopwatch.StartNew();
            try
            {
                pathResult = Pathfinder.FindPath(navigation, startCell, acceptedCell, pathOptions ?? PathOptions);
            }
            catch (Exception exception)
            {
                pathfindingStopwatch.Stop();
                expectedRoom.RecordPathfindingMetrics(0, pathfindingStopwatch.Elapsed.TotalMilliseconds);
                Console.WriteLine("[Movement] Pathfinding error. Room=" + expectedRoom.RoomId +
                                  " Player=" + expectedPlayer.Info.ObjectId + " Error=" + exception.Message);

                return MoveRequestDecision.Reject(request.ClientMoveSequence, MOVE_REJECT_REASON.ServerError,
                                                  serverStart);
            }

            pathfindingStopwatch.Stop();
            expectedRoom.RecordPathfindingMetrics(pathResult.ExpandedNodeCount,
                                                  pathfindingStopwatch.Elapsed.TotalMilliseconds);

            if (!pathResult.Success)
                return MoveRequestDecision.Reject(request.ClientMoveSequence, ToRejectReason(pathResult.Status),
                                                  serverStart);

            NavGridPath path =
                NavGridPathfinder.Straighten(navigation, pathResult.Path.WithDestinationAdjusted(adjusted));

            if (!path.GoalCell.Equals(acceptedCell))
                return MoveRequestDecision.Reject(request.ClientMoveSequence, MOVE_REJECT_REASON.ServerError,
                                                  serverStart);

            NumericsVector3 requestedDestination = new NumericsVector3(requestedX, navigation.Origin.Y, requestedZ);
            ulong serverMoveId = expectedRoom.GenerateMoveId();
            var state = new PlayerMovementState(serverMoveId, request.ClientMoveSequence, serverStart,
                                                acceptedDestination, acceptedCell, path, DateTime.UtcNow.Ticks);

            if (path.Cells.Count > 1 && !NavGridPathfinder.IsClearFromPosition(navigation, serverStart, path.Cells[1]))
                state.BeginAtStartCell();
            expectedPlayer.ReplaceMovementState(state);

            return MoveRequestDecision.Accept(request.ClientMoveSequence, serverMoveId, requestedDestination,
                                              acceptedDestination, serverStart, adjusted, pathResult.ExpandedNodeCount,
                                              pathfindingStopwatch.Elapsed.TotalMilliseconds);
        }

        public static bool TryStartServerMove(GameRoom room, Player unit, NumericsVector3 destination)
        {
            if (room == null || unit?.Info == null || unit.Info.Hp <= 0 || unit.Status.IsStunned)
                return false;
            NavGridAsset navigation = room.Navigation;
            NumericsVector3 start = GetServerPosition(unit);

            if (!navigation.TryWorldToCell(start, out NavGridCoordinate startCell) ||
                !navigation.IsWalkable(startCell.X, startCell.Z) ||
                !navigation.TryWorldToCell(destination.X, navigation.Origin.Y, destination.Z,
                                           out NavGridCoordinate requestedCell) ||
                !TryResolveDestination(navigation, requestedCell, out NavGridCoordinate goalCell, out bool adjusted) ||
                !navigation.TryGetCellWorldCenter(goalCell.X, goalCell.Z, out NumericsVector3 goal))
                return false;
            goal.Y = start.Y;

            NavGridPathResult result = Pathfinder.FindPath(navigation, startCell, goalCell, PathOptions);
            room.RecordPathfindingMetrics(result.ExpandedNodeCount, 0.0);

            if (!result.Success)
                return false;
            NavGridPath path = NavGridPathfinder.Straighten(navigation, result.Path.WithDestinationAdjusted(adjusted));
            var state =
                new PlayerMovementState(room.GenerateMoveId(), 0, start, goal, goalCell, path, DateTime.UtcNow.Ticks);

            if (path.Cells.Count > 1 && !NavGridPathfinder.IsClearFromPosition(navigation, start, path.Cells[1]))
                state.BeginAtStartCell();
            unit.ReplaceMovementState(state);

            return true;
        }

        public static bool TryResolveDestination(NavGridAsset navigation, NavGridCoordinate requestedCell,
                                                 out NavGridCoordinate acceptedCell, out bool adjusted)
        {
            acceptedCell = default;
            adjusted = false;

            if (navigation == null || !navigation.IsValidCell(requestedCell.X, requestedCell.Z))
                return false;

            if (navigation.IsWalkable(requestedCell.X, requestedCell.Z))
            {
                acceptedCell = requestedCell;

                return true;
            }

            var queue = new Queue<NavGridCoordinate>();
            var visited = new HashSet<int>();
            queue.Enqueue(requestedCell);
            visited.Add(ToIndex(navigation, requestedCell));

            while (queue.Count > 0 && visited.Count <= NearbyWalkableVisitLimit)
            {
                NavGridCoordinate current = queue.Dequeue();

                for (int i = 0; i < NeighborOrder.Length; ++i)
                {
                    int nextX = current.X + NeighborOrder[i].X;
                    int nextZ = current.Z + NeighborOrder[i].Z;

                    if (!navigation.IsValidCell(nextX, nextZ))
                        continue;

                    if (Math.Max(Math.Abs(nextX - requestedCell.X), Math.Abs(nextZ - requestedCell.Z)) >
                        NearbyWalkableRadius)
                        continue;

                    var next = new NavGridCoordinate(nextX, nextZ);
                    int index = ToIndex(navigation, next);

                    if (visited.Count >= NearbyWalkableVisitLimit)
                        return false;

                    if (!visited.Add(index))
                        continue;

                    if (navigation.IsWalkable(nextX, nextZ))
                    {
                        acceptedCell = next;
                        adjusted = true;

                        return true;
                    }

                    if (visited.Count < NearbyWalkableVisitLimit)
                        queue.Enqueue(next);
                }
            }

            return false;
        }

        private static MOVE_REJECT_REASON ToRejectReason(NavGridPathStatus status)
        {
            switch (status)
            {
            case NavGridPathStatus.NoPath:
                return MOVE_REJECT_REASON.NoPath;
            case NavGridPathStatus.SearchLimitExceeded:
                return MOVE_REJECT_REASON.PathSearchLimitExceeded;
            case NavGridPathStatus.PathTooLong:
                return MOVE_REJECT_REASON.PathTooLong;
            case NavGridPathStatus.InvalidStart:
            case NavGridPathStatus.BlockedStart:
                return MOVE_REJECT_REASON.InvalidNavigationStart;
            default:
                return MOVE_REJECT_REASON.ServerError;
            }
        }

        private static int ToIndex(NavGridAsset navigation, NavGridCoordinate cell)
        {
            return checked(cell.Z * (int)navigation.Width + cell.X);
        }

        private static bool IsExpectedHash(string requested, string expected)
        {
            if (string.IsNullOrEmpty(requested) || requested.Length != 64)
                return false;

            for (int i = 0; i < requested.Length; ++i)
            {
                char value = requested[i];

                if (!((value >= '0' && value <= '9') || (value >= 'a' && value <= 'f') ||
                      (value >= 'A' && value <= 'F')))
                    return false;
            }

            return string.Equals(requested, expected, StringComparison.OrdinalIgnoreCase);
        }

        private static NumericsVector3 GetServerPosition(Player player)
        {
            return player?.Info?.Position == null
                       ? NumericsVector3.Zero
                       : new NumericsVector3(player.Info.Position.X, player.Info.Position.Y, player.Info.Position.Z);
        }

        private static bool IsFinite(float value)
        {
            return !float.IsNaN(value) && !float.IsInfinity(value);
        }
    }
}
