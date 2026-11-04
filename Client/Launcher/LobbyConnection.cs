// 런처의 TCP 세션. 수신 패킷을 UI 스레드로 전달한다.
using Google.Protobuf;
using Google.Protobuf.Protocol;
using Launcher.Services;
using System.IO;
using System.Net.Sockets;

namespace Launcher
{
    public sealed class LobbyConnection : ILobbyClient
    {
        private TcpClient _client;
        private NetworkStream _stream;
        private readonly object _sendLock = new object();
        private readonly Action<Action> _dispatch;

        public event Action<S_Login> LoginReceived;
        public event Action<S_LobbyState> LobbyStateReceived;
        public event Action<S_GameStart> GameStartReceived;
        public event Action<S_ChatMessage> ChatReceived;
        public event Action<S_RoomList> RoomListReceived;
        public event Action<string> Disconnected;

        public LobbyConnection(Action<Action> dispatch)
        {
            _dispatch = dispatch;
        }

        // 서버에 접속하고 수신 루프를 시작한다.
        public async Task ConnectAsync(string host, int port)
        {
            _client = new TcpClient { NoDelay = true };
            await _client.ConnectAsync(host, port);
            _stream = _client.GetStream();
            _ = Task.Run(ReceiveLoop);
        }

        // 패킷을 [크기][메시지 ID][본문] 형식으로 보낸다.
        public void Send(IMessage packet)
        {
            string name = packet.Descriptor.Name.Replace("_", string.Empty);
            var id = (MsgId)Enum.Parse(typeof(MsgId), name, ignoreCase: true);
            byte[] body = packet.ToByteArray();
            var buffer = new byte[body.Length + 4];
            BitConverter.GetBytes((ushort)buffer.Length).CopyTo(buffer, 0);
            BitConverter.GetBytes((ushort)id).CopyTo(buffer, 2);
            body.CopyTo(buffer, 4);
            lock (_sendLock) _stream?.Write(buffer, 0, buffer.Length);
        }

        // 패킷을 읽고, 연결이 끊기면 UI 스레드에 알린다.
        private async Task ReceiveLoop()
        {
            var header = new byte[4];
            try
            {
                while (true)
                {
                    await ReadExactly(header, 4);
                    ushort size = BitConverter.ToUInt16(header, 0);
                    var id = (MsgId)BitConverter.ToUInt16(header, 2);
                    var body = new byte[size - 4];
                    await ReadExactly(body, body.Length);
                    Dispatch(id, body);
                }
            }
            catch (Exception exception)
                when (exception is IOException || exception is ObjectDisposedException || exception is SocketException)
            {
                _dispatch(() => Disconnected?.Invoke("서버와 연결이 끊어졌습니다."));
            }
        }

        // 메시지 종류별로 역직렬화해 UI 스레드 이벤트로 전달한다.
        private void Dispatch(MsgId id, byte[] body)
        {
            switch (id)
            {
            case MsgId.SLogin:
                var login = S_Login.Parser.ParseFrom(body);
                _dispatch(() => LoginReceived?.Invoke(login));
                break;
            case MsgId.SLobbyState:
                var state = S_LobbyState.Parser.ParseFrom(body);
                _dispatch(() => LobbyStateReceived?.Invoke(state));
                break;
            case MsgId.SGameStart:
                var start = S_GameStart.Parser.ParseFrom(body);
                _dispatch(() => GameStartReceived?.Invoke(start));
                break;
            case MsgId.SChatMessage:
                var chat = S_ChatMessage.Parser.ParseFrom(body);
                _dispatch(() => ChatReceived?.Invoke(chat));
                break;
            case MsgId.SRoomList:
                var rooms = S_RoomList.Parser.ParseFrom(body);
                _dispatch(() => RoomListReceived?.Invoke(rooms));
                break;
            }
        }

        // 요청한 길이를 다 읽을 때까지 기다린다.
        private async Task ReadExactly(byte[] buffer, int count)
        {
            int read = 0;

            while (read < count)
            {
                int n = await _stream.ReadAsync(buffer.AsMemory(read, count - read));

                if (n == 0)
                    throw new IOException("closed");
                read += n;
            }
        }

        public void Dispose()
        {
            _stream?.Dispose();
            _client?.Dispose();
        }
    }
}
