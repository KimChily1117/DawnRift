#pragma once

class Texture;

namespace Pings
{
struct Active
{
    Protocol::PING_TYPE type = Protocol::PING_GENERIC;
    Vec3 position;
    uint64 sourceId = 0, targetId = 0;
    string sourceName;
    float at = 0.f;
};

constexpr float ShowSeconds = 3.5f;

struct Wheel
{
    bool open = false;
    Vec2 center;
    int32 hovered = -1;
};

constexpr int32 WheelSectors = 8;

Protocol::PING_TYPE SectorType(int32 sector);

void Send(Protocol::PING_TYPE type, const Vec3& position, uint64 targetId);
void OnPacket(const Protocol::S_Ping& packet);
void Update();
const vector<Active>& Current();
Wheel& GetWheel();

const char* VfxSystem(Protocol::PING_TYPE type);
shared_ptr<Texture> Icon(Protocol::PING_TYPE type);
string Label(Protocol::PING_TYPE type);

uint64 LastVfxHandle();
size_t Received();
void Clear();
}
