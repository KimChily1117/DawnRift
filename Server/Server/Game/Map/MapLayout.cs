using Google.Protobuf.Protocol;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json;
using Vec3 = System.Numerics.Vector3;

namespace Server.Game.Map
{
    public sealed class MapLayout
    {
        public const float GroundY = 2.0f;
        public const string DefaultFile = "Map/SummonersRift.json";

        public sealed class Structure
        {
            public string Name;
            public int Team;
            public string Lane;
            public int Tier;
            public Vec3 Position;
        }

        public sealed class Camp
        {
            public string Name;
            public int Side;
            public PLAYER_CHAMPION_TYPE Monster;
            public Vec3 Position;
        }

        public static readonly string[] LaneNames = { "top", "mid", "bot" };

        public List<Structure> Turrets { get; } = new List<Structure>();
        public List<Structure> Inhibitors { get; } = new List<Structure>();
        public List<Structure> Nexuses { get; } = new List<Structure>();
        public Dictionary<(int team, string lane), Vec3> MinionSpawns { get; } = new Dictionary<(int, string), Vec3>();

        public Dictionary<string, List<Vec3>> Lanes { get; } = new Dictionary<string, List<Vec3>>();
        public List<Camp> Camps { get; } = new List<Camp>();
        // Each team's fountain spawn point (__Spawn_T1 / __Spawn_T2).
        public Dictionary<int, Vec3> Spawns { get; } = new Dictionary<int, Vec3>();
        // The dragon and baron pits ("dragon", "baron").
        public Dictionary<string, Vec3> Epics { get; } = new Dictionary<string, Vec3>();

        public IReadOnlyList<Vec3> LanePath(int team, string lane)
        {
            List<Vec3> points = Lanes.TryGetValue(lane, out List<Vec3> path) ? path : new List<Vec3>();

            return team == 1 ? points : Enumerable.Reverse(points).ToList();
        }

        public static Vec3 Ground(float x, float z) => new Vec3(x, GroundY, z);

        private static readonly Dictionary<string, PLAYER_CHAMPION_TYPE> Monsters =
            new Dictionary<string, PLAYER_CHAMPION_TYPE> {
                ["SRU_Blue"] = PLAYER_CHAMPION_TYPE.PlayerTypeSruBlue,
                ["SRU_Red"] = PLAYER_CHAMPION_TYPE.PlayerTypeSruRed,
                ["SRU_Gromp"] = PLAYER_CHAMPION_TYPE.PlayerTypeSruGromp,
                ["SRU_Murkwolf"] = PLAYER_CHAMPION_TYPE.PlayerTypeSruMurkwolf,
                ["SRU_Razorbeak"] = PLAYER_CHAMPION_TYPE.PlayerTypeSruRazorbeak,
                ["SRU_Krug"] = PLAYER_CHAMPION_TYPE.PlayerTypeSruKrug,
            };

        public static MapLayout Load(string path)
        {
            using JsonDocument document = JsonDocument.Parse(File.ReadAllText(path));
            JsonElement root = document.RootElement;
            var layout = new MapLayout();

            foreach (JsonElement turret in root.GetProperty("turrets").EnumerateArray())
                layout.Turrets.Add(ReadStructure(turret));

            foreach (JsonElement inhibitor in root.GetProperty("inhibitors").EnumerateArray())
                layout.Inhibitors.Add(ReadStructure(inhibitor));

            foreach (JsonElement nexus in root.GetProperty("nexus").EnumerateArray())
                layout.Nexuses.Add(ReadStructure(nexus));

            foreach (JsonProperty team in root.GetProperty("minionSpawns").EnumerateObject())
                foreach (JsonProperty lane in team.Value.EnumerateObject())
                    layout.MinionSpawns[(int.Parse(team.Name), lane.Name)] = ReadPoint(lane.Value);

            if (root.TryGetProperty("spawns", out JsonElement spawns))
                foreach (JsonProperty team in spawns.EnumerateObject())
                    layout.Spawns[int.Parse(team.Name)] = ReadPoint(team.Value);

            if (root.TryGetProperty("epics", out JsonElement epics))
                foreach (JsonProperty pit in epics.EnumerateObject())
                    layout.Epics[pit.Name] = ReadPoint(pit.Value);

            foreach (JsonProperty lane in root.GetProperty("lanes").EnumerateObject())
                layout.Lanes[lane.Name] = lane.Value.EnumerateArray().Select(ReadPoint).ToList();

            foreach (JsonElement camp in root.GetProperty("camps").EnumerateArray())
            {
                string monster = camp.GetProperty("monsters") [0].GetString();

                if (!Monsters.TryGetValue(monster, out PLAYER_CHAMPION_TYPE type))
                    continue;
                layout.Camps.Add(new Camp {
                    Name = camp.GetProperty("name").GetString(),
                    Side = camp.GetProperty("side").GetInt32(),
                    Monster = type,
                    Position = ReadPoint(camp.GetProperty("position")),
                });
            }

            return layout;
        }

        private static Structure ReadStructure(JsonElement element) => new Structure {
            Name = element.TryGetProperty("name", out JsonElement name) ? name.GetString() : null,
            Team = element.GetProperty("team").GetInt32(),
            Lane = element.TryGetProperty("lane", out JsonElement lane) ? lane.GetString() : "mid",
            Tier = element.TryGetProperty("tier", out JsonElement tier) ? tier.GetInt32() : 0,
            Position = ReadPoint(element.GetProperty("position")),
        };

        private static Vec3 ReadPoint(JsonElement point) => Ground(point[0].GetSingle(), point[1].GetSingle());

        private static readonly Lazy<MapLayout> SummonersRift = new Lazy<MapLayout>(
            () =>
            {
                string path = Path.Combine(Navigation.NavigationContentPath.GetDefaultContentRoot(), DefaultFile);

                if (!File.Exists(path))
                {
                    Console.WriteLine("[Map] " + path + " not found; no turrets, minions or monsters.");

                    return null;
                }

                MapLayout layout = Load(path);
                Console.WriteLine(
                    $"[Map] Loaded {layout.Turrets.Count} turrets, {layout.Inhibitors.Count} inhibitors, " +
                    $"{layout.Lanes.Count} lanes, {layout.Camps.Count} camps, {layout.Epics.Count} epic pits.");

                return layout;
            });
        public static MapLayout LoadSummonersRift() => SummonersRift.Value;
    }
}
