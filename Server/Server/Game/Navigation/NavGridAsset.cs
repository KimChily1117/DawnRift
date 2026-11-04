using System;
using System.Globalization;
using System.Numerics;

namespace Server.Game.Navigation
{
    public enum NavCellType : byte
    {
        Walkable = 0,
        Blocked = 1,
        Slow = 2,
    }

    public readonly struct NavGridCoordinate : IEquatable<NavGridCoordinate>
    {
        public NavGridCoordinate(int x, int z)
        {
            X = x;
            Z = z;
        }

        public int X { get; }
        public int Z { get; }

        public bool Equals(NavGridCoordinate other) => X == other.X && Z == other.Z;
        public override bool Equals(object obj) => obj is NavGridCoordinate other && Equals(other);
        public override int GetHashCode() => HashCode.Combine(X, Z);
        public override string ToString() => X.ToString(CultureInfo.InvariantCulture) +
                                             ":" + Z.ToString(CultureInfo.InvariantCulture);
    }

    public sealed class NavGridAsset
    {
        private readonly byte[] _cells;
        private readonly byte[] _contentHash;

        private int[] _blockers;

        internal NavGridAsset(uint formatVersion, string mapId, uint width, uint height, float cellSize, Vector3 origin,
                              float defaultAgentRadius, uint cellEncoding, byte[] contentHash, byte[] cells)
        {
            FormatVersion = formatVersion;
            MapId = mapId;
            Width = width;
            Height = height;
            CellSize = cellSize;
            Origin = origin;
            DefaultAgentRadius = defaultAgentRadius;
            CellEncoding = cellEncoding;
            _contentHash = (byte[])contentHash.Clone();
            _cells = (byte[])cells.Clone();
        }

        public uint FormatVersion { get; }
        public string MapId { get; }
        public uint Width { get; }
        public uint Height { get; }
        public float CellSize { get; }
        public Vector3 Origin { get; }
        public float DefaultAgentRadius { get; }
        public uint CellEncoding { get; }
        public string ContentHashHex => NavGridHash.ToHex(_contentHash);

        public byte[] CopyContentHash() => (byte[])_contentHash.Clone();
        public byte[] CopyCellData() => (byte[])_cells.Clone();

        public bool IsValidCell(int x, int z)
        {
            return x >= 0 && z >= 0 && (uint)x < Width && (uint)z < Height;
        }

        public NavCellType GetCell(int x, int z)
        {
            if (!IsValidCell(x, z))
                throw new ArgumentOutOfRangeException(nameof(x), "NavGrid cell coordinate is outside the grid.");

            int index = checked(z * (int)Width + x);

            return (NavCellType)_cells[index];
        }

        public bool IsValidWorldPosition(float worldX, float worldY, float worldZ)
        {
            return TryWorldToCell(new Vector3(worldX, worldY, worldZ), out _);
        }

        public bool TryWorldToCell(float worldX, float worldY, float worldZ, out NavGridCoordinate coordinate)
        {
            return TryWorldToCell(new Vector3(worldX, worldY, worldZ), out coordinate);
        }

        public bool IsWalkable(int cellX, int cellZ)
        {
            if (!IsValidCell(cellX, cellZ))
                return false;

            if (_blockers != null && _blockers[cellZ * (int)Width + cellX] > 0)
                return false;

            return GetCell(cellX, cellZ) != NavCellType.Blocked;
        }

        public int AddBlocker(Vector3 center, float radius, int amount = 1)
        {
            _blockers ??= new int[checked((int)(Width * Height))];
            int touched = 0;
            int x0 = Math.Max(0, (int)Math.Floor((center.X - radius - Origin.X) / CellSize));
            int x1 = Math.Min((int)Width - 1, (int)Math.Floor((center.X + radius - Origin.X) / CellSize));
            int z0 = Math.Max(0, (int)Math.Floor((center.Z - radius - Origin.Z) / CellSize));
            int z1 = Math.Min((int)Height - 1, (int)Math.Floor((center.Z + radius - Origin.Z) / CellSize));

            for (int z = z0; z <= z1; ++z)
                for (int x = x0; x <= x1; ++x)
                {
                    float dx = Origin.X + (x + 0.5f) * CellSize - center.X,
                          dz = Origin.Z + (z + 0.5f) * CellSize - center.Z;

                    if (dx * dx + dz * dz > radius * radius)
                        continue;
                    int index = z * (int)Width + x;
                    _blockers[index] = Math.Max(0, _blockers[index] + amount);
                    ++touched;
                }
            return touched;
        }

        public int RemoveBlocker(Vector3 center, float radius) => AddBlocker(center, radius, -1);

        public NavCellType GetCellType(int cellX, int cellZ)
        {
            return GetCell(cellX, cellZ);
        }

        public bool TryGetCellWorldCenter(int cellX, int cellZ, out Vector3 center)
        {
            return TryCellToWorldCenter(new NavGridCoordinate(cellX, cellZ), out center);
        }

        public bool TryWorldToCell(Vector3 worldPosition, out NavGridCoordinate coordinate)
        {
            coordinate = default;

            if (!IsFinite(worldPosition.X) || !IsFinite(worldPosition.Y) || !IsFinite(worldPosition.Z))
                return false;

            double cellX = Math.Floor(((double)worldPosition.X - Origin.X) / CellSize);

            double cellZ = Math.Floor(((double)worldPosition.Z - Origin.Z) / CellSize);

            if (cellX < 0.0 || cellZ < 0.0 || cellX >= Width || cellZ >= Height)
                return false;

            coordinate = new NavGridCoordinate((int)cellX, (int)cellZ);

            return true;
        }

        public bool TryCellToWorldCenter(NavGridCoordinate cell, out Vector3 center)
        {
            center = default;

            if (!IsValidCell(cell.X, cell.Z))
                return false;

            center =
                new Vector3(Origin.X + (cell.X + 0.5f) * CellSize, Origin.Y, Origin.Z + (cell.Z + 0.5f) * CellSize);

            return true;
        }

        private static bool IsFinite(float value) => !float.IsNaN(value) && !float.IsInfinity(value);
    }
}
