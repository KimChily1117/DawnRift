// 화면 전환과 서버 이벤트 처리를 담당하는 최상위 ViewModel.
using Google.Protobuf.Protocol;
using Launcher.Mvvm;
using Launcher.Services;
using System.Windows.Media;

namespace Launcher.ViewModels
{
    public sealed class MainViewModel : ObservableObject, IDisposable
    {
        private const int ProtocolVersion = 5;
        private readonly StartupOptions _options;
        private readonly Func<ILobbyClient> _clientFactory;
        private readonly IGameLauncher _game;
        private readonly IWindowService _window;
        private readonly LobbySession _session = new LobbySession();
        private readonly AutoRunner _auto;
        private object _currentView;
        private string _windowTitle = "LoL Portfolio Launcher";
        private ImageSource _backdrop;
        private string _lobbyHost = "127.0.0.1";
        private string _versionWarning = "";

        public MainViewModel(StartupOptions options, RosterData roster, Func<ILobbyClient> clientFactory, IGameLauncher game, IWindowService window)
        {
            _options = options;
            _clientFactory = clientFactory;
            _game = game;
            _window = window;
            var history = new HistoryViewModel(game);
            var chat = new ChatViewModel(_session);
            Login = new LoginViewModel(ConnectAsync);
            Rooms = new RoomListViewModel(_session, history);
            Lobby = new LobbyViewModel(_session, chat, history, window);
            ChampSelect = new ChampSelectViewModel(_session, roster, chat, image => Backdrop = image);
            InGame = new InGameViewModel(_session, game, Lobby.LeaveCommand, Lobby.DestroyCommand);
            Chat = chat;
            _auto = new AutoRunner(options, _session, Lobby, game, () => Login.Name);
            Backdrop = roster.Find(PLAYER_CHAMPION_TYPE.PlayerTypeAnnie)?.Skins.FirstOrDefault()?.Splash;
            CurrentView = Login;

            if (options.TryGet("--server", out string server))
                Login.Server = server;
            else if (ServerAddressStore.Load() is string saved)
                Login.Server = saved;

            if (options.TryGet("--auto-name", out string name))
                Login.Name = name;
        }

        public LoginViewModel Login { get; }

        public RoomListViewModel Rooms { get; }

        public LobbyViewModel Lobby { get; }

        public ChampSelectViewModel ChampSelect { get; }

        public InGameViewModel InGame { get; }

        public ChatViewModel Chat { get; }

        public object CurrentView
        {
            get => _currentView;
            private set => Set(ref _currentView, value);
        }

        public string WindowTitle
        {
            get => _windowTitle;
            private set => Set(ref _windowTitle, value);
        }

        public ImageSource Backdrop
        {
            get => _backdrop;
            private set => Set(ref _backdrop, value);
        }

        // 자동 실행 인자가 있으면 창이 뜬 뒤 바로 로그인한다.
        public async Task StartAsync()
        {
            if (!_options.Has("--auto-name"))
                return;

            if (_auto.Pace > 0.0)
                await Task.Delay(TimeSpan.FromSeconds(_auto.Pace));
            await ConnectAsync();
        }

        // 입력한 서버에 접속하고 로그인을 요청한다.
        private async Task ConnectAsync()
        {
            Login.Message = "";
            string address = Login.Server.Trim();
            string[] parts = address.Split(':');

            if (parts.Length != 2 || !int.TryParse(parts[1], out int port))
            {
                Login.Message = "서버는 주소:포트 형식으로 입력하세요.";

                return;
            }

            Login.IsBusy = true;
            try
            {
                _session.Client?.Dispose();
                ILobbyClient client = _clientFactory();
                client.LoginReceived += OnLogin;
                client.RoomListReceived += OnRoomList;
                client.LobbyStateReceived += OnLobbyState;
                client.GameStartReceived += OnGameStart;
                client.ChatReceived += Chat.Receive;
                client.Disconnected += OnDisconnected;
                _session.Client = client;
                await client.ConnectAsync(parts[0], port);
                _lobbyHost = parts[0];
                ServerAddressStore.Save(address);
                client.Send(new C_Login { Name = Login.Name });
            }
            catch (Exception exception)
            {
                Login.Message = "서버에 연결할 수 없습니다: " + exception.Message;
                Login.IsBusy = false;
            }
        }

        private void OnDisconnected(string message)
        {
            CurrentView = Login;
            Login.Message = message;
            Login.IsBusy = false;
        }

        // 로그인 결과를 처리한다. 성공하면 서버가 방 목록을 보낸다.
        private void OnLogin(S_Login login)
        {
            Login.IsBusy = false;

            if (!login.Success)
            {
                Login.Message = login.Reason;
                _session.Client.Dispose();

                return;
            }

            _session.AccountId = login.AccountId;
            WindowTitle = "LoL Portfolio Launcher - " + login.Name;

            if (login.ProtocolVersion != ProtocolVersion)
                _versionWarning = $"서버 버전이 다릅니다 (서버 {login.ProtocolVersion}, 런처 {ProtocolVersion}). 서버를 다시 빌드해 실행하세요.";
        }

        private void OnRoomList(S_RoomList list)
        {
            _session.Rooms = list;
            _session.State = null;
            Rooms.Update(list);
            Rooms.Message = WithWarning(list.Message);
            CurrentView = Rooms;
            _auto.RunRoomStep();
        }

        // 서버 상태에 맞는 화면으로 전환한다.
        private void OnLobbyState(S_LobbyState state)
        {
            _session.State = state;

            if (!string.IsNullOrEmpty(state.RoomName))
                Lobby.Title = "사용자 설정 게임 · " + state.RoomName;
            Lobby.RefreshHost();
            InGame.Update();

            switch (state.Phase)
            {
            case LOBBY_PHASE.Lobby:
                Lobby.Update();
                Lobby.Message = WithWarning(state.Message);
                CurrentView = Lobby;
                break;
            case LOBBY_PHASE.ChampSelect:
                ChampSelect.Update();
                ChampSelect.Message = state.Message;
                CurrentView = ChampSelect;
                break;
            case LOBBY_PHASE.InGame:
                CurrentView = InGame;
                break;
            }

            _auto.RunStateSteps();
        }

        // 입장 토큰으로 게임을 실행하고, 종료되면 로비로 복귀한다.
        private void OnGameStart(S_GameStart start)
        {
            _options.TryGet("--client-args", out string extra);
            string address = GameLauncher.ResolveAddress(start.Host, _lobbyHost);

            if (!_game.TryStart(address, start.Port, start.Token, extra, OnGameExited))
            {
                InGame.Text = "Client.exe를 찾을 수 없습니다 (Binaries 폴더).";

                return;
            }

            InGame.Text = "게임 클라이언트가 실행 중입니다. 게임을 끝내면 로비로 돌아옵니다.";
            _window.Minimize();
        }

        private void OnGameExited()
        {
            _window.Restore();

            if (_session.State != null)
                OnLobbyState(_session.State);

            if (_options.Has("--exit-after-game"))
                _window.Close();
        }

        private string WithWarning(string message) => string.IsNullOrEmpty(message) ? _versionWarning : message;

        public void Dispose() => _session.Client?.Dispose();
    }
}
