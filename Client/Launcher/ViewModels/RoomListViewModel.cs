// 방 목록 화면. 서버가 보낸 목록을 표시용 항목으로 바꾼다.
using Google.Protobuf.Protocol;
using Launcher.Mvvm;
using Launcher.Services;
using System.Windows.Input;

namespace Launcher.ViewModels
{
    public sealed class RoomListViewModel : ObservableObject
    {
        private readonly LobbySession _session;
        private string _subtitle = "";
        private string _message = "";
        private string _roomName = "";
        private IReadOnlyList<RoomItem> _rooms = Array.Empty<RoomItem>();

        public RoomListViewModel(LobbySession session, HistoryViewModel history)
        {
            _session = session;
            History = history;
            CreateCommand = new RelayCommand(() => _session.CreateRoom(RoomName.Trim()));
        }

        public HistoryViewModel History { get; }

        public ICommand CreateCommand { get; }

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

        public string RoomName
        {
            get => _roomName;
            set => Set(ref _roomName, value);
        }

        public IReadOnlyList<RoomItem> Rooms
        {
            get => _rooms;
            private set
            {
                if (Set(ref _rooms, value))
                    IsEmpty = value.Count == 0;
            }
        }

        private bool _isEmpty = true;

        public bool IsEmpty
        {
            get => _isEmpty;
            private set => Set(ref _isEmpty, value);
        }

        public void Update(S_RoomList list)
        {
            Subtitle = $"방 {list.Rooms.Count}개 · 진행 중인 게임 {list.RunningGames}/{list.MaxGames}";
            Rooms = list.Rooms.Select(ToItem).ToList();
        }

        private RoomItem ToItem(RoomSummary room)
        {
            bool open = room.Phase == LOBBY_PHASE.Lobby && room.Players < room.MaxPlayers;
            string button = open ? "참가"
                : room.Phase == LOBBY_PHASE.InGame ? "게임 중"
                : room.Phase == LOBBY_PHASE.ChampSelect ? "챔피언 선택 중"
                : "가득 참";
            string bots = room.Bots > 0 ? $" + 봇 {room.Bots}" : "";
            string text = $"{room.Name}   ·   방장 {room.Host}   ·   {room.Players}/{room.MaxPlayers}명{bots}";
            int roomId = room.RoomId;

            return new RoomItem(text, button, open, new RelayCommand(() => _session.Send(LOBBY_ACTION.JoinRoom, roomId)));
        }
    }
}
