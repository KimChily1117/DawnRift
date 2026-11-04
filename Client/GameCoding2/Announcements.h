#pragma once

namespace Announcements
{
struct Line
{
    string cue;
    string text;
    bool good = true;
    int32 sourceType = 0;
    int32 targetType = 0;
};

// The line as the player with myId on myTeam hears it.
Line Resolve(const Protocol::S_Announcement& packet, uint64 myId, int32 myTeam);
void OnPacket(const Protocol::S_Announcement& packet);
void OnGameOver(bool won);

string Text(const string& id, const string& src = "", const string& dst = "");
}
