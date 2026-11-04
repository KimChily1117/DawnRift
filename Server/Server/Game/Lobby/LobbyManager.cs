// 로비 목록과 입장 예약 관리.
using Google.Protobuf.Protocol;
using Server.Game.Room;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;

namespace Server.Game.Lobbies
{
    public sealed class LobbyManager
    {
        public const int MaxNameLength = 16;
        public const int MaxRoomNameLength = 24;

        public static float NoShowSeconds = 60f, AfterGameSeconds = 180f, ClosedLingerSeconds = 2f;

        private sealed class Account
        {
            public ulong Id;
            public string Name = "";
            public ILobbyClient Client;
            public Lobby Room;
        }

        private sealed class GameTrack
        {
            public GameRoom Room;
            public float Age;
            public float OverFor;
            public float ClosedFor;
            public HashSet<string> Waiting = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        }

        private static readonly Lazy<LobbyManager> DefaultManager =
            new Lazy<LobbyManager>(() => new LobbyManager(Roster.Default));
        public static LobbyManager Instance => DefaultManager.Value;

        private readonly object _lock = new object();
        private readonly Roster _roster;
        private readonly Dictionary<ulong, Account> _accounts = new Dictionary<ulong, Account>();
        private readonly Dictionary<int, Lobby> _lobbies = new Dictionary<int, Lobby>();
        private readonly Dictionary<int, GameTrack> _games = new Dictionary<int, GameTrack>();
        private int _nextRoomId = 1;
        private string _lastListSignature = "";

        // The game rooms (tests give their own).
        public Func<GameRoom> CreateRoom { get; set; } = () => RoomManager.Instance.AddGameRoom();
        public Action<GameRoom> RemoveRoom { get; set; } = room => RoomManager.Instance.Remove(room.RoomId);
        public Func<int> RunningGames { get; set; } = () => RoomManager.Instance.RunningGames;
        public Func<int> MaxGames { get; set; } = () => RoomManager.MaxGames;

        public LobbyManager(Roster roster)
        {
            _roster = roster ?? throw new ArgumentNullException(nameof(roster));
        }

        // ---- Accounts ----

        public bool Login(ulong accountId, string name, ILobbyClient client)
        {
            name = (name ?? "").Trim();
            lock (_lock)
            {
                string reason = name.Length == 0 || name.Length > MaxNameLength ? "이름은 1~16자로 입력하세요."
                                : _accounts.ContainsKey(accountId) ? "이미 로그인했습니다."
                                : _accounts.Values.Any(a => a.Name.Equals(name, StringComparison.OrdinalIgnoreCase))
                                    ? "이미 사용 중인 이름입니다."
                                    : null;

                if (reason != null)
                {
                    client.Send(
                        new S_Login { Success = false, Reason = reason, ProtocolVersion = ProtocolInfo.Version });

                    return false;
                }

                var account = new Account { Id = accountId, Name = name, Client = client };
                _accounts[accountId] = account;
                client.Send(new S_Login { Success = true, AccountId = accountId, Name = name,
                                          ProtocolVersion = ProtocolInfo.Version });
                Console.WriteLine($"[Lobby] {name} ({accountId}) logged in.");

                Lobby home = _lobbies.Values.FirstOrDefault(l => l.PlaysInGame(name));

                if (home != null && home.Join(accountId, name, client) == null)
                    account.Room = home;
                else
                    client.Send(List(""));
                BroadcastList(force: true);

                return true;
            }
        }

        // The launcher's connection closed.
        public void Leave(ulong accountId)
        {
            lock (_lock)
            {
                if (!_accounts.TryGetValue(accountId, out Account account))
                    return;
                _accounts.Remove(accountId);

                if (account.Room != null)
                    LeaveRoom(account);
                Console.WriteLine($"[Lobby] {account.Name} logged out.");
                BroadcastList(force: true);
            }
        }

        public bool IsLoggedIn(ulong accountId)
        {
            lock (_lock) return _accounts.ContainsKey(accountId);
        }

        // ---- Room actions ----

        public void Act(ulong accountId, LOBBY_ACTION action, int value, string text = "")
        {
            lock (_lock)
            {
                if (!_accounts.TryGetValue(accountId, out Account account))
                    return;

                switch (action)
                {
                case LOBBY_ACTION.CreateRoom: {
                    if (account.Room != null)
                    {
                        account.Client.Send(List("이미 방에 있습니다."));

                        return;
                    }

                    string roomName = (text ?? "").Trim();

                    if (roomName.Length == 0)
                        roomName = account.Name + "의 방";

                    if (roomName.Length > MaxRoomNameLength)
                        roomName = roomName.Substring(0, MaxRoomNameLength);
                    Lobby lobby = NewLobby(roomName);
                    lobby.Join(account.Id, account.Name, account.Client);
                    account.Room = lobby;
                    Console.WriteLine($"[Lobby] {account.Name} created room {lobby.RoomId} '{roomName}'.");
                    BroadcastList(force: true);

                    return;
                }
                case LOBBY_ACTION.JoinRoom: {
                    if (account.Room != null)
                    {
                        account.Client.Send(List("이미 방에 있습니다."));

                        return;
                    }

                    if (!_lobbies.TryGetValue(value, out Lobby lobby))
                    {
                        account.Client.Send(List("없는 방입니다."));

                        return;
                    }

                    string refused = lobby.Join(account.Id, account.Name, account.Client);

                    if (refused != null)
                    {
                        account.Client.Send(List(refused));

                        return;
                    }

                    account.Room = lobby;
                    BroadcastList(force: true);

                    return;
                }
                case LOBBY_ACTION.LeaveRoom:
                    if (account.Room == null)
                        return;
                    LeaveRoom(account);
                    account.Client.Send(List(""));
                    BroadcastList(force: true);

                    return;
                case LOBBY_ACTION.DestroyRoom: {
                    Lobby lobby = account.Room;

                    if (lobby == null)
                        return;

                    if (!lobby.HostName.Equals(account.Name, StringComparison.OrdinalIgnoreCase))
                    {
                        account.Client.Send(List("방장만 방을 닫을 수 있습니다."));

                        return;
                    }

                    DestroyLobby(lobby, account.Name + " 님이 방을 닫았습니다.");

                    return;
                }
                default:
                    account.Room?.Act(accountId, action, value);
                    BroadcastList(force: false);

                    return;
                }
            }
        }

        public bool Chat(ulong accountId, string text)
        {
            Lobby lobby;
            lock (_lock) lobby = _accounts.TryGetValue(accountId, out Account account) ? account.Room : null;

            return lobby != null && lobby.Chat(accountId, text);
        }

        public GameReservation Claim(string token)
        {
            lock (_lock)
            {
                foreach (Lobby lobby in _lobbies.Values)
                    if (lobby.Claim(token) is GameReservation reservation)
                        return reservation;

                return null;
            }
        }

        public void OnGameClientLeft(string token) {}

        private Lobby NewLobby(string name)
        {
            var lobby = new Lobby(_roster, _nextRoomId++, name);
            lobby.CanStartGame = () => RunningGames() < MaxGames();
            lobby.CreateGameRoom = () =>
            {
                GameRoom room = CreateRoom();

                if (room == null)
                    return -1;
                room.RoomName = lobby.Name;
                room.HostName = lobby.HostName;
                room.WaitExpired += names =>
                {
                    foreach (string gone in names)
                        lobby.PassHost(gone);
                };
                _games[lobby.RoomId] = new GameTrack { Room = room };

                return room.RoomId;
            };
            lobby.IsWaitingInGame = name =>
                _games.TryGetValue(lobby.RoomId, out GameTrack track) && track.Room.IsWaitingFor(name);
            _lobbies[lobby.RoomId] = lobby;

            return lobby;
        }

        private void LeaveRoom(Account account)
        {
            Lobby lobby = account.Room;
            account.Room = null;
            lobby.Leave(account.Id);

            if (lobby.MemberCount == 0 && lobby.Phase != LOBBY_PHASE.InGame)
                _lobbies.Remove(lobby.RoomId);
        }

        private void DestroyLobby(Lobby lobby, string reason)
        {
            if (_games.TryGetValue(lobby.RoomId, out GameTrack track))
            {
                track.Room.ForceEnd(reason);
                _games.Remove(lobby.RoomId);
                GameTrack closing = track;
                _closing.Add(closing);
            }

            var members = _accounts.Values.Where(a => a.Room == lobby).ToList();

            foreach (Account member in members)
            {
                lobby.Leave(member.Id);
                member.Room = null;
            }

            _lobbies.Remove(lobby.RoomId);
            BroadcastList(force: true, except: members);

            foreach (Account member in members)
                member.Client.Send(List(reason));
            Console.WriteLine($"[Lobby] Room {lobby.RoomId} '{lobby.Name}' closed: {reason}");
        }

        private readonly List<GameTrack> _closing = new List<GameTrack>();

        public void Update(float deltaTime)
        {
            lock (_lock)
            {
                foreach (Lobby lobby in _lobbies.Values.ToList())
                {
                    lobby.Update(deltaTime);

                    if (lobby.Phase != LOBBY_PHASE.InGame || !_games.TryGetValue(lobby.RoomId, out GameTrack track))
                        continue;
                    GameRoom room = track.Room;
                    room.HostName = lobby.HostName;
                    track.Age += deltaTime;
                    bool over = room.Map?.IsOver ?? false;
                    track.OverFor = over ? track.OverFor + deltaTime : 0f;
                    // The rejoin buttons follow who the game waits for.
                    var waiting =
                        new HashSet<string>(room.Humans.Where(h => room.IsWaitingFor(h.Name)).Select(h => h.Name),
                                            StringComparer.OrdinalIgnoreCase);

                    if (!waiting.SetEquals(track.Waiting))
                    {
                        track.Waiting = waiting;
                        lobby.Refresh();
                    }

                    string end = null;

                    if (room.Closed)
                    {
                        track.ClosedFor += deltaTime;

                        if (track.ClosedFor >= ClosedLingerSeconds)
                            end =
                                string.IsNullOrEmpty(room.ClosedReason) ? "게임이 종료되었습니다." : room.ClosedReason;
                    }
                    else if (over && (room.ConnectedHumans == 0 || track.OverFor >= AfterGameSeconds))
                        end = "게임이 끝났습니다.";
                    else if (!room.IsLobbyGame && track.Age >= NoShowSeconds)
                        end = "아무도 게임에 들어오지 않아 취소되었습니다.";
                    else if (room.IsLobbyGame && room.ConnectedHumans == 0 && !room.Paused && !room.WaitingForReconnect)
                        end = "모두 나가서 게임이 끝났습니다.";

                    if (end == null)
                        continue;
                    _games.Remove(lobby.RoomId);
                    RemoveRoom(room);
                    Console.WriteLine($"[Lobby] Room {lobby.RoomId}: game room {room.RoomId} ended ({end}).");
                    lobby.EndGame(end);

                    if (lobby.MemberCount == 0)
                        _lobbies.Remove(lobby.RoomId);
                }

                foreach (GameTrack track in _closing.ToList())
                {
                    track.ClosedFor += deltaTime;

                    if (track.ClosedFor < ClosedLingerSeconds)
                        continue;
                    _closing.Remove(track);
                    RemoveRoom(track.Room);
                }

                BroadcastList(force: false);
            }
        }

        // ---- The room list ----

        private S_RoomList List(string message)
        {
            var list = new S_RoomList { Message = message ?? "", RunningGames = RunningGames(), MaxGames = MaxGames() };

            foreach (Lobby lobby in _lobbies.Values.OrderBy(l => l.RoomId))
                list.Rooms.Add(new RoomSummary {
                    RoomId = lobby.RoomId,
                    Name = lobby.Name,
                    Host = lobby.HostName,
                    Players = lobby.MemberCount,
                    Bots = lobby.BotsBlue + lobby.BotsRed,
                    Phase = lobby.Phase,
                    MaxPlayers = Lobby.MaxTeamSize * 2,
                });

            return list;
        }

        private void BroadcastList(bool force, ICollection<Account> except = null)
        {
            S_RoomList list = List("");
            string signature = list.ToString();

            if (!force && signature == _lastListSignature)
                return;
            _lastListSignature = signature;

            foreach (Account account in _accounts.Values)
                if (account.Room == null && (except == null || !except.Contains(account)))
                    account.Client.Send(list);
        }

        // Tests.
        public int LobbyCount
        {
            get {
                lock (_lock) return _lobbies.Count;
            }
        }

        public Lobby FindLobby(int roomId)
        {
            lock (_lock) return _lobbies.TryGetValue(roomId, out Lobby lobby) ? lobby : null;
        }

        public Lobby RoomOf(ulong accountId)
        {
            lock (_lock) return _accounts.TryGetValue(accountId, out Account account) ? account.Room : null;
        }
    }
}
