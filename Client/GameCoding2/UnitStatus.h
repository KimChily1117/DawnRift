#pragma once

namespace UnitStatus
{
struct State
{
    int32 shield = 0;
    float stunEndsAt = 0.f, stunDuration = 0.f;
    float silenceEndsAt = 0.f, silenceDuration = 0.f;
    bool stunReady = false;
    int32 pyromania = 0;
    unordered_map<int32, uint64> vfx;
    int32 level = 1;
    int32 mp = 0, maxMp = 0;
    float respawnAt = 0.f;
    int32 dragons[4] = {};
    int32 kills = 0, deaths = 0, assists = 0, cs = 0;
    float baronEndsAt = 0.f;
    float charmEndsAt = 0.f;
    float slowEndsAt = 0.f;
    float attackSpeedBonus = 0.f, attackSpeedEndsAt = 0.f;
    int32 asheFocus = 0;
    float asheQEndsAt = 0.f;
    int32 ahriDashes = 0;
    float ahriDashWindowEndsAt = 0.f;
    int32 ahriEssence = 0;
    int32 sonaChord = 0, sonaSong = 0;
    float sonaAuraEndsAt = 0.f;
    float nearsightEndsAt = 0.f;
    int32 gravesShells = 2;
    float gravesReloadEndsAt = 0.f;
    int32 gravesStacks = 0;
    float katarinaREndsAt = 0.f;
    float grievousEndsAt = 0.f;
    float itemAttackSpeed = 0.f;
    int32 items[6] = {};
    float stasisEndsAt = 0.f;
    bool spellShield = false;
    float recallEndsAt = 0.f, recallDuration = 0.f;
    int32 turretPlates = 0;
    int32 dragonSoul = -1;
    float elderEndsAt = 0.f;
    float heraldEyeEndsAt = 0.f;
};

// Sent only to the champion's owner (S_UnitStats).
struct Progress
{
    int32 gold = 0;
    int32 xp = 0, xpToNext = 0;
    int32 ranks[4] = {1, 1, 1, 1};
    int32 skillPoints = 0;
    int32 level = 0;

    struct Summoner
    {
        Protocol::SUMMONER_SPELL spell = Protocol::SUMMONER_NONE;
        float readyAt = 0.f, duration = 0.f;
        int32 charges = 0;
        float rechargeAt = 0.f;
    } summoners[2];

    int32 trinketCharges = 0, wards = 0;
    float trinketRechargeAt = 0.f, trinketRechargeDuration = 0.f;

    struct ItemSlot
    {
        int32 itemId = 0, stacks = 0, charges = -1;
        float cooldownAt = 0.f, cooldownDuration = 0.f;
    } items[6];

    bool inShop = false, canUndo = false;

    struct Stats
    {
        float attackDamage = 0.f, abilityPower = 0.f, armor = 0.f, magicResist = 0.f, attackSpeed = 0.f,
              moveSpeed = 0.f;
        float critChance = 0.f, abilityHaste = 0.f, lethality = 0.f, armorPenPercent = 0.f, magicPen = 0.f,
              magicPenPercent = 0.f;
        float lifeSteal = 0.f, omnivamp = 0.f, healShieldPower = 0.f, tenacity = 0.f, hpRegen = 0.f, manaRegen = 0.f;
    } stats;
};

State& Of(uint64 objectId);

void SetGameTime(float serverSeconds);
float GameTime();
const State* Find(uint64 objectId);
void ApplyBuff(const Protocol::S_Buff& buff);
void SetShield(uint64 objectId, int32 shield);
bool IsStunned(uint64 objectId);
bool IsSilenced(uint64 objectId);
bool IsCharmed(uint64 objectId);
bool IsNearsighted(uint64 objectId);
bool IsChanneling(uint64 objectId);
bool HasGrievousWounds(uint64 objectId);
bool InStasis(uint64 objectId);

float AttackSpeedBonus(uint64 objectId);
// The local champion's gold, experience and skill ranks.
Progress& MyProgress();
// The unit left the game: stop its buff effects.
void Remove(uint64 objectId);
void Clear();
}
