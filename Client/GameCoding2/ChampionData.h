#pragma once

struct ChampionSpellData
{
    vector<float> cooldown;
    vector<float> mana;
    float castRange = 0.f;
    float castRadius = 0.f;
    float missileSpeed = 0.f;

    float CooldownAt(int rank, float fallback) const;
    float ManaCostAt(int rank) const;

    float CastRangeUnits() const
    {
        return castRange * 0.01f;
    }

    float CastRadiusUnits() const
    {
        return castRadius * 0.01f;
    }
};

struct ChampionData
{
    bool loaded = false;
    float hp = 0.f;
    float attackDamage = 0.f;
    float moveSpeed = 335.f;
    float attackRange = 125.f;
    float attackSpeed = 0.625f;
    float attackWindupPercent = 0.3f;
    float selectionHeight = 100.f;
    bool ranged = false;
    float basicAttackMissileSpeed = 0.f;
    ChampionSpellData spells[5];

    const ChampionSpellData& Spell(int skillId) const;

    float MoveSpeedUnits() const
    {
        return moveSpeed * 0.01f;
    }

    float AttackRangeUnits() const
    {
        return attackRange * 0.01f;
    }
};

const ChampionData& GetChampionData(Protocol::PLAYER_CHAMPION_TYPE champType);
