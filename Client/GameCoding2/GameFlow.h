#pragma once

namespace GameFlow
{
// ---- recall (B) ----
void SendRecall();

// ---- surrender ----
struct SurrenderState
{
    bool active = false;
    int32 yes = 0, no = 0, needed = 0;
    float endsAt = 0.f;
    bool voted = false;
    int32 lastState = -1;
    float messageUntil = 0.f;
    string message;
};

const SurrenderState& Surrender();
void SendSurrender(bool agree);
void OnSurrender(const Protocol::S_Surrender& packet);

// ---- death recap ----
struct RecapEntry
{
    string name;
    int32 sourceType = 0;
    int32 skillId = 0;
    int32 damage = 0;
    int32 damageType = 0;
};

struct Recap
{
    bool shown = false;
    uint64 killerId = 0;
    int32 total = 0;
    float seconds = 0.f;
    vector<RecapEntry> entries;
};

const Recap& DeathRecap();
void OnDeathRecap(const Protocol::S_DeathRecap& packet);
void HideDeathRecap();

// ---- match summary ----
struct SummaryEntry
{
    uint64 objectId = 0;
    string name;
    int32 champion = 0, team = 0, level = 0, kills = 0, deaths = 0, assists = 0, cs = 0;
    int32 gold = 0, damageToChampions = 0, damageTaken = 0;
    vector<int32> items;
};

struct Summary
{
    bool shown = false;
    int32 winningTeam = 0;
    float gameTime = 0.f;
    bool surrendered = false;
    vector<SummaryEntry> entries;
    string savedTo;
};

const Summary& MatchSummary();
void OnMatchSummary(const Protocol::S_MatchSummary& packet);

string SaveMatch(const Summary& summary);

// ---- practice tool ----
bool PracticeEnabled();
void SetPracticeEnabled(bool on);
void SendPractice(Protocol::PRACTICE_ACTION action, const Vec3& position);

void SetVersionWarning(const string& text);
const string& VersionWarning();

struct RoomState
{
    string host, roomName;
    bool paused = false;
    float secondsLeft = 0.f;

    double receivedAt = 0.0;
    vector<string> waitingFor;
    string closedReason;

    double closedAt = 0.0;
};

const RoomState& Room();
void OnGamePause(const Protocol::S_GamePause& packet);
void OnRoomInfo(const Protocol::S_RoomInfo& packet);
void OnRoomClosed(const Protocol::S_RoomClosed& packet);
float PauseSecondsLeft();
bool IAmHost();
void SendForceEnd();

double RealSeconds();

void Clear();
}
