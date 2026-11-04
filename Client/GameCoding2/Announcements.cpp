#include "pch.h"
#include "Announcements.h"
#include "Chat.h"
#include "BasePlayerController.h"
#include "HUDController.h"
#include "GameSounds.h"
#include <fstream>

namespace
{
const wstring LinesFile = L"..\\Resources\\Data\\UI\\Announcements.ko.json";
const string Voice = "Play_vo_Announcer_Female1_";

struct Data
{
    unordered_map<string, string> lines;
    unordered_map<string, string> champions;
};

const Data& Loaded()
{
    static Data data;
    static bool loaded = false;

    if (!loaded)
    {
        loaded = true;
        std::ifstream file(LinesFile);
        json j = json::parse(file, nullptr, false);

        if (j.is_discarded())
        {
            DEBUG_LOG("[Announcer] missing Resources/Data/UI/Announcements.ko.json");
        }
        else
        {
            for (const auto& [key, value] : j["lines"].items())
                data.lines[key] = value.get<string>();

            for (const auto& [key, value] : j["champions"].items())
                data.champions[key] = value.get<string>();
        }
    }

    return data;
}

void ReplaceAll(string& text, const string& from, const string& to)
{
    for (size_t at = text.find(from); at != string::npos; at = text.find(from, at + to.size()))
        text.replace(at, from.size(), to);
}

int32 TypeOf(uint64 objectId)
{
    if (objectId == 0 || !CUR_SCENE)
        return 0;
    auto object = CUR_SCENE->FindObjectById(objectId);
    auto controller = object ? object->GetScript<BasePlayerController>() : nullptr;

    return controller && controller->_playerInfo ? controller->_playerInfo->champtype() : 0;
}

string NameOf(int32 champType)
{
    const string event = GameSounds::ChampionName(champType);
    const auto& names = Loaded().champions;
    auto it = names.find(event);

    return it != names.end() ? it->second : event;
}

// Champions only: portraits exist for them.
int32 Portrait(int32 type)
{
    return GameSounds::ChampionName(type).empty() ? 0 : type;
}
}

namespace Announcements
{
string Text(const string& id, const string& src, const string& dst)
{
    const auto& lines = Loaded().lines;
    auto it = lines.find(id);

    if (it == lines.end())
        return "";
    string text = it->second;
    ReplaceAll(text, "{src}", src);
    ReplaceAll(text, "{dst}", dst);

    return text;
}

Line Resolve(const Protocol::S_Announcement& packet, uint64 myId, int32 myTeam)
{
    Line line;
    const bool mine = packet.team() == myTeam;
    const bool youDid = myId != 0 && packet.sourceid() == myId;
    const bool youDied = myId != 0 && packet.targetid() == myId;
    const string team = mine ? "YourTeam" : "EnemyTeam";
    const int32 srcType = TypeOf(packet.sourceid()), dstType = TypeOf(packet.targetid());
    const string src = NameOf(srcType), dst = NameOf(dstType);
    line.sourceType = Portrait(srcType);
    line.targetType = Portrait(dstType);
    line.good = mine;

    const string who = youDid ? "YouYourTeam" : "Hero" + team;

    switch (packet.kind())
    {
    case Protocol::ANNOUNCE_WELCOME:
        line = {Voice + "StartGameMessage1Map11", Text("welcome")};
        break;
    case Protocol::ANNOUNCE_MINIONS_SOON:
        line = {Voice + "StartGameMessage2Map11", Text("minions_soon")};
        break;
    case Protocol::ANNOUNCE_MINIONS_SPAWNED:
        line = {Voice + "MinionSpawn", Text("minions")};
        break;
    case Protocol::ANNOUNCE_FIRST_BLOOD:
        line.cue = Voice + "FirstBlood" + who;
        line.text = Text("first_blood");
        break;
    case Protocol::ANNOUNCE_CHAMPION_KILL:
        if (youDid)
            line.cue = Voice + "KillChampionYouHeroYourTeam", line.text = Text("kill_you");
        else if (youDied)
            line.cue = Voice + "KillChampionHeroYouEnemyTeam", line.text = Text("killed_you");
        else
            line.cue = Voice + "KillChampionHeroHero" + team, line.text = Text("kill", src, dst);
        break;
    case Protocol::ANNOUNCE_MULTIKILL:
    {
        static const char* const Names[] = {"", "", "DoubleKill", "TripleKill", "QuadraKill", "PentaKill"};
        const int32 n = std::clamp(packet.count(), 2, 5);
        line.cue = Voice + Names[n] + who;
        line.text = Text("multi" + std::to_string(n), src, dst);
        break;
    }
    case Protocol::ANNOUNCE_KILLING_SPREE:
    {
        static const char* const Names[] = {"KillingSpree", "Rampage", "Unstoppable",
                                            "Dominating",   "Godlike", "Legendary"};
        const int32 n = std::clamp(packet.count(), 3, 8);
        line.cue = Voice + Names[n - 3] + who;
        line.text = Text("spree" + std::to_string(n), src, dst);
        break;
    }
    case Protocol::ANNOUNCE_SHUTDOWN:
        line.cue = Voice + "Shutdown" + who;
        line.text = Text("shutdown", src, dst);
        break;
    case Protocol::ANNOUNCE_EXECUTED:
    {
        const string by = srcType == Protocol::PLAYER_TYPE_TURRET ? "Turret" : "Minion";
        line.good = !mine;
        line.cue = Voice + "KillChampion" + by + (youDied ? "YouEnemyTeam" : mine ? "HeroEnemyTeam" : "HeroYourTeam");
        line.text = Text(by == "Turret" ? "executed_turret" : "executed_minion", src, dst);
        line.sourceType = 0;
        break;
    }
    case Protocol::ANNOUNCE_ACE:
        line.cue = Voice + "Ace" + team;
        line.text = Text(mine ? "ace_ally" : "ace_enemy");
        line.sourceType = line.targetType = 0;
        break;
    case Protocol::ANNOUNCE_TURRET_DESTROYED:
        line.good = !mine;
        line.cue = Voice + (packet.count() == 1 ? "TurretLostFirstBlood" : "TurretLost") + team;
        line.text = Text(mine ? "turret_lost_ally" : "turret_lost_enemy");
        line.sourceType = line.targetType = 0;
        break;
    case Protocol::ANNOUNCE_INHIBITOR_DESTROYED:
        line.good = !mine;
        line.cue = Voice + "InhibitorLost" + team;
        line.text = Text(mine ? "inhibitor_lost_ally" : "inhibitor_lost_enemy");
        line.sourceType = line.targetType = 0;
        break;
    case Protocol::ANNOUNCE_INHIBITOR_RESPAWN_SOON:
        line.cue = Voice + "InhibitorRespawnSoon" + team;
        line.text = Text(mine ? "inhibitor_soon_ally" : "inhibitor_soon_enemy");
        line.sourceType = line.targetType = 0;
        break;
    case Protocol::ANNOUNCE_INHIBITOR_RESPAWNED:
        line.cue = Voice + "InhibitorRespawn" + team;
        line.text = Text(mine ? "inhibitor_back_ally" : "inhibitor_back_enemy");
        line.sourceType = line.targetType = 0;
        break;
    case Protocol::ANNOUNCE_DRAGON_KILLED:
    case Protocol::ANNOUNCE_BARON_KILLED:
    {
        const bool dragon = packet.kind() == Protocol::ANNOUNCE_DRAGON_KILLED;
        line.cue = Voice + (dragon ? "KillDragon" : "KillWorm") + (packet.team() == 2 ? "Red" : "Blue");
        line.text.clear();
        line.sourceType = line.targetType = 0;
        break;
    }
    default:
        line = {};
        break;
    }

    return line;
}

void OnPacket(const Protocol::S_Announcement& packet)
{
    auto me = GAMEMANAGER->_myPlayer;
    const uint64 myId = me && me->_playerInfo ? me->_playerInfo->objectid() : 0;
    const int32 myTeam = me && me->_playerInfo ? me->_playerInfo->teamid() : 1;
    Line line = Resolve(packet, myId, myTeam);

    if (!line.cue.empty())
        SOUND->QueueAnnouncer(line.cue);

    if (!line.text.empty())
        if (auto hud = HUDController::Find())
        {
            hud->ShowAnnouncement(line.text, line.good, line.sourceType, line.targetType);
            Chat::AddSystem(line.text);
        }
    DEBUG_LOG("[Announcer] " << Protocol::ANNOUNCEMENT_Name(packet.kind()).c_str() << " team " << packet.team()
                             << " -> " << line.cue.c_str());
}

void OnGameOver(bool won)
{
    SOUND->QueueAnnouncer(won ? "Play_vo_Announcer_Global_Female1_OnVictory"
                              : "Play_vo_Announcer_Global_Female1_OnDefeat");
}
}
