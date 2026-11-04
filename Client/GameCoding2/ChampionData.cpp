#include "pch.h"
#include "ChampionData.h"
#include "ChampionKits.h"
#include "MapUnits.h"
#include <fstream>

namespace
{
const wstring ChampionFolder = L"..\\Resources\\Data\\Champions\\";

float ReadFloat(const json& object, const char* name, float fallback)
{
    auto it = object.find(name);

    return it != object.end() && it->is_number() ? it->get<float>() : fallback;
}

vector<float> ReadArray(const json& object, const char* name)
{
    vector<float> values;
    auto it = object.find(name);

    if (it != object.end() && it->is_array())
        for (const json& value : *it)
            if (value.is_number())
                values.push_back(value.get<float>());

    return values;
}

ChampionData Load(const wstring& name)
{
    ChampionData data;
    std::ifstream file(ChampionFolder + name + L".json");
    json root = json::parse(file, nullptr, false);

    if (root.is_discarded() || !root.contains("stats"))
    {
        DEBUG_LOG("[ChampionData] missing or invalid Resources/Data/Champions json");

        return data;
    }

    const json& stats = root["stats"];

    if (stats.contains("hp"))
        data.hp = ReadFloat(stats["hp"], "base", 0.f);

    if (stats.contains("attackDamage"))
        data.attackDamage = ReadFloat(stats["attackDamage"], "base", 0.f);
    data.moveSpeed = ReadFloat(stats, "moveSpeed", data.moveSpeed);
    data.attackRange = ReadFloat(stats, "attackRange", data.attackRange);
    data.attackSpeed = ReadFloat(stats, "attackSpeed", data.attackSpeed);
    data.attackWindupPercent = ReadFloat(stats, "attackWindupPercent", data.attackWindupPercent);
    data.selectionHeight = ReadFloat(stats, "selectionHeight", data.selectionHeight);
    data.ranged = stats.value("ranged", false);
    data.basicAttackMissileSpeed = ReadFloat(stats, "basicAttackMissileSpeed", 0.f);

    const char* slots[] = {"Q", "W", "E", "R"};

    if (root.contains("spells"))
    {
        for (int i = 0; i < 4; ++i)
        {
            if (!root["spells"].contains(slots[i]))
                continue;
            const json& source = root["spells"][slots[i]];
            ChampionSpellData& spell = data.spells[i + 1];
            spell.cooldown = ReadArray(source, "cooldown");
            spell.mana = ReadArray(source, "mana");
            spell.castRange = ReadFloat(source, "castRange", 0.f);
            spell.castRadius = ReadFloat(source, "castRadius", 0.f);
            spell.missileSpeed = ReadFloat(source, "missileSpeed", 0.f);
        }
    }

    data.loaded = true;

    return data;
}
}

float ChampionSpellData::CooldownAt(int rank, float fallback) const
{
    if (cooldown.empty())
        return fallback;

    return cooldown[std::clamp(rank, 1, (int)cooldown.size()) - 1];
}

float ChampionSpellData::ManaCostAt(int rank) const
{
    if (mana.empty())
        return 0.f;

    return mana[std::clamp(rank, 1, (int)mana.size()) - 1];
}

const ChampionSpellData& ChampionData::Spell(int skillId) const
{
    return spells[std::clamp(skillId, 0, 4)];
}

const ChampionData& GetChampionData(Protocol::PLAYER_CHAMPION_TYPE champType)
{
    static const ChampionData tibbers = Load(L"AnnieTibbers");

    if (const MapUnitInfo* unit = FindMapUnit(champType))
    {
        static unordered_map<int32, ChampionData> units;
        auto it = units.find(champType);

        if (it == units.end())
            it = units.emplace(champType, unit->dataFile ? Load(unit->dataFile) : ChampionData()).first;

        return it->second;
    }

    if (champType == Protocol::PLAYER_TYPE_TIBBERS)
        return tibbers;

    static unordered_map<int32, ChampionData> champions;
    const ChampionKit* kit = FindKit(champType);

    if (!kit)
        kit = FindKit(Protocol::PLAYER_TYPE_GAREN);
    auto it = champions.find(kit->type);

    if (it == champions.end())
        it = champions.emplace(kit->type, Load(kit->key)).first;

    return it->second;
}
