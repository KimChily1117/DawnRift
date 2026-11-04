// 패킷별 처리기.
using Google.Protobuf;
using Google.Protobuf.Protocol;
using Server.Game.Lobbies;
using Server;
using Server.Game.Objects;
using Server.Game.Room;
using Server.Game.Movement;
using ServerCore;
using System;
using System.Numerics;
using System.Collections.Generic;
using System.Linq;
using System.Net.Sockets;
using System.Text;

using Vec3 = System.Numerics.Vector3;
using System.Threading.Tasks;
using Server.Game;

class PacketHandler
{
    private const string CombatGatherCommand = "combat-gather-v1";

    private static bool CombatTestCommandsEnabled
    {
        get {
#if DEBUG
            return true;
#else
            return string.Equals(Environment.GetEnvironmentVariable("D3D_ENABLE_TEST_COMMANDS"), "1",
                                 StringComparison.Ordinal);
#endif
        }
    }

    private static int GetSkillRange(int skillId)
    {
        switch (skillId)
        {
        case 1:
            return 4;
        case 2:
            return 6;
        case 3:
            return 5;
        case 4:
            return 5;
        default:
            return 4;
        }
    }

    public static void C_TestMsgHandler(PacketSession session, IMessage message)
    {
        C_TestMsg testPacket = message as C_TestMsg;
        ClientSession clientSession = session as ClientSession;

        if (testPacket == null || clientSession == null)
            return;

        if (string.Equals(testPacket.Message, CombatGatherCommand, StringComparison.Ordinal))
        {
            if (!CombatTestCommandsEnabled)
            {
                clientSession.Send(new S_TestMsg {
                    Message = "Combat gather is disabled. Use a Debug server or set D3D_ENABLE_TEST_COMMANDS=1."
                });

                return;
            }

            GameRoom room = clientSession.GameRoom;
            Player requester = clientSession.Player;

            if (room == null || requester?.Info == null)
            {
                clientSession.Send(new S_TestMsg { Message = "Combat gather rejected: no active Player or Room." });

                return;
            }

            room.Push(() =>
                      {
                          if (clientSession.IsDisconnected || !ReferenceEquals(clientSession.GameRoom, room) ||
                              !ReferenceEquals(clientSession.Player, requester) ||
                              !ReferenceEquals(requester.Room, room) || !room.ContainsPlayer(requester))
                          {
                              return;
                          }

                          room.TryGatherPlayersForCombatTest(requester, out string resultMessage);
                          clientSession.Send(new S_TestMsg { Message = resultMessage });
                      });

            return;
        }

        Console.WriteLine("recv Test : " + testPacket.Message);
        clientSession.Send(new S_TestMsg { Message = "C# to C++ : Hello Client!!!" });
    }

    public static void C_ChatMessageHandler(PacketSession session, IMessage message)
    {
        if (!(message is C_ChatMessage packet) || !(session is ClientSession clientSession))
            return;

        if (clientSession.InLobby)
        {
            LobbyManager.Instance.Chat((ulong)clientSession.SessionId, packet.Message);

            return;
        }

        GameRoom room = clientSession.GameRoom;
        Player player = clientSession.Player;

        if (room == null || player?.Info == null)
            return;
        room.Push(() =>
                  {
                      if (!ReferenceEquals(clientSession.Player, player) || !ReferenceEquals(player.Room, room) ||
                          !room.ContainsPlayer(player))
                          return;
                      Chat.Send(room, player, packet.Channel, packet.Message);
                  });
    }

    public static void C_MoveHandler(PacketSession session, IMessage message)
    {
        var movePacket = message as C_Move;

        if (movePacket == null)
            return;

        if (!MovementProtocolPolicy.LegacyMoveEnabled)
        {
            Console.WriteLine(
                "[Movement] Deprecated C_Move rejected. Set D3D_ALLOW_LEGACY_MOVE=1 only for development compatibility.");

            return;
        }

        Console.WriteLine(
            $"[Server] Received Move Packet: {movePacket.ObjectId} to ({movePacket.TargetPos.X}, {movePacket.TargetPos.Y}, {movePacket.TargetPos.Z}) | Tile({movePacket.CellPos.X}, {movePacket.CellPos.Z})");

        ClientSession clientSession = (ClientSession)session;
        GameRoom room = clientSession.GameRoom;
        Player sessionPlayer = clientSession.Player;

        if (room == null || sessionPlayer?.Info == null)
            return;

        if (movePacket.ObjectId != sessionPlayer.Info.ObjectId)
        {
            Console.WriteLine(
                $"[Security] Rejected C_Move objectId {movePacket.ObjectId}; session owns {sessionPlayer.Info.ObjectId}.");

            return;
        }

        room.Push(
            () =>
            {
                if (clientSession.GameRoom != room || clientSession.Player != sessionPlayer)
                    return;

                if (sessionPlayer.Info.Hp <= 0 || sessionPlayer.IsMovementLockedByCombat)
                    return;
                GameObject obj = sessionPlayer;

                int tileX = movePacket.CellPos.X;
                int tileZ = movePacket.CellPos.Z;

                if (tileX < 0 || tileZ < 0)
                {
                    Console.WriteLine("[Server] ❌ Move failed: Out of bounds!");

                    return;
                }

                Tile targetTile = room._tilemap.GetTileAt(new Vec3(tileX, 0, tileZ));

                if (targetTile == null || !targetTile.IsWalkable)
                {
                    Console.WriteLine("[Server] 🚧 Invalid Move: Tile is not walkable!");

                    return;
                }

                int prevTileX = obj.TileX;
                int prevTileZ = obj.TileZ;

                Vec3 correctedPos = new Vec3(tileX + 0.5f, movePacket.TargetPos.Y, tileZ + 0.5f);

                obj.Info.Position = correctedPos.ToProtoVector3();
                obj.TileX = tileX;
                obj.TileZ = tileZ;

                room.UpdatePlayerTilePosition(obj, prevTileX, prevTileZ, tileX, tileZ);

                S_Move sMovePacket = new S_Move { Info = obj.Info };
                room.Broadcast(sMovePacket);

                Console.WriteLine(
                    $"[Server] ✅ Player {movePacket.ObjectId} moved to corrected Position ({correctedPos.X}, {correctedPos.Y}, {correctedPos.Z}) | New Tile ({tileX}, {tileZ})");
            });
    }

    public static void C_MoveRequestHandler(PacketSession session, IMessage message)
    {
        C_MoveRequest request = message as C_MoveRequest;
        ClientSession clientSession = session as ClientSession;

        if (request == null || clientSession == null)
            return;

        GameRoom room = clientSession.GameRoom;
        Player player = clientSession.Player;

        if (player?.Info == null)
        {
            SendMoveRejected(clientSession, request.ClientMoveSequence, MOVE_REJECT_REASON.NoPlayer, null, null,
                             request.NavigationMapId, request.NavigationContentHash);

            return;
        }

        if (room == null)
        {
            SendMoveRejected(clientSession, request.ClientMoveSequence, MOVE_REJECT_REASON.NoRoom, player, null,
                             request.NavigationMapId, request.NavigationContentHash);

            return;
        }

        room.Push(
            () =>
            {
                MoveRequestDecision decision = AuthoritativeMoveService.Process(clientSession, room, player, request);

                if (decision.Accepted)
                {
                    Recall.Cancel(room, player);
                    var accepted = new S_MoveAccepted {
                        ClientMoveSequence = decision.ClientMoveSequence,
                        ServerMoveId = decision.ServerMoveId,
                        NavigationMapId = room.NavigationMapId,
                        NavigationContentHash = room.NavigationContentHash,
                        RequestedDestination = decision.RequestedDestination.ToProtoVector3(),
                        AcceptedDestination = decision.AcceptedDestination.ToProtoVector3(),
                        ServerStartPosition = decision.ServerStartPosition.ToProtoVector3(),
                        WasDestinationAdjusted = decision.WasDestinationAdjusted,
                    };
                    clientSession.Send(accepted);
                    Console.WriteLine(
                        "[Movement] Accepted Room=" + room.RoomId + " Player=" + player.Info.ObjectId +
                        " Sequence=" + decision.ClientMoveSequence + " ServerMoveId=" + decision.ServerMoveId +
                        " Adjusted=" + decision.WasDestinationAdjusted + " Expanded=" + decision.PathExpandedNodeCount +
                        " PathMs=" + decision.PathfindingElapsedMilliseconds.ToString("F3"));

                    return;
                }

                SendMoveRejected(clientSession, decision.ClientMoveSequence, decision.RejectReason, player, room);
                Console.WriteLine("[Movement] Rejected Room=" + room.RoomId + " Player=" + player.Info.ObjectId +
                                  " Sequence=" + decision.ClientMoveSequence + " Reason=" + decision.RejectReason);
            });
    }

    public static void C_LevelUpSkillHandler(PacketSession session, IMessage message)
    {
        if (!(message is C_LevelUpSkill packet))
            return;
        var clientSession = (ClientSession)session;
        GameRoom room = clientSession.GameRoom;
        Player player = clientSession.Player;

        if (room == null || player?.Info == null)
            return;
        room.Push(() =>
                  {
                      if (!ReferenceEquals(clientSession.Player, player) || !ReferenceEquals(player.Room, room) ||
                          !room.ContainsPlayer(player))
                          return;

                      if (Progression.LevelUpSkill(player, (SkillType)packet.SkillId))
                          Progression.SendStats(room, player);
                  });
    }

    public static void C_ItemActionHandler(PacketSession session, IMessage message)
    {
        if (!(message is C_ItemAction packet))
            return;
        var clientSession = (ClientSession)session;
        GameRoom room = clientSession.GameRoom;
        Player player = clientSession.Player;

        if (room == null || player?.Info == null)
            return;
        System.Numerics.Vector3 position =
            packet.Position != null ? packet.Position.ToNumericsVector3() : player.Info.Position.ToNumericsVector3();
        room.Push(
            () =>
            {
                if (!ReferenceEquals(clientSession.Player, player) || !ReferenceEquals(player.Room, room) ||
                    !room.ContainsPlayer(player) || !Progression.IsChampion(player))
                    return;
                SKILL_REJECT_REASON reason;
                int rejectId = Server.Game.Items.Inventory.ShopRejectSkillId;

                switch (packet.Action)
                {
                case ITEM_ACTION.Buy:
                    reason = player.Items.Buy(room, player, packet.ItemId);
                    break;
                case ITEM_ACTION.Sell:
                    reason = player.Items.Sell(room, player, packet.Slot);
                    break;
                case ITEM_ACTION.Undo:
                    reason = player.Items.Undo(room, player);
                    break;
                case ITEM_ACTION.Use:
                    rejectId = Server.Game.Items.Inventory.UseRejectSkillId + packet.Slot;
                    reason = Server.Game.Items.ItemEffects.Use(room, player, packet.Slot, position, packet.TargetId);
                    break;
                default:
                    return;
                }

                if (reason != SKILL_REJECT_REASON.SkillRejectNone)
                {
                    Console.WriteLine(
                        $"[Items] {player.Info.ObjectId} {packet.Action} {packet.ItemId}/{packet.Slot} rejected: {reason}");
                    clientSession.Send(new S_SkillRejected { SkillId = rejectId, Reason = reason });

                    return;
                }

                player.Items.Send(room, player);
                Progression.SendStats(room, player);
            });
    }

    private static bool RoomPlayer(PacketSession session, out ClientSession clientSession, out GameRoom room,
                                   out Player player)
    {
        clientSession = (ClientSession)session;
        room = clientSession.GameRoom;
        player = clientSession.Player;

        return room != null && player?.Info != null;
    }

    public static void C_RecallHandler(PacketSession session, IMessage message)
    {
        if (!(message is C_Recall) ||
            !RoomPlayer(session, out ClientSession clientSession, out GameRoom room, out Player player))
            return;
        room.Push(() =>
                  {
                      if (ReferenceEquals(clientSession.Player, player) && ReferenceEquals(player.Room, room) &&
                          room.ContainsPlayer(player))
                          Recall.Start(room, player);
                  });
    }

    public static void C_SurrenderHandler(PacketSession session, IMessage message)
    {
        if (!(message is C_Surrender packet) ||
            !RoomPlayer(session, out ClientSession clientSession, out GameRoom room, out Player player))
            return;
        room.Push(() =>
                  {
                      if (ReferenceEquals(clientSession.Player, player) && ReferenceEquals(player.Room, room) &&
                          room.ContainsPlayer(player))
                          Surrender.Vote(room, player, packet.Agree);
                  });
    }

    public static void C_PracticeHandler(PacketSession session, IMessage message)
    {
        if (!(message is C_Practice packet) ||
            !RoomPlayer(session, out ClientSession clientSession, out GameRoom room, out Player player))
            return;
        System.Numerics.Vector3 position =
            packet.Position != null ? packet.Position.ToNumericsVector3() : player.Info.Position.ToNumericsVector3();
        room.Push(() =>
                  {
                      if (!ReferenceEquals(clientSession.Player, player) || !ReferenceEquals(player.Room, room) ||
                          !room.ContainsPlayer(player) || !room.Practice || !Progression.IsChampion(player))
                          return;
                      Practice.Apply(room, player, packet.Action, position);
                  });
    }

    public static void C_PingHandler(PacketSession session, IMessage message)
    {
        if (!(message is C_Ping packet))
            return;
        var clientSession = (ClientSession)session;
        GameRoom room = clientSession.GameRoom;
        Player player = clientSession.Player;

        if (room == null || player?.Info == null)
            return;
        System.Numerics.Vector3 position =
            packet.Position != null ? packet.Position.ToNumericsVector3() : player.Info.Position.ToNumericsVector3();
        room.Push(() =>
                  {
                      if (!ReferenceEquals(clientSession.Player, player) || !ReferenceEquals(player.Room, room) ||
                          !room.ContainsPlayer(player))
                          return;
                      Pings.Send(room, player, packet.Type, position, packet.TargetId);
                  });
    }

    public static void C_PlaceWardHandler(PacketSession session, IMessage message)
    {
        if (!(message is C_PlaceWard packet) || packet.Position == null)
            return;
        var clientSession = (ClientSession)session;
        GameRoom room = clientSession.GameRoom;
        Player player = clientSession.Player;

        if (room == null || player?.Info == null)
            return;
        System.Numerics.Vector3 position = packet.Position.ToNumericsVector3();
        room.Push(() =>
                  {
                      if (!ReferenceEquals(clientSession.Player, player) || !ReferenceEquals(player.Room, room) ||
                          !room.ContainsPlayer(player))
                          return;
                      Trinket.Place(room, player, position);
                  });
    }

    public static void C_SummonerSpellHandler(PacketSession session, IMessage message)
    {
        if (!(message is C_SummonerSpell packet))
            return;
        var clientSession = (ClientSession)session;
        GameRoom room = clientSession.GameRoom;
        Player player = clientSession.Player;

        if (room == null || player?.Info == null)
            return;
        System.Numerics.Vector3 target =
            packet.TargetPos != null ? packet.TargetPos.ToNumericsVector3() : player.Info.Position.ToNumericsVector3();
        room.Push(() =>
                  {
                      if (!ReferenceEquals(clientSession.Player, player) || !ReferenceEquals(player.Room, room) ||
                          !room.ContainsPlayer(player))
                          return;
                      SummonerSpells.Cast(room, player, packet.Slot, packet.TargetId, target);
                  });
    }

    public static void C_StopCommandHandler(PacketSession session, IMessage message)
    {
        ClientSession clientSession = session as ClientSession;

        if (!(message is C_StopCommand) || clientSession == null)
            return;
        GameRoom room = clientSession.GameRoom;
        Player player = clientSession.Player;

        if (room == null || player?.Info == null)
            return;

        room.Push(() =>
                  {
                      if (!ReferenceEquals(clientSession.Player, player) ||
                          !ReferenceEquals(clientSession.GameRoom, room) || !room.ContainsPlayer(player))
                          return;
                      player.CancelBasicAttackWindup();
                      room.CancelPlayerMovement(player);
                  });
    }

    private static void SendMoveRejected(ClientSession session, uint sequence, MOVE_REJECT_REASON reason, Player player,
                                         GameRoom room, string requestedMapId = null, string requestedHash = null)
    {
        Vec3 serverPosition = player?.Info?.Position == null
                                  ? Vec3.Zero
                                  : new Vec3(player.Info.Position.X, player.Info.Position.Y, player.Info.Position.Z);
        session.Send(new S_MoveRejected {
            ClientMoveSequence = sequence,
            RejectReason = reason,
            ServerPosition = serverPosition.ToProtoVector3(),
            NavigationMapId = room?.NavigationMapId ?? requestedMapId ?? string.Empty,
            NavigationContentHash = room?.NavigationContentHash ?? requestedHash ?? string.Empty,
        });
    }

    public static void C_RequestMapHandler(PacketSession session, IMessage message) {}

    private const double SkillCooldownToleranceSeconds = 0.15;
    private const float DefaultSkillCooldownSeconds = 10.0f;

    internal static SKILL_REJECT_REASON CheckCast(Player player, int skillId, out SpellData spell, out int rank,
                                                  out float manaCost)
    {
        bool isSpell = skillId >= (int)SkillType.QSpell && skillId <= (int)SkillType.RSpell;
        spell = isSpell ? CombatMath.DataOf(player)?.Spell((SkillType)skillId) ?? SpellData.Empty : SpellData.Empty;
        rank = isSpell ? Progression.SkillRank(player, (SkillType)skillId) : 0;
        manaCost = spell.ManaCostAt(rank) * Server.Game.Items.ItemEffects.ManaCostMultiplier(player);

        if (player.Info.Hp <= 0)
            return SKILL_REJECT_REASON.SkillRejectDead;

        if (player.Status.IsDisabled || player.Status.IsDashing)
            return SKILL_REJECT_REASON.SkillRejectStunned;

        if (isSpell && rank <= 0)
            return SKILL_REJECT_REASON.SkillRejectNotLearned;

        if (isSpell && player.Status.IsSilenced)
            return SKILL_REJECT_REASON.SkillRejectSilenced;

        if (isSpell && !player.IsSkillReady(skillId, SkillCooldownToleranceSeconds))
            return SKILL_REJECT_REASON.SkillRejectCooldown;

        if (isSpell && player.Progress.Mana + 0.001f < manaCost)
            return SKILL_REJECT_REASON.SkillRejectMana;

        return SKILL_REJECT_REASON.SkillRejectNone;
    }

    public static void C_SkillCastHandler(PacketSession session, IMessage message)
    {
        var skillPacket = message as C_SkillCast;

        if (skillPacket == null)
            return;

        ClientSession clientSession = (ClientSession)session;
        GameRoom room = clientSession.GameRoom;
        Player sessionPlayer = clientSession.Player;

        if (room == null || sessionPlayer == null || sessionPlayer.Info == null)
            return;

        if (skillPacket.CasterId != 0 && skillPacket.CasterId != sessionPlayer.Info.ObjectId)
        {
            Console.WriteLine("[Security] Skill cast rejected: casterId does not belong to the session.");

            return;
        }

        room.Push(() =>
                  {
                      if (!ReferenceEquals(clientSession.Player, sessionPlayer) ||
                          !ReferenceEquals(clientSession.GameRoom, room) ||
                          !ReferenceEquals(sessionPlayer.Room, room) || !room.ContainsPlayer(sessionPlayer))
                      {
                          return;
                      }

                      Cast(room, sessionPlayer, skillPacket);
                  });
    }

    public static bool Cast(GameRoom room, Player caster, C_SkillCast skillPacket)
    {
        IChampionSkillHandler skillHandler = ChampionRegistry.Find(caster.Info.ChampType)?.Handler();

        if (skillHandler == null)
            return false;

        int skillId = skillPacket.SkillId;
        bool isSpell = skillId >= (int)SkillType.QSpell && skillId <= (int)SkillType.RSpell;
        SKILL_REJECT_REASON reason = CheckCast(caster, skillId, out SpellData spell, out int rank, out float manaCost);

        if (reason != SKILL_REJECT_REASON.SkillRejectNone)
        {
            Console.WriteLine($"[Skill] Rejected skill {skillId} from {caster.Info.ObjectId}: {reason}.");
            caster.Session?.Send(new S_SkillRejected { SkillId = skillId, Reason = reason });

            return false;
        }

        // Units the caster's team cannot see cannot be targeted.
        if (skillPacket.TargetId != 0 && room.FindObject(skillPacket.TargetId) is GameObject target &&
            target.Info != null && target.Info.TeamId != caster.Info.TeamId &&
            room.Vision.IsHiddenFrom(caster.Info.TeamId, target.Info.ObjectId))
        {
            Console.WriteLine(
                $"[Skill] Rejected skill {skillId} from {caster.Info.ObjectId}: target {skillPacket.TargetId} is hidden.");
            caster.Session?.Send(
                new S_SkillRejected { SkillId = skillId, Reason = SKILL_REJECT_REASON.SkillRejectInvalid });

            return false;
        }

        bool accepted = skillHandler.HandleSkill(room, caster, skillPacket);

        if (!accepted && isSpell)
        {
            Console.WriteLine(
                $"[Skill] Rejected skill {skillId} from {caster.Info.ObjectId}: invalid target, range or busy " +
                $"(hp {caster.Info.Hp}, acting {caster.IsPerformingCombatAction}, target {skillPacket.TargetId}).");
            caster.Session?.Send(
                new S_SkillRejected { SkillId = skillId, Reason = SKILL_REJECT_REASON.SkillRejectInvalid });
        }

        if (accepted)
            Recall.Cancel(room, caster);

        if (accepted && isSpell)
        {
            Progression.TrySpendMana(caster, manaCost);

            float cooldown = ChampionRegistry.Find(caster.Info.ChampType)?.Cooldown?.Invoke(caster, skillId, rank) ??
                             spell.CooldownAt(rank, DefaultSkillCooldownSeconds);
            // Ability haste: cooldown * 100 / (100 + haste).
            cooldown *= 100f / (100f + Server.Game.Items.ItemEffects.Haste(caster, skillId));
            caster.StartSkillCooldown(skillId, cooldown);
            Server.Game.Items.ItemEffects.OnSpellCast(room, caster, skillId, manaCost);
            caster.Session?.Send(new S_SkillCooldown { SkillId = skillId, Duration = cooldown });
        }

        return accepted;
    }

    public static void C_EnterGameHandler(PacketSession session, IMessage message)
    {
        if (!(message is C_EnterGame enter) || !(session is ClientSession clientSession) ||
            clientSession.GameRoom != null)
            return;
        GameReservation reservation = null;

        if (!string.IsNullOrEmpty(enter.Token))
        {
            reservation = LobbyManager.Instance.Claim(enter.Token);

            if (reservation == null)
            {
                Console.WriteLine($"[Lobby] Session {clientSession.SessionId} brought an unknown or used token.");
                clientSession.Disconnect();

                return;
            }

            clientSession.GameToken = reservation.Token;
        }

        GameRoom room = RoomManager.Instance.Find(reservation?.RoomId ?? 0);

        if (room == null)
        {
            Console.WriteLine($"[Lobby] Session {clientSession.SessionId}: game room {reservation?.RoomId} is gone.");
            clientSession.Disconnect();

            return;
        }

        clientSession.GameRoom = room;
        room.EnterRoom(clientSession, reservation);
    }

    public static void C_LoginHandler(PacketSession session, IMessage message)
    {
        if (!(message is C_Login login) || !(session is ClientSession clientSession) || clientSession.InLobby ||
            clientSession.GameRoom != null)
            return;
        clientSession.InLobby = LobbyManager.Instance.Login((ulong)clientSession.SessionId, login.Name, clientSession);
    }

    public static void C_LobbyActionHandler(PacketSession session, IMessage message)
    {
        if (!(message is C_LobbyAction action) || !(session is ClientSession clientSession) || !clientSession.InLobby)
            return;
        LobbyManager.Instance.Act((ulong)clientSession.SessionId, action.Action, action.Value, action.Text);
    }

    public static void C_ForceEndHandler(PacketSession session, IMessage message)
    {
        if (!(message is C_ForceEnd) ||
            !RoomPlayer(session, out ClientSession clientSession, out GameRoom room, out Player player))
            return;
        room.Push(() =>
                  {
                      if (!ReferenceEquals(clientSession.Player, player) || !room.IsLobbyGame)
                          return;

                      if (!string.Equals(room.HostName, player.Info.Name, StringComparison.OrdinalIgnoreCase))
                      {
                          clientSession.Send(new S_ChatMessage { SenderName = "",
                                                                 Message = "방장만 게임을 끝낼 수 있습니다.",
                                                                 Channel = CHAT_CHANNEL.ChatSystem });

                          return;
                      }

                      room.ForceEnd(player.Info.Name + " 님(방장)이 게임을 종료했습니다.");
                  });
    }
}
