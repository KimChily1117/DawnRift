// 로비 화면. 팀 슬롯과 방장 권한을 서버 상태에서 계산한다.
using Google.Protobuf.Protocol;
using Launcher.Mvvm;
using Launcher.Services;
using System.Windows.Input;

namespace Launcher.ViewModels
{
    public sealed class LobbyViewModel : ObservableObject
    {
        private const int TeamSize = 5;
        private readonly LobbySession _session;
        private readonly RelayCommand _addBlueBot;
        private readonly RelayCommand _removeBlueBot;
        private readonly RelayCommand _addRedBot;
        private readonly RelayCommand _removeRedBot;
        private readonly RelayCommand _fillBots;
        private readonly RelayCommand _start;
        private bool _syncing;
        private string _title = "사용자 설정 게임 · 소환사의 협곡";
        private string _subtitle = "";
        private string _message = "";
        private string _startText = "";
        private bool _isHost;
        private bool _practice;
        private int _positionIndex;
        private int _difficultyIndex;
        private IReadOnlyList<SlotItem> _blueSlots = Array.Empty<SlotItem>();
        private IReadOnlyList<SlotItem> _redSlots = Array.Empty<SlotItem>();

        public LobbyViewModel(LobbySession session, ChatViewModel chat, HistoryViewModel history, IWindowService window)
        {
            _session = session;
            Chat = chat;
            History = history;
            _addBlueBot = new RelayCommand(() => Send(LOBBY_ACTION.SetBotsBlue, State.BotsBlue + 1), () => IsHost && State.BotsBlue < FreeSlots(1));
            _removeBlueBot = new RelayCommand(() => Send(LOBBY_ACTION.SetBotsBlue, Math.Max(0, State.BotsBlue - 1)), () => IsHost && State.BotsBlue > 0);
            _addRedBot = new RelayCommand(() => Send(LOBBY_ACTION.SetBotsRed, State.BotsRed + 1), () => IsHost && State.BotsRed < FreeSlots(2));
            _removeRedBot = new RelayCommand(() => Send(LOBBY_ACTION.SetBotsRed, Math.Max(0, State.BotsRed - 1)), () => IsHost && State.BotsRed > 0);
            _fillBots = new RelayCommand(FillBots, () => IsHost && (State.BotsBlue < FreeSlots(1) || State.BotsRed < FreeSlots(2)));
            _start = new RelayCommand(() => Send(LOBBY_ACTION.StartChampSelect), () => IsHost);
            JoinBlueCommand = new RelayCommand(() => Send(LOBBY_ACTION.JoinTeam, 1));
            JoinRedCommand = new RelayCommand(() => Send(LOBBY_ACTION.JoinTeam, 2));
            LeaveCommand = new RelayCommand(() => Send(LOBBY_ACTION.LeaveRoom));
            DestroyCommand = new RelayCommand(() =>
            {
                if (window.Confirm("방을 닫으면 진행 중인 게임도 끝납니다. 닫을까요?", "방 터트리기"))
                    Send(LOBBY_ACTION.DestroyRoom);
            });
        }

        public ChatViewModel Chat { get; }

        public HistoryViewModel History { get; }

        public IReadOnlyList<string> PositionLabels { get; } = GameCatalog.Positions.Select(p => p.Label).ToList();

        public ICommand AddBlueBotCommand => _addBlueBot;

        public ICommand RemoveBlueBotCommand => _removeBlueBot;

        public ICommand AddRedBotCommand => _addRedBot;

        public ICommand RemoveRedBotCommand => _removeRedBot;

        public ICommand FillBotsCommand => _fillBots;

        public ICommand StartCommand => _start;

        public ICommand JoinBlueCommand { get; }

        public ICommand JoinRedCommand { get; }

        public ICommand LeaveCommand { get; }

        public ICommand DestroyCommand { get; }

        public string Title
        {
            get => _title;
            set => Set(ref _title, value);
        }

        public string Subtitle
        {
            get => _subtitle;
            private set => Set(ref _subtitle, value);
        }

        public string Message
        {
            get => _message;
            set => Set(ref _message, value);
        }

        public string StartText
        {
            get => _startText;
            private set => Set(ref _startText, value);
        }

        public bool IsHost
        {
            get => _isHost;
            private set => Set(ref _isHost, value);
        }

        public IReadOnlyList<SlotItem> BlueSlots
        {
            get => _blueSlots;
            private set => Set(ref _blueSlots, value);
        }

        public IReadOnlyList<SlotItem> RedSlots
        {
            get => _redSlots;
            private set => Set(ref _redSlots, value);
        }

        // 사용자가 바꾼 값만 서버로 보낸다. 서버 상태를 반영하는 중에는 보내지 않는다.
        public int PositionIndex
        {
            get => _positionIndex;
            set
            {
                if (Set(ref _positionIndex, value) && !_syncing && value >= 0)
                    Send(LOBBY_ACTION.SetPosition, (int)GameCatalog.Positions[value].Position);
            }
        }

        public int DifficultyIndex
        {
            get => _difficultyIndex;
            set
            {
                if (Set(ref _difficultyIndex, value) && !_syncing && value >= 0)
                    Send(LOBBY_ACTION.SetBotDifficulty, value);
            }
        }

        public bool Practice
        {
            get => _practice;
            set
            {
                if (Set(ref _practice, value) && !_syncing)
                    Send(LOBBY_ACTION.TogglePractice, value ? 1 : 0);
            }
        }

        private S_LobbyState State => _session.State;

        // 서버 상태를 화면 값으로 옮긴다.
        public void Update()
        {
            LobbyMember me = _session.Me;
            int bots = State.BotsBlue + State.BotsRed;
            string botText = bots > 0 ? $" · 봇 {bots}명 ({GameCatalog.DifficultyLabel(State.BotDifficulty)})" : "";
            Subtitle = $"참가자 {State.Members.Count}/10{botText} · 방장이 챔피언 선택을 시작합니다";
            BlueSlots = BuildSlots(1, State.BotsBlue);
            RedSlots = BuildSlots(2, State.BotsRed);
            IsHost = me?.Host == true;
            StartText = IsHost ? "챔피언 선택 시작" : "방장을 기다리는 중";

            _syncing = true;
            DifficultyIndex = (int)State.BotDifficulty;
            Practice = State.Practice;

            if (me != null)
                PositionIndex = Array.FindIndex(GameCatalog.Positions, p => p.Position == me.Position);
            _syncing = false;

            foreach (RelayCommand command in new[] { _addBlueBot, _removeBlueBot, _addRedBot, _removeRedBot, _fillBots, _start })
                command.Refresh();
        }

        public void RefreshHost() => IsHost = _session.Me?.Host == true;

        private List<SlotItem> BuildSlots(int team, int botCount)
        {
            var members = State.Members.Where(m => m.Team == team).ToList();
            var slots = new List<SlotItem>(TeamSize);

            for (int i = 0; i < TeamSize; ++i)
            {
                if (i < members.Count)
                    slots.Add(ToSlot(members[i]));
                else if (i < members.Count + botCount)
                    slots.Add(new SlotItem(SlotKind.Bot, "봇", $"봇 플레이어 ({GameCatalog.DifficultyLabel(State.BotDifficulty)})"));
                else
                    slots.Add(new SlotItem(SlotKind.Empty, "", "빈 자리"));
            }

            return slots;
        }

        private SlotItem ToSlot(LobbyMember member)
        {
            string label = member.Position == LOBBY_POSITION.PositionNone
                ? ""
                : GameCatalog.Positions.First(p => p.Position == member.Position).Label;
            SlotKind kind = member.AccountId == _session.AccountId ? SlotKind.Me : SlotKind.Player;

            return new SlotItem(kind, label, member.Name + (member.Host ? "  ♛" : ""));
        }

        private int FreeSlots(int team) => State == null ? 0 : TeamSize - State.Members.Count(m => m.Team == team);

        public void FillBots()
        {
            Send(LOBBY_ACTION.SetBotsBlue, FreeSlots(1));
            Send(LOBBY_ACTION.SetBotsRed, FreeSlots(2));
        }

        private void Send(LOBBY_ACTION action, int value = 0) => _session.Send(action, value);
    }
}
