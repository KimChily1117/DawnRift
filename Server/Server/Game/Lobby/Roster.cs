using Google.Protobuf.Protocol;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json;

namespace Server.Game.Lobbies
{
    public sealed class Roster
    {
        public sealed class Champion
        {
            public PLAYER_CHAMPION_TYPE Type;
            public string Key = "";
            public bool Playable;
            public int[] Skins = { 0 };
        }

        public IReadOnlyList<Champion> Champions { get; }

        public Roster(IEnumerable<Champion> champions)
        {
            Champions = champions.ToList();
        }

        public Champion Find(PLAYER_CHAMPION_TYPE type) => Champions.FirstOrDefault(c => c.Type == type);
        public bool IsPlayable(PLAYER_CHAMPION_TYPE type) => Find(type)?.Playable == true;
        public bool HasSkin(PLAYER_CHAMPION_TYPE type, int skin) => Find(type)?.Skins.Contains(skin) == true;

        public static Roster Parse(string json)
        {
            using JsonDocument document = JsonDocument.Parse(json);
            var champions = new List<Champion>();

            foreach (JsonElement entry in document.RootElement.GetProperty("champions").EnumerateArray())
            {
                var champion = new Champion {
                    Type = (PLAYER_CHAMPION_TYPE)entry.GetProperty("type").GetInt32(),
                    Key = entry.GetProperty("key").GetString(),
                    Playable = entry.TryGetProperty("playable", out JsonElement playable) && playable.GetBoolean(),
                };

                if (entry.TryGetProperty("skins", out JsonElement skins))
                    // Only skins with an in-game model can be picked.
                    champion.Skins = skins.EnumerateArray()
                                         .Where(s => s.TryGetProperty("model", out JsonElement model)
                                                         ? model.GetBoolean()
                                                         : s.GetProperty("id").GetInt32() == 0)
                                         .Select(s => s.GetProperty("id").GetInt32())
                                         .ToArray();
                champions.Add(champion);
            }

            return new Roster(champions);
        }

        private static readonly Lazy<Roster> Content = new Lazy<Roster>(
            () =>
            {
                string path = Path.Combine(AppContext.BaseDirectory, "Content", "Champions", "Roster.json");

                if (File.Exists(path))
                    return Parse(File.ReadAllText(path));
                Console.WriteLine("[Lobby] " + path + " not found; the champions with kits (base skins) only.");

                return new Roster(
                    ChampionRegistry.All.Select(kit => new Champion { Type = kit.Type, Key = kit.Key, Playable = true })
                        .ToArray());
            });
        public static Roster Default => Content.Value;
    }
}
