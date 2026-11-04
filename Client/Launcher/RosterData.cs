// 챔피언과 스킨 목록 데이터 읽기.
using Google.Protobuf.Protocol;
using System.IO;
using System.Text.Json;
using System.Windows.Media.Imaging;

namespace Launcher
{
    public sealed class RosterData
    {
        public sealed class Skin
        {
            public int Id { get; init; }
            public string Name { get; init; }
            public bool HasModel { get; init; }
            public BitmapImage LoadScreen { get; init; }
            public BitmapImage Splash { get; init; }
        }

        public sealed class Champion
        {
            public PLAYER_CHAMPION_TYPE Type { get; init; }
            public string Key { get; init; }
            public string Name { get; init; }
            public string Title { get; init; }
            public string[] Positions { get; init; }
            public bool Playable { get; init; }
            public BitmapImage Icon { get; init; }
            public List<Skin> Skins { get; init; }
        }

        public List<Champion> Champions { get; } = new List<Champion>();

        public Champion Find(PLAYER_CHAMPION_TYPE type) => Champions.FirstOrDefault(c => c.Type == type);

        public static RosterData Load(string assetsFolder)
        {
            var roster = new RosterData();
            using JsonDocument document =
                JsonDocument.Parse(File.ReadAllText(Path.Combine(assetsFolder, "Roster.json")));

            foreach (JsonElement entry in document.RootElement.GetProperty("champions").EnumerateArray())
            {
                string key = entry.GetProperty("key").GetString();
                string folder = Path.Combine(assetsFolder, "Roster", key);
                roster.Champions.Add(new Champion {
                    Type = (PLAYER_CHAMPION_TYPE)entry.GetProperty("type").GetInt32(),
                    Key = key,
                    Name = entry.GetProperty("name").GetString(),
                    Title = entry.GetProperty("title").GetString(),
                    Positions = entry.GetProperty("positions").EnumerateArray().Select(p => p.GetString()).ToArray(),
                    Playable = entry.GetProperty("playable").GetBoolean(),
                    Icon = Image(Path.Combine(folder, "icon.png")),
                    Skins = entry.GetProperty("skins")
                                .EnumerateArray()
                                .Select(s =>
                                        {
                                            int id = s.GetProperty("id").GetInt32();

                                            return new Skin {
                                                Id = id,
                                                Name = s.GetProperty("name").GetString(),
                                                HasModel = s.TryGetProperty("model", out JsonElement model)
                                                               ? model.GetBoolean()
                                                               : id == 0,
                                                LoadScreen = Image(Path.Combine(folder, $"loadscreen_{id}.jpg")),
                                                Splash = Image(Path.Combine(folder, $"splash_{id}.jpg")),
                                            };
                                        })
                                .ToList(),
                });
            }

            return roster;
        }

        public static BitmapImage Image(string path)
        {
            if (!File.Exists(path))
                return null;
            var image = new BitmapImage();
            image.BeginInit();
            image.CacheOption = BitmapCacheOption.OnLoad;
            image.UriSource = new Uri(Path.GetFullPath(path));
            image.EndInit();
            image.Freeze();

            return image;
        }
    }
}
