#pragma once
#include "Protocol.pb.h"

inline bool UsesAnnieQEffect(int championType, const Protocol::S_ProjectileSpawn& packet)
{
    return championType == Protocol::PLAYER_TYPE_ANNIE && packet.has_skillid() && packet.skillid() == 1;
}
