// 게임 클라이언트를 찾아 실행하고 종료를 감지한다.
using System.Diagnostics;
using System.IO;
using System.Net;
using System.Net.Sockets;

namespace Launcher.Services
{
    public sealed class GameLauncher : IGameLauncher
    {
        private readonly Action<Action> _dispatch;
        private Process _game;

        public GameLauncher(Action<Action> dispatch)
        {
            _dispatch = dispatch;
        }

        public bool IsRunning => _game != null;

        public string MatchHistoryFolder
        {
            get
            {
                string client = FindClient();

                return client == null ? null : Path.Combine(Path.GetDirectoryName(client), "..", "Saved", "Matches");
            }
        }

        public bool TryStart(string address, int port, string token, string extraArguments, Action exited)
        {
            string client = FindClient();

            if (client == null)
                return false;
            string arguments = $"--server {address}:{port} --token {token}";

            if (!string.IsNullOrEmpty(extraArguments))
                arguments += " " + extraArguments;
            _game = Process.Start(new ProcessStartInfo(client, arguments) {
                WorkingDirectory = Path.GetDirectoryName(client),
                UseShellExecute = false,
            });
            _game.EnableRaisingEvents = true;
            _game.Exited += (_, _) => _dispatch(() =>
            {
                _game = null;
                exited();
            });

            return true;
        }

        // 서버가 루프백 주소를 알려주면 원격 접속자는 로비 서버 주소로 대신 접속한다.
        public static string ResolveAddress(string gameHost, string lobbyHost)
        {
            string target = gameHost;

            if (IPAddress.TryParse(gameHost, out IPAddress parsed) && IPAddress.IsLoopback(parsed) && !IsLoopback(lobbyHost))
                target = lobbyHost;

            if (IPAddress.TryParse(target, out IPAddress ip) && ip.AddressFamily == AddressFamily.InterNetwork)
                return ip.ToString();
            try
            {
                foreach (IPAddress address in Dns.GetHostAddresses(target))
                {
                    if (address.AddressFamily == AddressFamily.InterNetwork)
                        return address.ToString();
                }
            }
            catch (SocketException)
            {
            }

            return target;
        }

        private static bool IsLoopback(string host)
        {
            return host.Equals("localhost", StringComparison.OrdinalIgnoreCase) ||
                   (IPAddress.TryParse(host, out IPAddress ip) && IPAddress.IsLoopback(ip));
        }

        private static string FindClient()
        {
            for (var directory = new DirectoryInfo(AppContext.BaseDirectory); directory != null; directory = directory.Parent)
            {
                string path = Path.Combine(directory.FullName, "Binaries", "Client.exe");

                if (File.Exists(path))
                    return path;
            }

            return null;
        }
    }
}
