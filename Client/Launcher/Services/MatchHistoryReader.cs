// 저장된 경기 결과 파일을 읽어 표시할 문장으로 바꾼다.
using System.IO;
using System.Text.Json;

namespace Launcher.Services
{
    public static class MatchHistoryReader
    {
        private const string Empty = "저장된 경기가 없습니다.";

        public static List<string> Read(string folder)
        {
            var lines = new List<string>();

            if (folder != null && Directory.Exists(folder))
            {
                foreach (string file in Directory.GetFiles(folder, "*.json").OrderByDescending(f => f).Take(30))
                {
                    try
                    {
                        lines.Add(Describe(file));
                    }
                    catch (Exception)
                    {
                        lines.Add(Path.GetFileName(file));
                    }
                }
            }

            if (lines.Count == 0)
                lines.Add(Empty);

            return lines;
        }

        private static string Describe(string file)
        {
            using JsonDocument document = JsonDocument.Parse(File.ReadAllText(file));
            JsonElement root = document.RootElement;
            int myTeam = root.TryGetProperty("myTeam", out JsonElement team) ? team.GetInt32() : 0;
            int winner = root.GetProperty("winningTeam").GetInt32();
            float seconds = root.GetProperty("gameTime").GetSingle();
            string myName = root.TryGetProperty("myName", out JsonElement name) ? name.GetString() : "";
            string result = winner == myTeam ? "승리" : "패배";
            string line = $"{root.GetProperty("playedAt").GetString()}  {result}  {(int)seconds / 60}:{(int)seconds % 60:00}";

            foreach (JsonElement entry in root.GetProperty("entries").EnumerateArray())
            {
                if (entry.GetProperty("name").GetString() != myName)
                    continue;
                int kills = entry.GetProperty("kills").GetInt32();
                int deaths = entry.GetProperty("deaths").GetInt32();
                int assists = entry.GetProperty("assists").GetInt32();
                line += $"  {entry.GetProperty("champion").GetString()} {kills}/{deaths}/{assists}  CS {entry.GetProperty("cs").GetInt32()}";
            }

            return line;
        }
    }
}
