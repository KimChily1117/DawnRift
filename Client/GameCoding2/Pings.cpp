#include "pch.h"
#include "Pings.h"
#include "LolVfx.h"
#include "UiStrings.h"
#include "ClientPacketHandler.h"
#include "BasePlayerController.h"
#include "Texture.h"
#include "ChampionKits.h"

namespace
{
vector<Pings::Active> g_active;
Pings::Wheel g_wheel;
uint64 g_lastVfx = 0;
size_t g_received = 0;
const wstring IconRoot = L"..\\Resources\\Extracted\\Items\\assets\\shared\\particles\\";

struct Kind
{
    Protocol::PING_TYPE type;
    const char* system;
    const wchar_t* icon;
    const char* label;
};

const Kind Kinds[] = {
    {Protocol::PING_GENERIC, "SR_Pings_Blue", L"pingupdate\\need\\generic_ping.dds", "ping.generic"},
    {Protocol::PING_RETREAT, "SR_Pings_Danger", L"ping-danger.dds", "ping.retreat"},
    {Protocol::PING_ON_MY_WAY, "SR_Pings_OMW", L"pingupdate\\need\\omw.dds", "ping.on_my_way"},
    {Protocol::PING_ASSIST, "SR_Pings_Assist", L"pingupdate\\need\\assist_me.dds", "ping.assist"},
    {Protocol::PING_MISSING, "SR_Pings_Missing", L"pingupdate\\need\\enemy_missing.dds", "ping.missing"},
    {Protocol::PING_PUSH, "SR_Pings_Push", L"pingupdate\\need\\push.dds", "ping.push"},
    {Protocol::PING_HOLD, "SR_Pings_Hold", L"pingupdate\\need\\hold_ig.dds", "ping.hold"},
    {Protocol::PING_ALL_IN, "SR_Pings_All_In", L"pingupdate\\need\\all_in.dds", "ping.all_in"},
    {Protocol::PING_NEED_VISION, "SR_Pings_NeedVision", L"pingupdate\\need\\need_vision.dds", "ping.need_vision"},
};

const Kind& KindOf(Protocol::PING_TYPE type)
{
    for (const Kind& kind : Kinds)
        if (kind.type == type)
            return kind;

    return Kinds[0];
}

string NameOf(uint64 objectId)
{
    auto me = GAMEMANAGER->_myPlayer;

    if (me && me->_playerInfo && me->_playerInfo->objectid() == objectId)
        return me->_playerInfo->name();

    if (auto object = CUR_SCENE ? CUR_SCENE->FindObjectById(objectId) : nullptr)
        if (auto controller = object->GetScript<BasePlayerController>(); controller && controller->_playerInfo)
            return controller->_playerInfo->name();

    return "";
}
}

namespace Pings
{
Protocol::PING_TYPE SectorType(int32 sector)
{
    static const Protocol::PING_TYPE Order[WheelSectors] = {
        Protocol::PING_RETREAT, Protocol::PING_PUSH, Protocol::PING_ON_MY_WAY, Protocol::PING_ALL_IN,
        Protocol::PING_ASSIST,  Protocol::PING_HOLD, Protocol::PING_MISSING,   Protocol::PING_NEED_VISION,
    };

    return sector >= 0 && sector < WheelSectors ? Order[sector] : Protocol::PING_GENERIC;
}

void Send(Protocol::PING_TYPE type, const Vec3& position, uint64 targetId)
{
    Protocol::C_Ping packet;
    packet.set_type(type);
    packet.mutable_position()->set_x(position.x);
    packet.mutable_position()->set_y(position.y);
    packet.mutable_position()->set_z(position.z);
    packet.set_targetid(targetId);
    NETWORK->SendPacket(ClientPacketHandler::MakeSendBuffer(packet, C_PING));
}

void OnPacket(const Protocol::S_Ping& packet)
{
    ++g_received;
    Active ping;
    ping.type = packet.type();
    ping.position = Vec3(packet.position().x(), packet.position().y(), packet.position().z());
    ping.sourceId = packet.sourceid();
    ping.targetId = packet.targetid();
    ping.sourceName = NameOf(packet.sourceid());
    ping.at = TIME->GetGameTime();

    g_active.erase(std::remove_if(g_active.begin(), g_active.end(),
                                  [&](const Active& other) {
                                      return other.sourceId == ping.sourceId &&
                                             Vec3::Distance(other.position, ping.position) < 1.f;
                                  }),
                   g_active.end());
    g_active.push_back(ping);

    if (g_active.size() > 8)
        g_active.erase(g_active.begin());
    LolVfx::PlayOptions options;
    options.duration = ShowSeconds;
    g_lastVfx = LolVfx::Get()->Play(KindOf(ping.type).system, ping.position, Vec3(0.f, 0.f, 1.f), options);
    auto me = GAMEMANAGER->_myPlayer;
    const bool mine = me && me->_playerInfo && me->_playerInfo->objectid() == packet.sourceid();

    static const char* const Sounds[] = {"Click",  "Danger", "OnMyWay", "Defend", "MIA",
                                         "Attack", "Defend", "Attack",  "Click"};
    const int32 kind = std::clamp(static_cast<int32>(ping.type), 0, static_cast<int32>(std::size(Sounds)) - 1);
    const string sound = string("Play_sfx_hud_Pings_") + Sounds[kind];

    if (SOUND->HasCue(sound))
        SOUND->PlayCue(sound);
    else
        SOUND->PlayCue(mine ? "Play_sfx_hud_Generic_Button_Click" : "Play_sfx_hud_Menu_Open");

    if (mine)
        if (const ChampionKit* kit = FindKit(me->_playerInfo->champtype()))
        {
            static const char* const Lines[] = {"",     "Retreat", "OnMyWay", "AssistMe",  "EnemyMissing",
                                                "Push", "Hold",    "AllIn",   "NeedVision"};
            const int32 type = static_cast<int32>(ping.type);

            if (type > 0 && type < static_cast<int32>(std::size(Lines)))
            {
                char skin[8];
                snprintf(skin, sizeof(skin), "%02d", me->_playerInfo->skinid());
                const string cue = string("Play_vo_") + kit->name +
                                   (me->_playerInfo->skinid() > 0 ? string("Skin") + skin : "") + "_Ping2D" +
                                   Lines[type];

                if (SOUND->HasCue(cue))
                    SOUND->PlayCue(cue);
            }
        }
}

void Update()
{
    const float now = TIME->GetGameTime();
    g_active.erase(
        std::remove_if(g_active.begin(), g_active.end(), [&](const Active& p) { return now - p.at > ShowSeconds; }),
        g_active.end());
}

const vector<Active>& Current()
{
    return g_active;
}

Wheel& GetWheel()
{
    return g_wheel;
}

const char* VfxSystem(Protocol::PING_TYPE type)
{
    return KindOf(type).system;
}

shared_ptr<Texture> Icon(Protocol::PING_TYPE type)
{
    const Kind& kind = KindOf(type);
    const wstring file = IconRoot + kind.icon;

    if (!std::filesystem::exists(file))
        return nullptr;

    return RESOURCES->Load<Texture>(L"PingIcon_" + std::to_wstring(static_cast<int32>(type)), file);
}

string Label(Protocol::PING_TYPE type)
{
    return UiStrings::Get(KindOf(type).label);
}

uint64 LastVfxHandle()
{
    return g_lastVfx;
}

size_t Received()
{
    return g_received;
}

void Clear()
{
    g_active.clear();
    g_wheel = Wheel();
}
}
