using Server.Game.Objects;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Numerics;

using Vec3 = System.Numerics.Vector3;

namespace Server.Game
{
    public class Tile
    {
        public Vec3 Position { get; set; }
        public int Value { get; set; }
        public bool IsWalkable { get; set; }
        private HashSet<GameObject> players;

        public Tile(Vector3 position, int value, bool isWalkable)
        {
            Position = position;
            Value = value;
            IsWalkable = isWalkable;
            players = new HashSet<GameObject>();
        }

        // ✅ 플레이어 추가
        public void AddPlayer(GameObject player)
        {
            if (!players.Contains(player))
            {
                players.Add(player);
                Console.WriteLine($"[Tile] ✅ Player {player.Info.ObjectId} entered tile ({Position.X}, {Position.Z})");
            }
        }

        // ✅ 플레이어 제거
        public void RemovePlayer(GameObject player)
        {
            if (players.Contains(player))
            {
                players.Remove(player);
                Console.WriteLine($"[Tile] ❌ Player {player.Info.ObjectId} left tile ({Position.X}, {Position.Z})");
            }
        }

        // ✅ 타일 위에 플레이어가 있는지 확인
        public bool HasPlayer(GameObject player)
        {
            return players.Contains(player);
        }

        // ✅ 현재 타일에 있는 모든 플레이어 리스트 반환
        public List<GameObject> GetPlayers()
        {
            return players.ToList();
        }
    }

    public class Tilemap
    {
        private List<Tile> _tiles = new List<Tile>();
        public Vector2 _mapSize { private set; get; }
        private int _tileSize = 1;

        public bool LoadFile(string path)
        {
            try
            {
                if (!File.Exists(path))
                {
                    Console.WriteLine($"[Server] Tilemap file not found: {path}");

                    return false;
                }

                Console.WriteLine($"[Server] Loading Tilemap from {path}");
                string[] lines = File.ReadAllLines(path, System.Text.Encoding.UTF8);

                if (lines.Length == 0)
                {
                    Console.WriteLine("[Server] Tilemap file is empty.");

                    return false;
                }

                string[] sizeTokens = lines[0].Split(' ');

                if (sizeTokens.Length != 2 || !int.TryParse(sizeTokens[0], out int sizeX) ||
                    !int.TryParse(sizeTokens[1], out int sizeZ) || sizeX <= 0 || sizeZ <= 0)
                {
                    Console.WriteLine("[Server] Invalid Tilemap size.");

                    return false;
                }

                _mapSize = new Vector2(sizeX, sizeZ);
                _tiles.Clear();

                for (int i = 1; i < lines.Length; i++)
                {
                    string[] tokens = lines[i].Split('\t');

                    if (tokens.Length != 5 || !int.TryParse(tokens[0], out int x) ||
                        !int.TryParse(tokens[1], out int y) || !int.TryParse(tokens[2], out int z) ||
                        !int.TryParse(tokens[3], out int value) || !int.TryParse(tokens[4], out int isWalkable))
                    {
                        Console.WriteLine($"[Server] Invalid Tilemap row {i}.");

                        return false;
                    }

                    _tiles.Add(new Tile(new Vec3(x, y, z), value, isWalkable == 1));
                }

                int expectedCount = checked(sizeX * sizeZ);

                if (_tiles.Count != expectedCount)
                {
                    Console.WriteLine(
                        $"[Server] Tilemap cell count mismatch: expected {expectedCount}, actual {_tiles.Count}.");

                    return false;
                }

                Console.WriteLine($"[Server] Tilemap loaded with {_tiles.Count} cells.");

                return true;
            }
            catch (Exception ex)
            {
                Console.WriteLine($"[Server] Error loading Tilemap: {ex.Message}");

                return false;
            }
        }

        // Returns a tile at an exact XZ cell coordinate.
        public Tile GetTileAt(Vec3 position)
        {
            foreach (var tile in _tiles)
            {
                if (tile.Position.X == position.X && tile.Position.Z == position.Z)
                    return tile;
            }

            return null;
        }

        // 🔥 충돌 체크
        public bool CanGo(Vec3 position)
        {
            Tile tile = GetTileAt(position);

            return tile != null && tile.IsWalkable;
        }

        public List<Tile> GetAllTiles()
        {
            return _tiles;
        }

        public List<Tile> GetTilesInRange(Vec3 center, int range)
        {
            List<Tile> tilesInRange = new List<Tile>();

            foreach (var tile in _tiles)
            {
                Vec3 tilePos = tile.Position;
                int manhattanDistance =
                    Math.Abs((int)center.X - (int)tilePos.X) + Math.Abs((int)center.Z - (int)tilePos.Z);

                if (manhattanDistance <= range)
                    tilesInRange.Add(tile);
            }

            return tilesInRange;
        }
    }
}
