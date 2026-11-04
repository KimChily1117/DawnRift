// 클라이언트 연결 세션.
using System;
using System.Collections.Generic;
using System.Text;
using System.Net.Sockets;
using System.Threading;
using System.Threading.Tasks;
using System.Diagnostics;
using ServerCore;
using System.Net;
using Google.Protobuf.Protocol;
using Google.Protobuf;
using Server.Game.Objects;
using Server.Game.Room;
using Server.Game.Lobbies;

namespace Server
{
    public partial class ClientSession : PacketSession, ILobbyClient
    {
        public int SessionId { get; set; }

        public bool InLobby { get; set; }
        public string GameToken { get; set; }

        public GameRoom GameRoom { get; set; }

        public Player Player { get; set; }

        private bool _hasProcessedMoveSequence;
        private uint _lastProcessedMoveSequence;
        private const double MoveRequestTokensPerSecond = 12.0;
        private const double MoveRequestBurstCapacity = 4.0;
        private double _moveRequestTokens = MoveRequestBurstCapacity;
        private long _moveRequestBudgetTimestamp;

        public uint LastProcessedMoveSequence => _lastProcessedMoveSequence;

        internal bool TryConsumeMoveSequence(uint sequence)
        {
            if (sequence == 0 || (_hasProcessedMoveSequence && sequence <= _lastProcessedMoveSequence))
                return false;

            _hasProcessedMoveSequence = true;
            _lastProcessedMoveSequence = sequence;

            return true;
        }

        internal bool TryConsumeMoveRequestBudget(long timestamp)
        {
            if (timestamp < 0)
                return false;

            if (_moveRequestBudgetTimestamp == 0)
            {
                _moveRequestBudgetTimestamp = timestamp;
            }
            else if (timestamp >= _moveRequestBudgetTimestamp)
            {
                double elapsedSeconds = (timestamp - _moveRequestBudgetTimestamp) / (double)Stopwatch.Frequency;
                _moveRequestTokens = Math.Min(MoveRequestBurstCapacity,
                                              _moveRequestTokens + elapsedSeconds * MoveRequestTokensPerSecond);
                _moveRequestBudgetTimestamp = timestamp;
            }

            if (_moveRequestTokens < 1.0)
                return false;
            _moveRequestTokens -= 1.0;

            return true;
        }

        internal void ResetMoveSequence()
        {
            _hasProcessedMoveSequence = false;
            _lastProcessedMoveSequence = 0;
            _moveRequestTokens = MoveRequestBurstCapacity;
            _moveRequestBudgetTimestamp = 0;
        }

#region Network

        public Action<IMessage> TestSink { get; set; }

        public void Send(IMessage packet)
        {
            if (TestSink != null)
            {
                TestSink(packet);

                return;
            }

            string msgName = packet.Descriptor.Name.Replace("_", string.Empty);
            MsgId msgId = (MsgId)Enum.Parse(typeof(MsgId), msgName, ignoreCase: true);
            ushort size = (ushort)packet.CalculateSize();
            byte[] sendBuffer = new byte[size + 4];
            Array.Copy(BitConverter.GetBytes((ushort)(size + 4)), 0, sendBuffer, 0, sizeof(ushort));
            Array.Copy(BitConverter.GetBytes((ushort)msgId), 0, sendBuffer, 2, sizeof(ushort));
            Array.Copy(packet.ToByteArray(), 0, sendBuffer, 4, size);
            Send(new ArraySegment<byte>(sendBuffer));
        }

        public override void OnConnected(EndPoint endPoint)
        {
            Console.WriteLine($"OnConnected : {endPoint}");

            {
                var recv = new S_EnterGame();
                recv.AccountId = (ulong)SessionId;
                recv.Success = true;
                recv.FogOfWar = Server.Game.Vision.VisionSystem.FogEnabled;
                recv.Practice = Environment.GetEnvironmentVariable("DAWN_PRACTICE") == "1";
                recv.ProtocolVersion = ProtocolInfo.Version;
                Send(recv);
            }
        }

        private static readonly HashSet<ushort> AllowedWhilePaused = new HashSet<ushort> {
            (ushort)MsgId.CTestMsg, (ushort)MsgId.CChatMessage, (ushort)MsgId.CPing,     (ushort)MsgId.CEnterGame,
            (ushort)MsgId.CLogin,   (ushort)MsgId.CLobbyAction, (ushort)MsgId.CForceEnd, (ushort)MsgId.CSurrender,
        };

        public override void OnRecvPacket(ArraySegment<byte> buffer)
        {
            if (GameRoom?.Paused == true && buffer.Count >= 4 &&
                !AllowedWhilePaused.Contains(BitConverter.ToUInt16(buffer.Array, buffer.Offset + 2)))
                return;
            PacketManager.Instance.OnRecvPacket(this, buffer);
        }

        public override void OnDisconnected(EndPoint endPoint)
        {
            GameRoom room = GameRoom;
            room?.OnSessionDisconnected(this);

            if (InLobby)
                LobbyManager.Instance.Leave((ulong)SessionId);

            if (!string.IsNullOrEmpty(GameToken))
                LobbyManager.Instance.OnGameClientLeft(GameToken);
            SessionManager.Instance.Remove(this);
            Console.WriteLine($"OnDisconnected : {endPoint}");
        }

        public override void OnSend(int numOfBytes) {}

#endregion Network
    }
}
