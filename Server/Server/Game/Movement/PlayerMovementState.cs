using Server.Game.Navigation;
using System;
using System.Numerics;

namespace Server.Game.Movement
{
    public sealed class PlayerMovementState
    {
        public PlayerMovementState(ulong serverMoveId, uint clientMoveSequence, Vector3 startPosition,
                                   Vector3 destination, NavGridCoordinate destinationCell, NavGridPath path,
                                   long acceptedServerTicks)
        {
            if (serverMoveId == 0)
                throw new ArgumentOutOfRangeException(nameof(serverMoveId));

            if (path == null)
                throw new ArgumentNullException(nameof(path));

            if (!path.GoalCell.Equals(destinationCell))
                throw new ArgumentException("Movement destination Cell must match the Path goal.",
                                            nameof(destinationCell));

            ServerMoveId = serverMoveId;
            ClientMoveSequence = clientMoveSequence;
            StartPosition = startPosition;
            Destination = destination;
            DestinationCell = destinationCell;
            Path = path;
            CurrentWaypointIndex = path.Cells.Count > 1 ? 1 : 0;
            AcceptedServerTicks = acceptedServerTicks;
            IsActive = true;
        }

        public ulong ServerMoveId { get; }
        public uint ClientMoveSequence { get; }
        public Vector3 StartPosition { get; }
        public Vector3 Destination { get; }
        public NavGridCoordinate DestinationCell { get; }
        public NavGridPath Path { get; }
        public int CurrentWaypointIndex { get; private set; }
        public long AcceptedServerTicks { get; }
        public bool IsActive { get; private set; }

        internal void BeginAtStartCell() => CurrentWaypointIndex = 0;

        internal bool AdvanceWaypoint()
        {
            if (!IsActive || CurrentWaypointIndex >= Path.Cells.Count)
                return false;
            ++CurrentWaypointIndex;

            return true;
        }

        internal void Complete()
        {
            CurrentWaypointIndex = Path.Cells.Count;
            IsActive = false;
        }

        public void Deactivate()
        {
            IsActive = false;
        }
    }
}
