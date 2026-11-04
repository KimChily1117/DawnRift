// 서버 패킷 처리.
#pragma once

enum Proto
{
    C_TEST_MSG = 0,
    S_TEST_MSG = 1,
    C_MOVE = 2,
    S_OBJECT_UPDATE = 3,
    C_SKILL_CAST = 4,
    S_SKILL_RESULT = 5,
    C_REQUEST_MAP = 6,
    S_UPDATE_MAP = 7,
    C_CHAT_MESSAGE = 8,
    S_CHAT_MESSAGE = 9,
    S_ENTER_GAME = 10,
    S_MY_PLAYER = 11,
    S_ADD_OBJECT = 12,
    S_REMOVE_OBJECT = 13,
    S_MOVE = 14,
    C_ENTER_GAME = 15,
    S_PROJECTILE_SPAWN = 16,
    S_PROJECTILE_HIT = 17,
    S_DAMAGE = 18,
    S_DEAD = 19,
    S_NAVIGATION_INFO = 20,
    C_MOVE_REQUEST = 21,
    S_MOVE_ACCEPTED = 22,
    S_MOVE_REJECTED = 23,
    S_MOVEMENT_SNAPSHOT = 24,
    C_STOP_COMMAND = 25,
    S_BUFF = 26,
    S_HP_UPDATE = 27,
    S_UNIT_STATS = 28,
    S_RESPAWN = 29,
    S_SKILL_COOLDOWN = 30,
    S_SKILL_REJECTED = 31,
    S_VISION = 32,
    S_GAME_OVER = 38,
    C_SUMMONER_SPELL = 39,
    S_SUMMONER_SPELL = 40,
    S_SUMMONER_COOLDOWN = 41,
    S_SCOREBOARD = 42,
    C_PLACE_WARD = 43,
    S_TRINKET = 44,
    S_ANNOUNCEMENT = 45,
    C_LEVEL_UP_SKILL = 46,
    S_KATARINA_DAGGER = 47,
    C_ITEM_ACTION = 48,
    S_INVENTORY = 49,
    S_ITEM_EFFECT = 50,
    C_PING = 51,
    S_PING = 52,
    C_RECALL = 53,
    S_RECALL = 54,
    C_SURRENDER = 55,
    S_SURRENDER = 56,
    S_DEATH_RECAP = 57,
    S_MATCH_SUMMARY = 58,
    C_PRACTICE = 59,
    S_ROOM_LIST = 60,
    S_GAME_PAUSE = 61,
    C_FORCE_END = 62,
    S_ROOM_INFO = 63,
    S_ROOM_CLOSED = 64
};

constexpr int32 kProtocolVersion = 5;

// �ڵ� �ڵ�ȭ�� �ϸ� �����ٵ�. �װ� ��Ƴ׿�.
class Model;

wstring ChampionSkinFolder(int32 champType, int32 skinId);
shared_ptr<Model> LoadChampionModel(const wstring& folder);

class ClientPacketHandler
{
public:
    static void HandlePacket(ServerSessionRef session, BYTE* buffer, int32 len);

    static void SendEnterGame(ServerSessionRef session);

    // �ޱ�
    static void Handle_S_TEST(BYTE* buffer, int32 len);
    static void Handle_S_EnterGame(BYTE* buffer, int32 len);
    static void Handle_S_MyPlayer(BYTE* buffer, int32 len);
    static void Handle_S_AddObject(BYTE* buffer, int32 len);

    static void Handle_S_GamePause(BYTE* buffer, int32 len);
    static void Handle_S_RoomInfo(BYTE* buffer, int32 len);
    static void Handle_S_RoomClosed(BYTE* buffer, int32 len);
    static void Handle_S_RemoveObject(BYTE* buffer, int32 len);
    static void Handle_S_Move(BYTE* buffer, int32 len);
    static void Handle_S_NavigationInfo(BYTE* buffer, int32 len);
    static void Handle_S_MoveAccepted(BYTE* buffer, int32 len);
    static void Handle_S_MoveRejected(BYTE* buffer, int32 len);
    static void Handle_S_MovementSnapshot(BYTE* buffer, int32 len);
    static void Handle_S_SkillResult(BYTE* buffer, int32 len);
    static void Handle_S_ProjectileSpawn(BYTE* buffer, int32 len);
    static void Handle_S_ProjectileHit(BYTE* buffer, int32 len);
    static void Handle_S_Damage(BYTE* buffer, int32 len);
    static void Handle_S_Buff(BYTE* buffer, int32 len);
    static void Handle_S_HpUpdate(BYTE* buffer, int32 len);
    static void Handle_S_Dead(BYTE* buffer, int32 len);
    static void Handle_S_UnitStats(BYTE* buffer, int32 len);
    static void Handle_S_Respawn(BYTE* buffer, int32 len);
    static void Handle_S_SkillCooldown(BYTE* buffer, int32 len);
    static void Handle_S_SkillRejected(BYTE* buffer, int32 len);
    static void Handle_S_Vision(BYTE* buffer, int32 len);
    static void Handle_S_GameOver(BYTE* buffer, int32 len);
    static void Handle_S_SummonerSpell(BYTE* buffer, int32 len);
    static void Handle_S_SummonerCooldown(BYTE* buffer, int32 len);
    static void Handle_S_Scoreboard(BYTE* buffer, int32 len);
    static void Handle_S_Trinket(BYTE* buffer, int32 len);
    static void Handle_S_Announcement(BYTE* buffer, int32 len);
    static void Handle_S_KatarinaDagger(BYTE* buffer, int32 len);
    static void Handle_S_Inventory(BYTE* buffer, int32 len);
    static void Handle_S_ItemEffect(BYTE* buffer, int32 len);
    static void Handle_S_Ping(BYTE* buffer, int32 len);
    static void Handle_S_ChatMessage(BYTE* buffer, int32 len);
    static void Handle_S_Recall(BYTE* buffer, int32 len);
    static void Handle_S_Surrender(BYTE* buffer, int32 len);
    static void Handle_S_DeathRecap(BYTE* buffer, int32 len);
    static void Handle_S_MatchSummary(BYTE* buffer, int32 len);

    static std::unordered_map<uint64, int32> g_lastPlayedSkill;

    template <typename T> static SendBufferRef MakeSendBuffer(T& pkt, uint16 pktId)
    {
        const uint16 dataSize = static_cast<uint16>(pkt.ByteSizeLong());
        const uint16 packetSize = dataSize + sizeof(PacketHeader);

        SendBufferRef sendBuffer = make_shared<SendBuffer>(packetSize);
        PacketHeader* header = reinterpret_cast<PacketHeader*>(sendBuffer->Buffer());
        header->size = packetSize;
        header->id = pktId;
        const bool serialized = pkt.SerializeToArray(&header[1], dataSize);
        assert(serialized);
        (void)serialized;
        sendBuffer->Close(packetSize);

        return sendBuffer;
    }
};
