// 마지막으로 접속한 서버 주소를 저장하고 읽는다.
using System.IO;

namespace Launcher.Services
{
    public static class ServerAddressStore
    {
        private static string SavedPath => Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "DawnRift", "server.txt");

        public static string Load()
        {
            foreach (string path in new[] { SavedPath, Path.Combine(AppContext.BaseDirectory, "server.txt") })
            {
                try
                {
                    if (File.Exists(path) && File.ReadAllText(path).Trim() is string text && text.Contains(':'))
                        return text;
                }
                catch (IOException)
                {
                }
            }

            return null;
        }

        public static void Save(string address)
        {
            try
            {
                Directory.CreateDirectory(Path.GetDirectoryName(SavedPath));
                File.WriteAllText(SavedPath, address);
            }
            catch (IOException)
            {
            }
            catch (UnauthorizedAccessException)
            {
            }
        }
    }
}
