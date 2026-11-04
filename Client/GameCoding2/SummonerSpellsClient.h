#pragma once

class PlayerController;

namespace SummonerSpellsClient
{
struct Info
{
    Protocol::SUMMONER_SPELL spell;
    const char* name;
    bool targeted;
    float castRange = 0.f;
    float indicatorRange = 0.f;
};

const Info* Find(Protocol::SUMMONER_SPELL spell);

wstring IconFile(Protocol::SUMMONER_SPELL spell);
void PreloadVfx();

bool Cast(PlayerController& me, int32 slot, int32 mouseX, int32 mouseY);

bool CastAt(PlayerController& me, int32 slot, const shared_ptr<GameObject>& picked, Vec3 point);

shared_ptr<GameObject> TargetFor(PlayerController& me, Protocol::SUMMONER_SPELL spell,
                                 const shared_ptr<GameObject>& picked, const Vec3& point);

void OnCast(const Protocol::S_SummonerSpell& packet);
void OnCooldown(const Protocol::S_SummonerCooldown& packet);

constexpr float CursorForgiveness = 2.f;
}
