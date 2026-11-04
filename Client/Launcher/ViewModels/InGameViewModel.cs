// 게임 진행 중 화면. 재접속과 방 나가기를 처리한다.
using Google.Protobuf.Protocol;
using Launcher.Mvvm;
using Launcher.Services;
using System.Windows.Input;

namespace Launcher.ViewModels
{
    public sealed class InGameViewModel : ObservableObject
    {
        private readonly LobbySession _session;
        private readonly IGameLauncher _game;
        private string _text = "";
        private string _message = "";
        private bool _canRejoin;
        private bool _isHost;

        public InGameViewModel(LobbySession session, IGameLauncher game, ICommand leave, ICommand destroy)
        {
            _session = session;
            _game = game;
            LeaveCommand = leave;
            DestroyCommand = destroy;
            RejoinCommand = new RelayCommand(() => _session.Send(LOBBY_ACTION.RejoinGame));
        }

        public ICommand RejoinCommand { get; }

        public ICommand LeaveCommand { get; }

        public ICommand DestroyCommand { get; }

        public string Text
        {
            get => _text;
            set => Set(ref _text, value);
        }

        public string Message
        {
            get => _message;
            private set => Set(ref _message, value);
        }

        public bool CanRejoin
        {
            get => _canRejoin;
            private set => Set(ref _canRejoin, value);
        }

        public bool IsHost
        {
            get => _isHost;
            private set => Set(ref _isHost, value);
        }

        // 서버 상태를 화면 값으로 옮긴다.
        public void Update()
        {
            S_LobbyState state = _session.State;
            IsHost = _session.Me?.Host == true;
            CanRejoin = state.CanRejoin && !_game.IsRunning;
            Message = state.Phase == LOBBY_PHASE.InGame ? state.Message : "";

            if (state.Phase == LOBBY_PHASE.InGame && !_game.IsRunning)
                Text = state.CanRejoin ? "게임이 진행 중입니다. 재접속을 누르면 내 챔피언으로 돌아갑니다." : "게임을 불러오는 중...";
        }
    }
}
