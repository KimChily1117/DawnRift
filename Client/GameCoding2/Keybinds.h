#pragma once

namespace Keybinds
{
enum class Action
{
    Stop = 0,
    AttackMove,
    Ward,
    Shop,
    Ping,
    PingRetreat,
    Recall,
    CameraLock,
    Count
};

void Load();
void Save();
KEY_TYPE Key(Action action);
void SetKey(Action action, KEY_TYPE key);
bool Down(Action action);
bool Held(Action action);

bool QuickCast();
void SetQuickCast(bool on);
const char* LabelKey(Action action);
string KeyName(KEY_TYPE key);
void ResetDefaults();
}
