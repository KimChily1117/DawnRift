#pragma once

enum class MapUnitBar
{
    Minion,
    Monster,
    Structure
};

struct MapUnitInfo
{
    const wchar_t* orderModel;
    const wchar_t* chaosModel;
    const wchar_t* dataFile;
    float radius;
    float barHeight;
    MapUnitBar bar;

    const char* orderMissile;
    const char* chaosMissile;
    const char* orderHit;
    const char* chaosHit;
    float missileHeight;
};

const MapUnitInfo* FindMapUnit(Protocol::PLAYER_CHAMPION_TYPE type);

inline bool IsMapUnit(Protocol::PLAYER_CHAMPION_TYPE type)
{
    return FindMapUnit(type) != nullptr;
}

// The unit's model folder for its side.
wstring MapUnitModel(const Protocol::ObjectInfo& info);

const char* MapUnitMissileVfx(const Protocol::ObjectInfo& info);
const char* MapUnitHitVfx(const Protocol::ObjectInfo& info);

float TargetRadius(const shared_ptr<GameObject>& target);
