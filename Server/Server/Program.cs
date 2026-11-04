// 게임 서버 진입점.
using System;
using System.Collections.Generic;
using System.Net;
using System.Threading;
using Server.Game.Navigation;
using Server.Game.Room;
using ServerCore;

namespace Server
{
    class Program
    {
        private static readonly Listener _listener = new Listener();
        private static readonly List<System.Timers.Timer> _timers = new List<System.Timers.Timer>();

        static void TickRooms(int tick = 100)
        {
            var timer = new System.Timers.Timer();
            timer.Interval = tick;
            timer.Elapsed += ((s, e) =>
                              { RoomManager.Instance.UpdateRooms(); });
            timer.AutoReset = true;
            timer.Enabled = true;
            _timers.Add(timer);
        }

        static void Main(string[] args)
        {
            if (!TryGetContentRootOverride(args, out string contentRootOverride, out string argumentError))
            {
                Console.Error.WriteLine("[Startup] " + argumentError);
                Environment.ExitCode = 1;

                return;
            }

            if (!NavigationContentPath.TryResolveContentRoot(contentRootOverride, out string contentRoot,
                                                             out string rootError))
            {
                Console.Error.WriteLine("[Startup] " + rootError);
                Environment.ExitCode = 1;

                return;
            }

            Console.WriteLine("[Navigation] Content Root: " + contentRoot);
            NavigationRegistryLoadResult registryLoad = NavigationRegistry.LoadFromContent(
                contentRoot, NavigationContentPath.DefaultConfigurationPath, message => Console.WriteLine(message));

            if (!registryLoad.Success)
            {
                Console.Error.WriteLine("[Navigation] Startup failed: " + registryLoad.Error);
                Environment.ExitCode = 1;

                return;
            }

            NavigationRegistry registry = registryLoad.Registry;

            if (!registry.TryGetRegistrationByRoomId(0, out _))
            {
                Console.Error.WriteLine(
                    "[Navigation] Startup failed: current ClientSession flow requires a Room 0 mapping.");
                Environment.ExitCode = 1;

                return;
            }

            try
            {
                foreach (NavigationRegistration registration in registry.Registrations)
                    LogRegistration(registration);

                RoomManager.Instance.Initialize(registry);
                RoomManager.Instance.AddConfiguredRooms();
            }
            catch (Exception exception)
            {
                Console.Error.WriteLine("[Startup] Room initialization failed: " + exception.Message);
                Environment.ExitCode = 1;

                return;
            }

            IPAddress ipAddr = IPAddress.TryParse(Environment.GetEnvironmentVariable("DAWN_BIND"), out IPAddress bind)
                                   ? bind
                                   : IPAddress.Loopback;
            IPEndPoint endPoint = new IPEndPoint(ipAddr, Server.Game.Lobbies.Lobby.GamePort);

            Console.WriteLine("Server IpAddress : " + ipAddr);
            _listener.Init(endPoint, () => SessionManager.Instance.Generate());
            Console.WriteLine("Listening...");

            var clock = System.Diagnostics.Stopwatch.StartNew();

            while (true)
            {
                Thread.Sleep(100);
                RoomManager.Instance.UpdateRooms();
                float elapsed = (float)clock.Elapsed.TotalSeconds;
                clock.Restart();
                Server.Game.Lobbies.LobbyManager.Instance.Update(elapsed);
            }
        }

        private static void LogRegistration(NavigationRegistration registration)
        {
            NavGridAsset asset = registration.Asset;
            Console.WriteLine("[Navigation] Loaded" + " RoomId=" + registration.RoomId +
                              " SceneId=" + registration.SceneId + " MapId=" + registration.NavigationMapId +
                              " FormatVersion=" + asset.FormatVersion + " Size=" + asset.Width + "x" + asset.Height +
                              " CellSize=" + asset.CellSize + " Origin=(" + asset.Origin.X + "," + asset.Origin.Y +
                              "," + asset.Origin.Z + ")" + " Hash=" + asset.ContentHashHex +
                              " Asset=" + registration.AssetPath);
        }

        private static bool TryGetContentRootOverride(string[] args, out string contentRootOverride, out string error)
        {
            contentRootOverride = null;
            error = null;

            if (args == null || args.Length == 0)
                return true;

            const string prefix = "--content-root=";

            if (args.Length != 1 || !args[0].StartsWith(prefix, StringComparison.OrdinalIgnoreCase))
            {
                error = "Only the development option --content-root=<absolute-path> is supported.";

                return false;
            }

#if !DEBUG
            error = "--content-root is available only in Debug builds.";

            return false;
#else
            contentRootOverride = args[0].Substring(prefix.Length);

            if (string.IsNullOrWhiteSpace(contentRootOverride))
            {
                error = "--content-root requires an absolute path.";

                return false;
            }

            return true;
#endif
        }
    }
}
