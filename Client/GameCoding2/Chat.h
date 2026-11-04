#pragma once

namespace Chat
{
struct Line
{
    Protocol::CHAT_CHANNEL channel = Protocol::CHAT_TEAM;
    string sender;
    string text;
    int32 team = 0;
    float at = 0.f;
};

constexpr float ShowSeconds = 10.f;
constexpr size_t MaxLines = 60;

void Open(Protocol::CHAT_CHANNEL channel);
void Close();
bool IsOpen();
Protocol::CHAT_CHANNEL Channel();

void Update();
void Submit();
void OnPacket(const Protocol::S_ChatMessage& packet);
void AddSystem(const string& text);
const std::deque<Line>& Lines();

string Input();
string Composition();
// Tests: type as if from the keyboard.
void TypeForTests(const string& utf8);
size_t Sent();
void Clear();
}
