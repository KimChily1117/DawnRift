#pragma once
#include "ChampionData.h"
#include "MapUnits.h"

struct ChampionCombatStats
{
    float attackRange;
    float attackSpeed;
    float windupRatio;
    bool ranged;
    const char* basicAttackHitVfx;
    const char* basicAttackMissileVfx;
    float centerHeight;

    float AttackCycle() const
    {
        return 1.f / attackSpeed;
    }

    float Windup() const
    {
        return AttackCycle() * windupRatio;
    }
};

inline ChampionCombatStats MakeChampionCombatStats(Protocol::PLAYER_CHAMPION_TYPE champType, const char* hitVfx,
                                                   const char* missileVfx)
{
    const ChampionData& data = GetChampionData(champType);

    return {data.AttackRangeUnits(),      data.attackSpeed, data.attackWindupPercent, data.ranged, hitVfx, missileVfx,
            data.selectionHeight * 0.005f};
}

inline const ChampionCombatStats& GetChampionCombatStats(Protocol::PLAYER_CHAMPION_TYPE champType)
{
    static const ChampionCombatStats annie =
        MakeChampionCombatStats(Protocol::PLAYER_TYPE_ANNIE, "Annie_Base_BA_tar", "Annie_Base_BA_mis");
    static const ChampionCombatStats garen =
        MakeChampionCombatStats(Protocol::PLAYER_TYPE_GAREN, "Garen_Base_AA_Tar", "");
    static const ChampionCombatStats tibbers =
        MakeChampionCombatStats(Protocol::PLAYER_TYPE_TIBBERS, "AnnieTibbers_Base_BA", "");

    if (IsMapUnit(champType))
    {
        static unordered_map<int32, ChampionCombatStats> units;
        auto it = units.find(champType);

        if (it == units.end())
            it = units.emplace(champType, MakeChampionCombatStats(champType, "", "")).first;

        return it->second;
    }

    if (champType == Protocol::PLAYER_TYPE_ANNIE)
        return annie;

    if (champType == Protocol::PLAYER_TYPE_TIBBERS)
        return tibbers;
    static const ChampionCombatStats ashe =
        MakeChampionCombatStats(Protocol::PLAYER_TYPE_ASHE, "Ashe_Base_BA_tar", "Ashe_Base_BA_mis");
    static const ChampionCombatStats ahri =
        MakeChampionCombatStats(Protocol::PLAYER_TYPE_AHRI, "Ahri_Base_BA_tar", "Ahri_Base_BA_mis");

    if (champType == Protocol::PLAYER_TYPE_ASHE)
        return ashe;

    if (champType == Protocol::PLAYER_TYPE_AHRI)
        return ahri;
    static const ChampionCombatStats sona =
        MakeChampionCombatStats(Protocol::PLAYER_TYPE_SONA, "Sona_Base_BA_Tar", "Sona_Base_BA_Mis");
    static const ChampionCombatStats graves =
        MakeChampionCombatStats(Protocol::PLAYER_TYPE_GRAVES, "Graves_Base_BA_tar", "Graves_Base_BA_mis");

    if (champType == Protocol::PLAYER_TYPE_SONA)
        return sona;

    if (champType == Protocol::PLAYER_TYPE_GRAVES)
        return graves;
    static const ChampionCombatStats katarina =
        MakeChampionCombatStats(Protocol::PLAYER_TYPE_KATARINA, "Katarina_Base_BA_Tar", "");

    if (champType == Protocol::PLAYER_TYPE_KATARINA)
        return katarina;

    return garen;
}
