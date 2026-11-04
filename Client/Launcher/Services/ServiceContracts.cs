// ViewModel이 의존하는 외부 기능의 인터페이스. 실제 소켓, 프로세스, 창 없이 대체할 수 있다.
using Google.Protobuf;
using Google.Protobuf.Protocol;

namespace Launcher.Services
{
    public interface ILobbyClient : IDisposable
    {
        event Action<S_Login> LoginReceived;
        event Action<S_LobbyState> LobbyStateReceived;
        event Action<S_GameStart> GameStartReceived;
        event Action<S_ChatMessage> ChatReceived;
        event Action<S_RoomList> RoomListReceived;
        event Action<string> Disconnected;

        Task ConnectAsync(string host, int port);

        void Send(IMessage packet);
    }

    public interface IGameLauncher
    {
        bool IsRunning { get; }

        // 게임을 실행한다. 실행 파일이 없으면 false를 돌려준다.
        bool TryStart(string address, int port, string token, string extraArguments, Action exited);

        // 경기 기록이 저장된 폴더. 없으면 null이다.
        string MatchHistoryFolder { get; }
    }

    public interface IWindowService
    {
        void Minimize();

        void Restore();

        void Close();

        bool Confirm(string message, string title);
    }
}
