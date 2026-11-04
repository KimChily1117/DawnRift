// 방 단위 로비 상태.
using Google.Protobuf;
using Google.Protobuf.Protocol;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Security.Cryptography;

namespace Server.Game.Lobbies
{
    // A launcher connection in the lobby.
    public interface ILobbyClient
    {
        void Send(IMessage packet);
    }

    public sealed class GameReservation
    {
        public string Token = "";
        public ulong AccountId;
        public string Name = "";
        public int Team;
        public LOBBY_POSITION Position;
        public PLAYER_CHAMPION_TYPE Champion;
        public int SkinId;
        public SUMMONER_SPELL[] Spells = (SUMMONER_SPELL[])SummonerSpells.Defaults.Clone();
        public bool Practice;

        public Server.Game.Bots.BotSettings Bots;

        public int RoomId;

        public bool Rejoin;
    }

    public sealed class Lobby
    {
        public const int MaxTeamSize = 5;
        public const float ChampSelectSeconds = 60f;

        public static readonly string GameHost = EnvOr("DAWN_PUBLIC_HOST", "127.0.0.1");
        public static readonly int GamePort =
            int.TryParse(Environment.GetEnvironmentVariable("DAWN_PORT"), out int port) && port > 0 &&
            port < 65536? port : 8080;

        private static string EnvOr(string name, string fallback)
        {
            string value = Environment.GetEnvironmentVariable(name);

            return string.IsNullOrWhiteSpace(value) ? fallback : value.Trim();
        }

        private sealed class Member
        {
            public ulong AccountId;
            public string Name = "";
            public ILobbyClient Client;
            public int Team;
            public LOBBY_POSITION Position;
            public PLAYER_CHAMPION_TYPE Champion;
            public int SkinId;
            public SUMMONER_SPELL[] Spells = (SUMMONER_SPELL[])SummonerSpells.Defaults.Clone();
            public bool Locked;
            public long JoinOrder;
        }

        private readonly object _lock = new object();
        private readonly List<Member> _members = new List<Member>();
        private readonly Dictionary<string, GameReservation> _reservations = new Dictionary<string, GameReservation>();
        private readonly HashSet<string> _playersInGame = new HashSet<string>();
        private readonly Roster _roster;
        private long _joinCounter;
        private float _secondsLeft;

        public LOBBY_PHASE Phase { get; private set; } = LOBBY_PHASE.Lobby;

        public bool Practice { get; private set; }

        public int BotsBlue { get; private set; }
        public int BotsRed { get; private set; }
        public BOT_DIFFICULTY BotDifficulty { get; private set; } = BOT_DIFFICULTY.Normal;

        public int RoomId { get; }
        public string Name { get; set; } = "";
        public int GameRoomId { get; private set; } = -1;

        public Func<int> CreateGameRoom { get; set; }

        public Func<string, bool> IsWaitingInGame { get; set; }
        // The names of the people playing this lobby's game.
        private readonly HashSet<string> _gamePlayers = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        private bool Managed => CreateGameRoom != null;

        public Lobby(Roster roster, int roomId = 0, string name = "")
        {
            _roster = roster ?? throw new ArgumentNullException(nameof(roster));
            RoomId = roomId;
            Name = name ?? "";
        }

        public int MemberCount
        {
            get {
                lock (_lock) return _members.Count;
            }
        }

        public string HostName
        {
            get {
                lock (_lock) return Host()?.Name ?? "";
            }
        }

        public bool HasMember(ulong accountId)
        {
            lock (_lock) return Find(accountId) != null;
        }

        public bool PlaysInGame(string name)
        {
            lock (_lock) return Phase == LOBBY_PHASE.InGame && _gamePlayers.Contains(name ?? "");
        }

        public IReadOnlyList<ulong> MemberIds
        {
            get {
                lock (_lock) return _members.Select(m => m.AccountId).ToList();
            }
        }

        public string Join(ulong accountId, string name, ILobbyClient client)
        {
            lock (_lock)
            {
                string reason = _members.Any(m => m.AccountId == accountId) ? "이미 이 방에 있습니다."
                                : _members.Any(m => m.Name.Equals(name, StringComparison.OrdinalIgnoreCase))
                                    ? "이미 사용 중인 이름입니다."
                                : Phase == LOBBY_PHASE.ChampSelect ? "챔피언 선택 중인 방입니다."
                                : Phase == LOBBY_PHASE.InGame && !_gamePlayers.Contains(name)
                                    ? "게임이 진행 중인 방입니다."
                                : _members.Count >= MaxTeamSize * 2 ? "방이 가득 찼습니다."
                                                                    : null;

                if (reason != null)
                    return reason;
                int blue = _members.Count(m => m.Team == 1), red = _members.Count(m => m.Team == 2);
                _members.Add(new Member {
                    AccountId = accountId,
                    Name = name,
                    Client = client,
                    Team = blue <= red ? 1 : 2,
                    JoinOrder = ++_joinCounter,
                });
                Console.WriteLine($"[Lobby] {name} ({accountId}) joined room {RoomId} '{Name}'.");
                BroadcastState("");

                return null;
            }
        }

        public void PassHost(string name)
        {
            lock (_lock)
            {
                Member host = Host();

                if (host == null || !host.Name.Equals(name ?? "", StringComparison.OrdinalIgnoreCase) ||
                    _members.Count < 2)
                    return;
                host.JoinOrder = ++_joinCounter;
                Console.WriteLine($"[Lobby] Room {RoomId}: {name} did not come back, {Host()?.Name} hosts now.");
                BroadcastState(Host()?.Name + " 님이 방장이 되었습니다.");
            }
        }

        public void EndGame(string message)
        {
            lock (_lock)
            {
                if (Phase != LOBBY_PHASE.InGame)
                    return;
                Phase = LOBBY_PHASE.Lobby;
                GameRoomId = -1;
                _gamePlayers.Clear();
                _reservations.Clear();
                _playersInGame.Clear();
                ResetPicks();
                BroadcastState(message ?? "게임이 끝났습니다.");
            }
        }

        private string Rejoin(Member member)
        {
            if (Phase != LOBBY_PHASE.InGame || GameRoomId < 0)
                return "진행 중인 게임이 없습니다.";

            if (!_gamePlayers.Contains(member.Name) || !(IsWaitingInGame?.Invoke(member.Name) ?? false))
                return "다시 들어갈 게임이 없습니다.";
            var reservation = new GameReservation {
                Token = NewToken(), AccountId = member.AccountId, Name = member.Name,
                Team = member.Team, RoomId = GameRoomId,          Rejoin = true,
            };
            _reservations[reservation.Token] = reservation;
            member.Client.Send(new S_GameStart { Host = GameHost, Port = GamePort, Token = reservation.Token });
            Console.WriteLine($"[Lobby] Room {RoomId}: {member.Name} rejoins game room {GameRoomId}.");

            return null;
        }

        private static readonly Lazy<Lobby> DefaultLobby = new Lazy<Lobby>(() => new Lobby(Roster.Default));
        public static Lobby Instance => DefaultLobby.Value;

        public bool Login(ulong accountId, string name, ILobbyClient client)
        {
            name = (name ?? "").Trim();
            lock (_lock)
            {
                string reason = name.Length == 0 || name.Length > 16 ? "이름은 1~16자로 입력하세요."
                                : _members.Any(m => m.Name.Equals(name, StringComparison.OrdinalIgnoreCase))
                                    ? "이미 사용 중인 이름입니다."
                                : _members.Any(m => m.AccountId == accountId) ? "이미 로그인했습니다."
                                : Phase != LOBBY_PHASE.Lobby ? "게임이 진행 중입니다. 잠시 후 다시 시도하세요."
                                : _members.Count >= MaxTeamSize * 2 ? "로비가 가득 찼습니다."
                                                                    : null;

                if (reason != null)
                {
                    client.Send(
                        new S_Login { Success = false, Reason = reason, ProtocolVersion = ProtocolInfo.Version });

                    return false;
                }

                int blue = _members.Count(m => m.Team == 1), red = _members.Count(m => m.Team == 2);

                _members.Add(new Member {
                    AccountId = accountId,
                    Name = name,
                    Client = client,
                    Team = blue <= red ? 1 : 2,
                    JoinOrder = ++_joinCounter,
                });
                Console.WriteLine($"[Lobby] {name} ({accountId}) joined.");
                client.Send(new S_Login { Success = true, AccountId = accountId, Name = name,
                                          ProtocolVersion = ProtocolInfo.Version });
                BroadcastState("");

                return true;
            }
        }

        public void Leave(ulong accountId)
        {
            lock (_lock)
            {
                Member member = Find(accountId);

                if (member == null)
                    return;
                _members.Remove(member);
                Console.WriteLine($"[Lobby] {member.Name} left.");

                if (Phase == LOBBY_PHASE.ChampSelect)
                {
                    Phase = LOBBY_PHASE.Lobby;
                    ResetPicks();
                    BroadcastState(member.Name + " 님이 나가서 챔피언 선택이 취소되었습니다.");

                    return;
                }

                BroadcastState("");
            }
        }

        public void Act(ulong accountId, LOBBY_ACTION action, int value)
        {
            lock (_lock)
            {
                Member member = Find(accountId);

                if (member == null)
                    return;

                if (action == LOBBY_ACTION.RejoinGame)
                {
                    string rejoinRefused = Rejoin(member);

                    if (rejoinRefused != null)
                        member.Client.Send(State(rejoinRefused, member));

                    return;
                }

                string refused = Apply(member, action, value);

                if (refused != null)
                {
                    member.Client.Send(State(refused, member));

                    return;
                }

                if (Phase == LOBBY_PHASE.ChampSelect && _members.All(m => m.Locked))
                {
                    StartGame();

                    return;
                }

                BroadcastState("");
            }
        }

        private string Apply(Member member, LOBBY_ACTION action, int value)
        {
            switch (action)
            {
            case LOBBY_ACTION.JoinTeam:
                if (Phase != LOBBY_PHASE.Lobby)
                    return "지금은 팀을 바꿀 수 없습니다.";

                if (value != 1 && value != 2)
                    return "알 수 없는 팀입니다.";

                if (member.Team != value && _members.Count(m => m.Team == value) >= MaxTeamSize)
                    return "그 팀은 가득 찼습니다.";
                member.Team = value;

                return null;
            case LOBBY_ACTION.SetPosition:
                if (Phase == LOBBY_PHASE.InGame)
                    return "게임이 진행 중입니다.";

                if (!Enum.IsDefined(typeof(LOBBY_POSITION), value))
                    return "알 수 없는 포지션입니다.";
                member.Position = (LOBBY_POSITION)value;

                return null;
            case LOBBY_ACTION.StartChampSelect:
                if (Phase != LOBBY_PHASE.Lobby)
                    return "이미 챔피언 선택 중입니다.";

                if (Host() != member)
                    return "방장만 시작할 수 있습니다.";

                if (Managed && !(CanStartGame?.Invoke() ?? true))
                    return "서버가 동시에 진행할 수 있는 게임 수를 넘었습니다. 다른 게임이 끝나면 시작하세요.";
                Phase = LOBBY_PHASE.ChampSelect;
                _secondsLeft = ChampSelectSeconds;
                ResetPicks();
                Console.WriteLine("[Lobby] Champion select started.");

                return null;
            case LOBBY_ACTION.HoverChampion: {
                if (Phase != LOBBY_PHASE.ChampSelect)
                    return "챔피언 선택 중이 아닙니다.";

                if (member.Locked)
                    return "이미 확정했습니다.";
                var champion = (PLAYER_CHAMPION_TYPE)value;

                if (!_roster.IsPlayable(champion))
                    return "아직 플레이할 수 없는 챔피언입니다.";

                if (_members.Any(m => m != member && m.Team == member.Team && m.Champion == champion))
                    return "같은 팀이 이미 고른 챔피언입니다.";

                if (member.Champion != champion)
                    member.SkinId = 0;
                member.Champion = champion;

                return null;
            }
            case LOBBY_ACTION.SelectSkin:
                if (Phase != LOBBY_PHASE.ChampSelect)
                    return "챔피언 선택 중이 아닙니다.";

                if (member.Champion == PLAYER_CHAMPION_TYPE.PlayerTypeNone)
                    return "먼저 챔피언을 고르세요.";

                if (!_roster.HasSkin(member.Champion, value))
                    return "없는 스킨입니다.";
                member.SkinId = value;

                return null;
            case LOBBY_ACTION.SelectSpellD:
            case LOBBY_ACTION.SelectSpellF: {
                if (Phase != LOBBY_PHASE.ChampSelect)
                    return "챔피언 선택 중이 아닙니다.";

                if (member.Locked)
                    return "이미 확정했습니다.";
                var spell = (SUMMONER_SPELL)value;

                if (SummonerSpellData.Get(spell) == null)
                    return "없는 소환사 주문입니다.";
                int slot = action == LOBBY_ACTION.SelectSpellD ? 0 : 1;

                if (member.Spells[1 - slot] == spell)
                    member.Spells[1 - slot] = member.Spells[slot];
                member.Spells[slot] = spell;

                return null;
            }
            case LOBBY_ACTION.TogglePractice:
                if (member != Host())
                    return "방장만 연습 도구를 켤 수 있습니다.";
                Practice = value != 0;

                return null;
            case LOBBY_ACTION.SetBotsBlue:
            case LOBBY_ACTION.SetBotsRed: {
                if (member != Host())
                    return "방장만 봇을 설정할 수 있습니다.";

                if (Phase != LOBBY_PHASE.Lobby)
                    return "지금은 봇을 바꿀 수 없습니다.";
                int team = action == LOBBY_ACTION.SetBotsBlue ? 1 : 2;
                int free = MaxTeamSize - _members.Count(m => m.Team == team);

                if (value < 0 || value > free)
                    return $"그 팀에는 봇을 {free}명까지 넣을 수 있습니다.";

                if (team == 1)
                    BotsBlue = value;
                else
                    BotsRed = value;

                return null;
            }
            case LOBBY_ACTION.SetBotDifficulty:
                if (member != Host())
                    return "방장만 봇을 설정할 수 있습니다.";

                if (!Enum.IsDefined(typeof(BOT_DIFFICULTY), value))
                    return "알 수 없는 난이도입니다.";
                BotDifficulty = (BOT_DIFFICULTY)value;

                return null;
            case LOBBY_ACTION.LockIn:
                if (Phase != LOBBY_PHASE.ChampSelect)
                    return "챔피언 선택 중이 아닙니다.";

                if (member.Champion == PLAYER_CHAMPION_TYPE.PlayerTypeNone)
                    return "먼저 챔피언을 고르세요.";
                member.Locked = true;

                return null;
            default:
                return "알 수 없는 요청입니다.";
            }
        }

        // Champion select timer; the server's main loop calls this.
        public void Update(float deltaTime)
        {
            lock (_lock)
            {
                if (Phase != LOBBY_PHASE.ChampSelect)
                    return;
                int before = (int)Math.Ceiling(_secondsLeft);
                _secondsLeft -= deltaTime;

                if (_secondsLeft > 0f)
                {
                    if ((int)Math.Ceiling(_secondsLeft) != before)
                        BroadcastState("");

                    return;
                }

                foreach (Member member in _members.Where(m => !m.Locked))
                {
                    if (member.Champion == PLAYER_CHAMPION_TYPE.PlayerTypeNone)
                        member.Champion =
                            _roster.Champions.Where(c => c.Playable)
                                .Select(c => c.Type)
                                .FirstOrDefault(c => !_members.Any(m => m.Team == member.Team && m.Champion == c));
                    member.Locked = member.Champion != PLAYER_CHAMPION_TYPE.PlayerTypeNone;
                }

                if (_members.All(m => m.Locked))
                    StartGame();
                else
                {
                    Phase = LOBBY_PHASE.Lobby;
                    ResetPicks();
                    BroadcastState("남은 챔피언이 없어 챔피언 선택이 취소되었습니다.");
                }
            }
        }

        public Func<bool> CanStartGame { get; set; }

        private void StartGame()
        {
            int gameRoom = 0;

            if (Managed)
            {
                gameRoom = CreateGameRoom();

                if (gameRoom < 0)
                {
                    Phase = LOBBY_PHASE.Lobby;
                    ResetPicks();
                    BroadcastState("서버가 동시에 진행할 수 있는 게임 수를 넘어 시작하지 못했습니다.");

                    return;
                }
            }

            GameRoomId = gameRoom;
            _gamePlayers.Clear();

            foreach (Member member in _members)
                _gamePlayers.Add(member.Name);
            Phase = LOBBY_PHASE.InGame;
            _reservations.Clear();
            _playersInGame.Clear();
            ClampBots();
            Server.Game.Bots.BotSettings bots =
                BotsBlue + BotsRed == 0
                    ? null
                    : new Server.Game.Bots.BotSettings {
                          BotsBlue = BotsBlue,
                          BotsRed = BotsRed,
                          Difficulty = BotDifficulty,
                          Humans = _members.Select(m => (m.Team, m.Champion, m.Position)).ToList(),

                          DriveHumans = Environment.GetEnvironmentVariable("DAWN_BOT_AUTOPILOT") == "1",
                      };

            foreach (Member member in _members)
            {
                var reservation = new GameReservation {
                    Token = NewToken(),         AccountId = member.AccountId,
                    Name = member.Name,         Team = member.Team,
                    Position = member.Position, Champion = member.Champion,
                    SkinId = member.SkinId,     Spells = (SUMMONER_SPELL[])member.Spells.Clone(),
                    Practice = Practice,        Bots = bots,
                    RoomId = gameRoom,
                };
                _reservations[reservation.Token] = reservation;
                _playersInGame.Add(reservation.Token);
            }

            BroadcastState("");

            foreach (GameReservation reservation in _reservations.Values)
                Find(reservation.AccountId)
                    ?.Client.Send(new S_GameStart { Host = GameHost, Port = GamePort, Token = reservation.Token });
            Console.WriteLine(
                $"[Lobby] Room {RoomId}: game starting with {_reservations.Count} players in game room {gameRoom}.");
        }

        // A game client presents its token once.
        public GameReservation Claim(string token)
        {
            lock (_lock)
            {
                if (string.IsNullOrEmpty(token) || !_reservations.TryGetValue(token, out GameReservation reservation))
                    return null;
                _reservations.Remove(token);

                return reservation;
            }
        }

        public void OnGameClientLeft(string token)
        {
            lock (_lock)
            {
                if (Managed || !_playersInGame.Remove(token) || _playersInGame.Count > 0 || Phase != LOBBY_PHASE.InGame)
                    return;
                Phase = LOBBY_PHASE.Lobby;
                _reservations.Clear();
                ResetPicks();
                BroadcastState("게임이 끝났습니다.");
            }
        }

        private void ResetPicks()
        {
            foreach (Member member in _members)
            {
                member.Champion = PLAYER_CHAMPION_TYPE.PlayerTypeNone;
                member.SkinId = 0;
                member.Locked = false;
                member.Spells = DefaultSpells(member.Position);
            }
        }

        public static SUMMONER_SPELL[] DefaultSpells(LOBBY_POSITION position) => new[] {
            SUMMONER_SPELL.SummonerFlash,
            position == LOBBY_POSITION.PositionJungle? SUMMONER_SPELL.SummonerSmite : SUMMONER_SPELL.SummonerIgnite,
        };

        private Member Find(ulong accountId) => _members.FirstOrDefault(m => m.AccountId == accountId);

        private void ClampBots()
        {
            BotsBlue = Math.Clamp(BotsBlue, 0, Math.Max(0, MaxTeamSize - _members.Count(m => m.Team == 1)));
            BotsRed = Math.Clamp(BotsRed, 0, Math.Max(0, MaxTeamSize - _members.Count(m => m.Team == 2)));
        }

        private Member Host() => _members.OrderBy(m => m.JoinOrder).FirstOrDefault();

        private S_LobbyState State(string message, Member receiver = null)
        {
            Member host = Host();
            ClampBots();
            var state = new S_LobbyState {
                Phase = Phase,
                SecondsLeft = Math.Max(0f, _secondsLeft),
                Message = message ?? "",
                Practice = Practice,
                BotsBlue = BotsBlue,
                BotsRed = BotsRed,
                BotDifficulty = BotDifficulty,
                RoomId = RoomId,
                RoomName = Name,
                CanRejoin = receiver != null && Phase == LOBBY_PHASE.InGame && _gamePlayers.Contains(receiver.Name) &&
                            (IsWaitingInGame?.Invoke(receiver.Name) ?? false),
            };

            foreach (Member member in _members.OrderBy(m => m.Team).ThenBy(m => m.JoinOrder))
                state.Members.Add(new LobbyMember {
                    AccountId = member.AccountId,
                    Name = member.Name,
                    Team = member.Team,
                    Position = member.Position,
                    Champion = member.Champion,
                    SkinId = member.SkinId,
                    Spells = { member.Spells },
                    Locked = member.Locked,
                    Host = member == host,
                });

            return state;
        }

        public bool Chat(ulong accountId, string text)
        {
            lock (_lock)
            {
                Member member = Find(accountId);
                string line = global::Chat.Clean(text);

                if (member == null || line.Length == 0)
                    return false;
                var packet = new S_ChatMessage { SenderId = accountId, SenderName = member.Name, Message = line,
                                                 Channel = CHAT_CHANNEL.ChatLobby, Team = member.Team };

                foreach (Member other in _members)
                    other.Client.Send(packet);

                return true;
            }
        }

        private void BroadcastState(string message)
        {
            foreach (Member member in _members)
                member.Client.Send(State(message, member));
        }

        public void Refresh()
        {
            lock (_lock) BroadcastState("");
        }

        private static string NewToken()
        {
            var bytes = new byte[16];

            using (var random = RandomNumberGenerator.Create()) random.GetBytes(bytes);

            return BitConverter.ToString(bytes).Replace("-", "").ToLowerInvariant();
        }
    }
}
