// 접속 중인 세션의 공유 상태와 서버 요청을 모은다.
using Google.Protobuf.Protocol;

namespace Launcher.Services
{
    public sealed class LobbySession
    {
        public ILobbyClient Client { get; set; }

        public ulong AccountId { get; set; }

        public S_LobbyState State { get; set; }

        public S_RoomList Rooms { get; set; }

        public LobbyMember Me => State?.Members.FirstOrDefault(m => m.AccountId == AccountId);

        public void Send(LOBBY_ACTION action, int value = 0) =>
            Client?.Send(new C_LobbyAction { Action = action, Value = value });

        public void CreateRoom(string name) =>
            Client?.Send(new C_LobbyAction { Action = LOBBY_ACTION.CreateRoom, Text = name });

        public void Chat(string message) =>
            Client?.Send(new C_ChatMessage { Message = message, Channel = CHAT_CHANNEL.ChatLobby });
    }
}
