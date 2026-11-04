#include "pch.h"
#include "GameFlow.h"
#include "ClientPacketHandler.h"
#include "UiStrings.h"
#include "ChampionKits.h"
#include <fstream>
#include <ctime>
#include <chrono>

namespace
{
GameFlow::SurrenderState g_surrender;
GameFlow::Recap g_recap;
GameFlow::Summary g_summary;
bool g_practice = false;
string g_versionWarning;
GameFlow::RoomState g_room;
}

namespace GameFlow
{
void SendRecall()
{
    Protocol::C_Recall packet;
    NETWORK->SendPacket(ClientPacketHandler::MakeSendBuffer(packet, C_RECALL));
}

const SurrenderState& Surrender()
{
    return g_surrender;
}

void SendSurrender(bool agree)
{
    Protocol::C_Surrender packet;
    packet.set_agree(agree);
    NETWORK->SendPacket(ClientPacketHandler::MakeSendBuffer(packet, C_SURRENDER));
}

void OnSurrender(const Protocol::S_Surrender& packet)
{
    const float now = TIME->GetGameTime();
    auto me = GAMEMANAGER->_myPlayer;
    const uint64 myId = me && me->_playerInfo ? me->_playerInfo->objectid() : 0;
    g_surrender.lastState = packet.state();
    g_surrender.yes = packet.yes();
    g_surrender.no = packet.no();
    g_surrender.needed = packet.needed();

    switch (packet.state())
    {
    case 0:
        if (!g_surrender.active)
            g_surrender.voted = false;
        g_surrender.active = true;
        g_surrender.endsAt = now + packet.secondsleft();

        if (packet.voterid() == myId)
            g_surrender.voted = true;
        break;
    case 1:
        g_surrender.active = false;
        g_surrender.message = UiStrings::Get("surrender.passed");
        g_surrender.messageUntil = now + 6.f;
        break;
    case 2:
        g_surrender.active = false;
        g_surrender.message =
            UiStrings::Format("surrender.failed", {std::to_string(packet.yes()), std::to_string(packet.needed())});
        g_surrender.messageUntil = now + 5.f;
        break;
    default:
        g_surrender.message =
            packet.secondsleft() > 0.f
                ? UiStrings::Format("surrender.too_early",
                                    {std::to_string(static_cast<int32>(std::ceil(packet.secondsleft() / 60.f)))})
                : UiStrings::Get("surrender.cooldown");
        g_surrender.messageUntil = now + 4.f;
        break;
    }
}

const Recap& DeathRecap()
{
    return g_recap;
}

void OnDeathRecap(const Protocol::S_DeathRecap& packet)
{
    g_recap = Recap();
    g_recap.shown = true;
    g_recap.killerId = packet.killerid();
    g_recap.total = packet.total();
    g_recap.seconds = packet.seconds();

    for (int32 i = 0; i < packet.sources_size() && i < 8; ++i)
    {
        const auto& s = packet.sources(i);
        g_recap.entries.push_back({s.sourcename(), s.sourcetype(), s.skillid(), s.damage(), s.damagetype()});
    }
}

void HideDeathRecap()
{
    g_recap.shown = false;
}

const Summary& MatchSummary()
{
    return g_summary;
}

void OnMatchSummary(const Protocol::S_MatchSummary& packet)
{
    g_summary = Summary();
    g_summary.shown = true;
    g_summary.winningTeam = packet.winningteam();
    g_summary.gameTime = packet.gametime();
    g_summary.surrendered = packet.surrendered();

    for (int32 i = 0; i < packet.entries_size(); ++i)
    {
        const auto& e = packet.entries(i);
        SummaryEntry entry;
        entry.objectId = e.objectid();
        entry.name = e.name();
        entry.champion = e.champion();
        entry.team = e.team();
        entry.level = e.level();
        entry.kills = e.kills();
        entry.deaths = e.deaths();
        entry.assists = e.assists();
        entry.cs = e.cs();
        entry.gold = e.goldearned();
        entry.damageToChampions = e.damagetochampions();
        entry.damageTaken = e.damagetaken();

        for (int32 k = 0; k < e.items_size(); ++k)
            entry.items.push_back(e.items(k));
        g_summary.entries.push_back(entry);
    }

    g_summary.savedTo = SaveMatch(g_summary);
}

string SaveMatch(const Summary& summary)
{
    auto me = GAMEMANAGER->_myPlayer;
    json j;
    j["winningTeam"] = summary.winningTeam;
    j["gameTime"] = summary.gameTime;
    j["surrendered"] = summary.surrendered;
    j["myTeam"] = me && me->_playerInfo ? me->_playerInfo->teamid() : 0;
    j["myName"] = me && me->_playerInfo ? me->_playerInfo->name() : "";
    const std::time_t now = std::time(nullptr);
    char stamp[32];
    std::tm local{};
    localtime_s(&local, &now);
    std::strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", &local);
    j["playedAt"] = stamp;

    for (const SummaryEntry& e : summary.entries)
    {
        const ChampionKit* kit = FindKit(e.champion);
        j["entries"].push_back({{"name", e.name},
                                {"champion", kit ? kit->name : std::to_string(e.champion)},
                                {"team", e.team},
                                {"level", e.level},
                                {"kills", e.kills},
                                {"deaths", e.deaths},
                                {"assists", e.assists},
                                {"cs", e.cs},
                                {"gold", e.gold},
                                {"damageToChampions", e.damageToChampions},
                                {"damageTaken", e.damageTaken},
                                {"items", e.items}});
    }

    char file[48];
    std::strftime(file, sizeof(file), "%Y%m%d-%H%M%S.json", &local);
    const std::filesystem::path dir = L"..\\Saved\\Matches";
    std::error_code error;
    std::filesystem::create_directories(dir, error);
    const std::filesystem::path path = dir / file;
    std::ofstream out(path);

    if (!out)
        return "";
    out << j.dump(1);

    return path.string();
}

bool PracticeEnabled()
{
    return g_practice;
}

void SetPracticeEnabled(bool on)
{
    g_practice = on;
}

void SendPractice(Protocol::PRACTICE_ACTION action, const Vec3& position)
{
    Protocol::C_Practice packet;
    packet.set_action(action);
    packet.mutable_position()->set_x(position.x);
    packet.mutable_position()->set_y(position.y);
    packet.mutable_position()->set_z(position.z);
    NETWORK->SendPacket(ClientPacketHandler::MakeSendBuffer(packet, C_PRACTICE));
}

void SetVersionWarning(const string& text)
{
    g_versionWarning = text;
}

const string& VersionWarning()
{
    return g_versionWarning;
}

double RealSeconds()
{
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

const RoomState& Room()
{
    return g_room;
}

void OnGamePause(const Protocol::S_GamePause& packet)
{
    g_room.paused = packet.paused();
    g_room.secondsLeft = packet.secondsleft();
    g_room.receivedAt = RealSeconds();
    g_room.waitingFor.assign(packet.waitingfor().begin(), packet.waitingfor().end());
    TIME->SetFrozen(g_room.paused);
}

void OnRoomInfo(const Protocol::S_RoomInfo& packet)
{
    g_room.host = packet.hostname();
    g_room.roomName = packet.roomname();
}

void OnRoomClosed(const Protocol::S_RoomClosed& packet)
{
    g_room.closedReason = packet.reason();
    g_room.closedAt = RealSeconds();
    g_room.paused = false;
    TIME->SetFrozen(false);
}

float PauseSecondsLeft()
{
    return static_cast<float>(max(0.0, g_room.secondsLeft - (RealSeconds() - g_room.receivedAt)));
}

bool IAmHost()
{
    auto me = GAMEMANAGER->_myPlayer;

    return !g_room.host.empty() && me && me->_playerInfo && me->_playerInfo->name() == g_room.host;
}

void SendForceEnd()
{
    Protocol::C_ForceEnd packet;
    NETWORK->SendPacket(ClientPacketHandler::MakeSendBuffer(packet, C_FORCE_END));
}

void Clear()
{
    g_room = RoomState();
    TIME->SetFrozen(false);
    g_surrender = SurrenderState();
    g_recap = Recap();
    g_summary = Summary();
    g_practice = false;
}
}
