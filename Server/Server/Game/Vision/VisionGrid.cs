using System;
using System.Collections.Generic;
using System.IO;
using Vec3 = System.Numerics.Vector3;

namespace Server.Game.Vision
{
    public sealed class VisionGrid
    {
        public const byte Brush = 1;
        public const byte BlocksSight = 2;
        public const string DefaultFile = "Navigation/sr-vision.grid";

        public int Width { get; }
        public int Height { get; }
        public float CellSize { get; }
        public float OriginX { get; }
        public float OriginZ { get; }
        private readonly byte[] _cells;
        private readonly int[] _bush;

        public VisionGrid(int width, int height, float cellSize, float originX, float originZ, byte[] cells)
        {
            if (width <= 0 || height <= 0 || cellSize <= 0f || cells == null || cells.Length != width * height)
                throw new ArgumentException("Invalid vision grid.");
            Width = width;
            Height = height;
            CellSize = cellSize;
            OriginX = originX;
            OriginZ = originZ;
            _cells = cells;
            _bush = LabelBushes();
        }

        public static VisionGrid Load(string path)
        {
            byte[] bytes = File.ReadAllBytes(path);

            if (bytes.Length < 24 || bytes[0] != 'V' || bytes[1] != 'I' || bytes[2] != 'S' || bytes[3] != '1')
                throw new InvalidDataException("Vision grid magic must be VIS1.");
            int width = BitConverter.ToInt32(bytes, 4);
            int height = BitConverter.ToInt32(bytes, 8);
            float cellSize = BitConverter.ToSingle(bytes, 12);
            float originX = BitConverter.ToSingle(bytes, 16);
            float originZ = BitConverter.ToSingle(bytes, 20);
            var cells = new byte[width * height];
            Buffer.BlockCopy(bytes, 24, cells, 0, cells.Length);

            return new VisionGrid(width, height, cellSize, originX, originZ, cells);
        }

        private static readonly Lazy<VisionGrid> SummonersRift = new Lazy<VisionGrid>(
            () =>
            {
                string path = Path.Combine(Navigation.NavigationContentPath.GetDefaultContentRoot(), DefaultFile);

                if (!File.Exists(path))
                {
                    Console.WriteLine("[Vision] " + path + " not found; fog of war disabled.");

                    return null;
                }

                VisionGrid grid = Load(path);
                Console.WriteLine($"[Vision] Loaded {grid.Width}x{grid.Height} vision grid.");

                return grid;
            });
        public static VisionGrid LoadSummonersRift() => SummonersRift.Value;

        public bool TryCell(float x, float z, out int cx, out int cz)
        {
            cx = (int)Math.Floor((x - OriginX) / CellSize);
            cz = (int)Math.Floor((z - OriginZ) / CellSize);

            return cx >= 0 && cz >= 0 && cx < Width && cz < Height;
        }

        public byte FlagsAt(float x, float z) => TryCell(x, z, out int cx, out int cz) ? _cells[cz * Width + cx]
                                                                                       : BlocksSight;

        // 0 outside brush.
        public int BushAt(float x, float z) => TryCell(x, z, out int cx, out int cz) ? _bush[cz * Width + cx] : 0;

        public bool HasLineOfSight(Vec3 from, Vec3 to)
        {
            if (!TryCell(from.X, from.Z, out int x, out int z) || !TryCell(to.X, to.Z, out int tx, out int tz))
                return false;
            var blocked = new List<bool> { Blocks(x, z) };
            float dx = to.X - from.X, dz = to.Z - from.Z;
            int stepX = Math.Sign(dx), stepZ = Math.Sign(dz);
            float tDeltaX = stepX != 0 ? CellSize / Math.Abs(dx) : float.PositiveInfinity;
            float tDeltaZ = stepZ != 0 ? CellSize / Math.Abs(dz) : float.PositiveInfinity;
            float cellX = OriginX + x * CellSize, cellZ = OriginZ + z * CellSize;
            float tMaxX = stepX > 0   ? (cellX + CellSize - from.X) / dx
                          : stepX < 0 ? (cellX - from.X) / dx
                                      : float.PositiveInfinity;
            float tMaxZ = stepZ > 0   ? (cellZ + CellSize - from.Z) / dz
                          : stepZ < 0 ? (cellZ - from.Z) / dz
                                      : float.PositiveInfinity;

            for (int guard = 0; guard < Width + Height && (x != tx || z != tz); ++guard)
            {
                if (tMaxX < tMaxZ)
                {
                    x += stepX;
                    tMaxX += tDeltaX;
                }
                else
                {
                    z += stepZ;
                    tMaxZ += tDeltaZ;
                }

                if (x < 0 || z < 0 || x >= Width || z >= Height)
                    return false;
                blocked.Add(Blocks(x, z));
            }

            return ClearBetweenEnds(blocked);
        }

        internal static bool ClearBetweenEnds(IReadOnlyList<bool> blocked)
        {
            int first = 0, last = blocked.Count - 1;

            while (first <= last && blocked[first])
                ++first;

            while (last >= first && blocked[last])
                --last;

            for (int i = first; i <= last; ++i)
                if (blocked[i])
                    return false;

            return true;
        }

        private bool Blocks(int x, int z) => (_cells[z * Width + x] & BlocksSight) != 0;

        private int[] LabelBushes()
        {
            var labels = new int[_cells.Length];
            int next = 0;
            var queue = new Queue<int>();

            for (int start = 0; start < _cells.Length; ++start)
            {
                if ((_cells[start] & Brush) == 0 || labels[start] != 0)
                    continue;
                labels[start] = ++next;
                queue.Enqueue(start);

                while (queue.Count > 0)
                {
                    int cell = queue.Dequeue();
                    int cx = cell % Width, cz = cell / Width;

                    foreach ((int nx, int nz) in new[] { (cx + 1, cz), (cx - 1, cz), (cx, cz + 1), (cx, cz - 1) })
                    {
                        if (nx < 0 || nz < 0 || nx >= Width || nz >= Height)
                            continue;
                        int n = nz * Width + nx;

                        if ((_cells[n] & Brush) != 0 && labels[n] == 0)
                        {
                            labels[n] = next;
                            queue.Enqueue(n);
                        }
                    }
                }
            }

            BushCount = next;

            return labels;
        }

        public int BushCount { get; private set; }
    }
}
