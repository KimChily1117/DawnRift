// 로비 채팅. 로비와 챔피언 선택 화면이 함께 쓴다.
using Google.Protobuf.Protocol;
using Launcher.Mvvm;
using Launcher.Services;
using System.Collections.ObjectModel;
using System.Windows.Input;

namespace Launcher.ViewModels
{
    public sealed class ChatViewModel : ObservableObject
    {
        private const int MaxLines = 80;
        private readonly LobbySession _session;
        private string _draft = "";

        public ChatViewModel(LobbySession session)
        {
            _session = session;
            SendCommand = new RelayCommand(Send);
        }

        public ObservableCollection<string> Lines { get; } = new ObservableCollection<string>();

        public ICommand SendCommand { get; }

        public string Draft
        {
            get => _draft;
            set => Set(ref _draft, value);
        }

        public void Receive(S_ChatMessage chat)
        {
            Lines.Add(chat.SenderId == 0 ? chat.Message : $"{chat.SenderName}: {chat.Message}");

            while (Lines.Count > MaxLines)
                Lines.RemoveAt(0);
        }

        private void Send()
        {
            string text = Draft.Trim();
            Draft = "";

            if (text.Length > 0)
                _session.Chat(text);
        }
    }
}
