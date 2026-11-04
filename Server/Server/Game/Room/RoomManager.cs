using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using Server.Game.Navigation;

namespace Server.Game.Room
{
    public class RoomManager
    {
        public static RoomManager Instance { get; } = new RoomManager();

        private readonly object _lock = new object();
        private readonly Dictionary<int, GameRoom> _rooms = new Dictionary<int, GameRoom>();
        private NavigationRegistry _navigationRegistry;
        private long _lastUpdateTimestamp;

        public void Initialize(NavigationRegistry navigationRegistry)
        {
            if (navigationRegistry == null)
                throw new ArgumentNullException(nameof(navigationRegistry));

            lock (_lock)
            {
                if (_rooms.Count != 0)
                    throw new InvalidOperationException("RoomManager cannot be initialized after Rooms are created.");

                if (_navigationRegistry != null)
                    throw new InvalidOperationException("RoomManager is already initialized.");
                _navigationRegistry = navigationRegistry;
                _lastUpdateTimestamp = Stopwatch.GetTimestamp();
            }
        }

        public void AddConfiguredRooms()
        {
            NavigationRegistration[] registrations;
            lock (_lock)
            {
                EnsureInitialized();
                registrations = _navigationRegistry.Registrations.OrderBy(item => item.RoomId).ToArray();
            }

            foreach (NavigationRegistration registration in registrations)
                Add(registration.RoomId);
        }

        public GameRoom Add(int roomId)
        {
            NavigationRegistration registration;
            string contentRoot;
            lock (_lock)
            {
                EnsureInitialized();

                if (_rooms.ContainsKey(roomId))
                    throw new InvalidOperationException("Room already exists: " + roomId);

                if (!_navigationRegistry.TryGetRegistrationByRoomId(roomId, out registration))
                    throw new InvalidOperationException("Room " + roomId + " has no required Navigation registration.");
                contentRoot = _navigationRegistry.ContentRoot;
            }

            var gameRoom = Build(registration, contentRoot, roomId);
            gameRoom.Demo = Server.Game.Demo.DemoDirector.ForRoom(gameRoom);

            if (Server.Game.Bots.BotSettings.FromEnvironment() is Server.Game.Bots.BotSettings botSettings)
                gameRoom.Bots = new Server.Game.Bots.BotDirector(gameRoom, botSettings);

            lock (_lock)
            {
                if (_rooms.ContainsKey(roomId))
                    throw new InvalidOperationException("Room already exists: " + roomId);
                _rooms.Add(roomId, gameRoom);
            }

            return gameRoom;
        }

        private static GameRoom Build(NavigationRegistration registration, string contentRoot, int roomId)
        {
            var gameRoom = new GameRoom(registration, roomId);
            gameRoom.Init(contentRoot);

            if (Server.Game.Vision.VisionSystem.FogEnabled)
                gameRoom.Vision.Grid = Server.Game.Vision.VisionGrid.LoadSummonersRift();
            gameRoom.Layout = Server.Game.Map.MapLayout.LoadSummonersRift();
            gameRoom.Map = Server.Game.Map.MapDirector.ForGame(gameRoom, gameRoom.Layout);

            return gameRoom;
        }

        public const int FirstGameRoomId = 1000;
        public static int MaxGames =
            int.TryParse(Environment.GetEnvironmentVariable("DAWN_MAX_GAMES"), out int max) && max > 0? max : 3;
        private int _nextGameRoomId = FirstGameRoomId;

        public int RunningGames
        {
            get {
                lock (_lock) return _rooms.Keys.Count(id => id >= FirstGameRoomId);
            }
        }

        // A new game room, or null when MaxGames already run.
        public GameRoom AddGameRoom()
        {
            NavigationRegistration registration;
            string contentRoot;
            int roomId;
            lock (_lock)
            {
                EnsureInitialized();

                if (_rooms.Keys.Count(id => id >= FirstGameRoomId) >= MaxGames)
                    return null;

                if (!_navigationRegistry.TryGetRegistrationByRoomId(0, out registration))
                    throw new InvalidOperationException("Game rooms need room 0's Navigation registration.");
                contentRoot = _navigationRegistry.ContentRoot;
                roomId = _nextGameRoomId++;
            }

            GameRoom gameRoom = Build(registration, contentRoot, roomId);
            lock (_lock) _rooms.Add(roomId, gameRoom);
            Console.WriteLine($"[Rooms] Game room {roomId} created ({RunningGames}/{MaxGames} running).");

            return gameRoom;
        }

        public GameRoom Find(int roomId)
        {
            lock (_lock)
            {
                _rooms.TryGetValue(roomId, out GameRoom room);

                return room;
            }
        }

        public bool Remove(int roomId)
        {
            lock (_lock) return _rooms.Remove(roomId);
        }

        public void UpdateRooms()
        {
            GameRoom[] rooms;
            float deltaTime;
            lock (_lock)
            {
                rooms = _rooms.Values.ToArray();
                long now = Stopwatch.GetTimestamp();
                deltaTime = _lastUpdateTimestamp == 0
                                ? 0.0f
                                : (float)((now - _lastUpdateTimestamp) / (double)Stopwatch.Frequency);
                _lastUpdateTimestamp = now;
            }

            foreach (GameRoom room in rooms)
                room.Update(deltaTime);
        }

        private void EnsureInitialized()
        {
            if (_navigationRegistry == null)
                throw new InvalidOperationException("RoomManager requires a NavigationRegistry before creating Rooms.");
        }
    }
}
