// 내비게이션 그리드 길찾기.
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using Vector3 = System.Numerics.Vector3;

namespace Server.Game.Navigation
{
    public enum NavGridPathStatus
    {
        Success = 0,
        InvalidStart = 1,
        InvalidGoal = 2,
        BlockedStart = 3,
        BlockedGoal = 4,
        NoPath = 5,
        SearchLimitExceeded = 6,
        PathTooLong = 7,
    }

    public sealed class NavGridPathOptions
    {
        public const int DefaultMaxExpandedNodes = 25000;
        public const int DefaultMaxPathCells = 4096;

        public int MaxExpandedNodes { get; set; } = DefaultMaxExpandedNodes;
        public int MaxPathCells { get; set; } = DefaultMaxPathCells;

        internal void Validate()
        {
            if (MaxExpandedNodes <= 0)
                throw new ArgumentOutOfRangeException(nameof(MaxExpandedNodes));

            if (MaxPathCells <= 0)
                throw new ArgumentOutOfRangeException(nameof(MaxPathCells));
        }
    }

    public sealed class NavGridPath
    {
        private readonly ReadOnlyCollection<NavGridCoordinate> _cells;

        internal NavGridPath(NavGridCoordinate startCell, NavGridCoordinate goalCell, IList<NavGridCoordinate> cells,
                             int totalCost, bool wasDestinationAdjusted)
        {
            if (cells == null || cells.Count == 0)
                throw new ArgumentException("A NavGrid path must contain at least one Cell.", nameof(cells));

            if (!cells[0].Equals(startCell) || !cells[cells.Count - 1].Equals(goalCell))
                throw new ArgumentException("Path endpoints do not match StartCell and GoalCell.", nameof(cells));

            if (totalCost < 0)
                throw new ArgumentOutOfRangeException(nameof(totalCost));

            var copy = new NavGridCoordinate[cells.Count];

            for (int i = 0; i < cells.Count; ++i)
                copy[i] = cells[i];

            StartCell = startCell;
            GoalCell = goalCell;
            TotalCost = totalCost;
            WasDestinationAdjusted = wasDestinationAdjusted;
            _cells = Array.AsReadOnly(copy);
        }

        public NavGridCoordinate StartCell { get; }
        public NavGridCoordinate GoalCell { get; }
        public IReadOnlyList<NavGridCoordinate> Cells => _cells;
        public int TotalCost { get; }
        public bool WasDestinationAdjusted { get; }

        internal NavGridPath WithDestinationAdjusted(bool adjusted)
        {
            if (WasDestinationAdjusted == adjusted)
                return this;

            return new NavGridPath(StartCell, GoalCell, _cells, TotalCost, adjusted);
        }
    }

    public sealed class NavGridPathResult
    {
        private NavGridPathResult(NavGridPathStatus status, NavGridPath path, int expandedNodeCount)
        {
            Status = status;
            Path = path;
            ExpandedNodeCount = expandedNodeCount;
        }

        public NavGridPathStatus Status { get; }
        public NavGridPath Path { get; }
        public int ExpandedNodeCount { get; }
        public bool Success => Status == NavGridPathStatus.Success;

        internal static NavGridPathResult Succeeded(NavGridPath path, int expandedNodeCount)
        {
            return new NavGridPathResult(NavGridPathStatus.Success, path, expandedNodeCount);
        }

        internal static NavGridPathResult Failed(NavGridPathStatus status, int expandedNodeCount = 0)
        {
            return new NavGridPathResult(status, null, expandedNodeCount);
        }
    }

    public sealed class NavGridPathfinder
    {
        public const int StraightCost = 10;
        public const int DiagonalCost = 14;
        public const int SlowCostMultiplier = 2;

        private static readonly NavGridCoordinate[] NeighborOrder = {
            new NavGridCoordinate(0, -1), new NavGridCoordinate(-1, 0),  new NavGridCoordinate(1, 0),
            new NavGridCoordinate(0, 1),  new NavGridCoordinate(-1, -1), new NavGridCoordinate(1, -1),
            new NavGridCoordinate(-1, 1), new NavGridCoordinate(1, 1),
        };

        public NavGridPathResult FindPath(NavGridAsset navigation, NavGridCoordinate start, NavGridCoordinate goal,
                                          NavGridPathOptions options = null)
        {
            if (navigation == null)
                throw new ArgumentNullException(nameof(navigation));

            options = options ?? new NavGridPathOptions();
            options.Validate();

            if (!navigation.IsValidCell(start.X, start.Z))
                return NavGridPathResult.Failed(NavGridPathStatus.InvalidStart);

            if (!navigation.IsValidCell(goal.X, goal.Z))
                return NavGridPathResult.Failed(NavGridPathStatus.InvalidGoal);

            if (!navigation.IsWalkable(start.X, start.Z))
                return NavGridPathResult.Failed(NavGridPathStatus.BlockedStart);

            if (!navigation.IsWalkable(goal.X, goal.Z))
                return NavGridPathResult.Failed(NavGridPathStatus.BlockedGoal);

            if (start.Equals(goal))
            {
                var singleCell = new[] { start };

                return NavGridPathResult.Succeeded(new NavGridPath(start, goal, singleCell, 0, false), 0);
            }

            ulong rawCellCount = (ulong)navigation.Width * navigation.Height;

            if (rawCellCount > int.MaxValue)
                throw new InvalidOperationException("NavGrid is too large for the server Pathfinder.");
            int cellCount = (int)rawCellCount;
            int width = checked((int)navigation.Width);

            var gScore = new int[cellCount];
            var cameFrom = new int[cellCount];
            var closed = new bool[cellCount];

            for (int i = 0; i < cellCount; ++i)
            {
                gScore[i] = int.MaxValue;
                cameFrom[i] = -1;
            }

            int startIndex = ToIndex(width, start.X, start.Z);
            int goalIndex = ToIndex(width, goal.X, goal.Z);
            int insertionOrder = 0;
            int startHeuristic = OctileDistance(start, goal);
            var open = new OpenSetHeap();
            gScore[startIndex] = 0;
            open.Push(new OpenNode(startIndex, 0, startHeuristic, startHeuristic, insertionOrder++));

            int expanded = 0;

            while (open.Count > 0)
            {
                OpenNode currentNode = open.Pop();

                if (closed[currentNode.Index] || currentNode.G != gScore[currentNode.Index])
                    continue;

                if (expanded >= options.MaxExpandedNodes)
                    return NavGridPathResult.Failed(NavGridPathStatus.SearchLimitExceeded, expanded);

                closed[currentNode.Index] = true;
                ++expanded;

                if (currentNode.Index == goalIndex)
                    return ReconstructPath(start, goal, width, cameFrom, gScore[goalIndex], goalIndex, expanded,
                                           options.MaxPathCells);

                int currentX = currentNode.Index % width;
                int currentZ = currentNode.Index / width;

                for (int i = 0; i < NeighborOrder.Length; ++i)
                {
                    int dx = NeighborOrder[i].X;
                    int dz = NeighborOrder[i].Z;
                    int nextX = currentX + dx;
                    int nextZ = currentZ + dz;

                    if (!navigation.IsWalkable(nextX, nextZ))
                        continue;

                    bool diagonal = dx != 0 && dz != 0;

                    if (diagonal && (!navigation.IsWalkable(currentX + dx, currentZ) ||
                                     !navigation.IsWalkable(currentX, currentZ + dz)))
                    {
                        continue;
                    }

                    int nextIndex = ToIndex(width, nextX, nextZ);

                    if (closed[nextIndex])
                        continue;

                    int baseCost = diagonal ? DiagonalCost : StraightCost;
                    int multiplier = navigation.GetCellType(nextX, nextZ) == NavCellType.Slow ? SlowCostMultiplier : 1;
                    int tentativeG = checked(gScore[currentNode.Index] + baseCost * multiplier);

                    if (tentativeG >= gScore[nextIndex])
                        continue;

                    var nextCell = new NavGridCoordinate(nextX, nextZ);
                    int heuristic = OctileDistance(nextCell, goal);
                    gScore[nextIndex] = tentativeG;
                    cameFrom[nextIndex] = currentNode.Index;
                    open.Push(new OpenNode(nextIndex, tentativeG, checked(tentativeG + heuristic), heuristic,
                                           insertionOrder++));
                }
            }

            return NavGridPathResult.Failed(NavGridPathStatus.NoPath, expanded);
        }

        public const float ClearanceCells = 0.35f;
        private const float LineStepCells = 0.1f;

        public static NavGridPath Straighten(NavGridAsset navigation, NavGridPath path)
        {
            if (navigation == null || path == null || path.Cells.Count <= 2)
                return path;
            IReadOnlyList<NavGridCoordinate> cells = path.Cells;
            var kept = new List<NavGridCoordinate> { cells[0] };
            int anchor = 0;

            while (anchor < cells.Count - 1)
            {
                int next = anchor + 1;

                while (next + 1 < cells.Count && IsClearLine(navigation, cells[anchor], cells[next + 1]))
                    ++next;
                kept.Add(cells[next]);
                anchor = next;
            }

            return kept.Count == cells.Count ? path
                                             : new NavGridPath(path.StartCell, path.GoalCell, kept, path.TotalCost,
                                                               path.WasDestinationAdjusted);
        }

        public static bool IsClearLine(NavGridAsset navigation, NavGridCoordinate from,
                                       NavGridCoordinate to) => IsClearLine(navigation, from.X + 0.5f, from.Z + 0.5f,
                                                                            to);

        public static bool IsClearFromPosition(NavGridAsset navigation, Vector3 position, NavGridCoordinate to)
        {
            return IsClearLine(navigation, (position.X - navigation.Origin.X) / navigation.CellSize,
                               (position.Z - navigation.Origin.Z) / navigation.CellSize, to);
        }

        private static bool IsClearLine(NavGridAsset navigation, float fromX, float fromZ, NavGridCoordinate to)
        {
            float dx = to.X + 0.5f - fromX, dz = to.Z + 0.5f - fromZ;
            float length = (float)Math.Sqrt(dx * dx + dz * dz);

            if (length <= 0f)
                return true;
            float sideX = -dz / length * ClearanceCells, sideZ = dx / length * ClearanceCells;
            int steps = (int)Math.Ceiling(length / LineStepCells);

            for (int i = 0; i <= steps; ++i)
            {
                float t = (float)i / steps;
                float x = fromX + dx * t, z = fromZ + dz * t;

                if (!WalkableAt(navigation, x, z) || !WalkableAt(navigation, x + sideX, z + sideZ) ||
                    !WalkableAt(navigation, x - sideX, z - sideZ))
                    return false;
            }

            return true;
        }

        private static bool WalkableAt(NavGridAsset navigation, float cellX, float cellZ)
        {
            int x = (int)Math.Floor(cellX), z = (int)Math.Floor(cellZ);

            return navigation.IsValidCell(x, z) && navigation.IsWalkable(x, z);
        }

        public static int OctileDistance(NavGridCoordinate from, NavGridCoordinate to)
        {
            int dx = Math.Abs(to.X - from.X);
            int dz = Math.Abs(to.Z - from.Z);
            int diagonal = Math.Min(dx, dz);
            int straight = Math.Max(dx, dz) - diagonal;

            return checked(DiagonalCost * diagonal + StraightCost * straight);
        }

        private static NavGridPathResult ReconstructPath(NavGridCoordinate start, NavGridCoordinate goal, int width,
                                                         int[] cameFrom, int totalCost, int goalIndex, int expanded,
                                                         int maxPathCells)
        {
            var reversed = new List<NavGridCoordinate>();
            int current = goalIndex;

            while (current >= 0)
            {
                if (reversed.Count >= maxPathCells)
                    return NavGridPathResult.Failed(NavGridPathStatus.PathTooLong, expanded);
                reversed.Add(new NavGridCoordinate(current % width, current / width));
                current = cameFrom[current];
            }

            reversed.Reverse();

            if (reversed.Count == 0 || !reversed[0].Equals(start) || !reversed[reversed.Count - 1].Equals(goal))
                throw new InvalidOperationException("Path reconstruction produced invalid endpoints.");

            return NavGridPathResult.Succeeded(new NavGridPath(start, goal, reversed, totalCost, false), expanded);
        }

        private static int ToIndex(int width, int x, int z)
        {
            return checked(z * width + x);
        }

        private readonly struct OpenNode
        {
            public OpenNode(int index, int g, int f, int h, int insertionOrder)
            {
                Index = index;
                G = g;
                F = f;
                H = h;
                InsertionOrder = insertionOrder;
            }

            public int Index { get; }
            public int G { get; }
            public int F { get; }
            public int H { get; }
            public int InsertionOrder { get; }
        }

        private sealed class OpenSetHeap
        {
            private readonly List<OpenNode> _items = new List<OpenNode>();
            public int Count => _items.Count;

            public void Push(OpenNode node)
            {
                _items.Add(node);
                int index = _items.Count - 1;

                while (index > 0)
                {
                    int parent = (index - 1) / 2;

                    if (!ComesBefore(_items[index], _items[parent]))
                        break;
                    Swap(index, parent);
                    index = parent;
                }
            }

            public OpenNode Pop()
            {
                if (_items.Count == 0)
                    throw new InvalidOperationException("The A* Open Set is empty.");

                OpenNode result = _items[0];
                int last = _items.Count - 1;
                _items[0] = _items[last];
                _items.RemoveAt(last);

                if (_items.Count == 0)
                    return result;

                int index = 0;

                while (true)
                {
                    int left = index * 2 + 1;

                    if (left >= _items.Count)
                        break;
                    int right = left + 1;
                    int best = right < _items.Count && ComesBefore(_items[right], _items[left]) ? right : left;

                    if (!ComesBefore(_items[best], _items[index]))
                        break;
                    Swap(index, best);
                    index = best;
                }

                return result;
            }

            private static bool ComesBefore(OpenNode left, OpenNode right)
            {
                if (left.F != right.F)
                    return left.F < right.F;

                if (left.H != right.H)
                    return left.H < right.H;

                if (left.InsertionOrder != right.InsertionOrder)
                    return left.InsertionOrder < right.InsertionOrder;

                return left.Index < right.Index;
            }

            private void Swap(int left, int right)
            {
                OpenNode temporary = _items[left];
                _items[left] = _items[right];
                _items[right] = temporary;
            }
        }
    }
}
