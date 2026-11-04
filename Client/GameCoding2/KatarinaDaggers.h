#pragma once

namespace KatarinaDaggers
{
void OnPacket(const Protocol::S_KatarinaDagger& packet);

void Update();
// Daggers thrown and not yet picked up or expired (tests).
size_t Count();

bool Nearest(const Vec3& from, Vec3& position);
}
