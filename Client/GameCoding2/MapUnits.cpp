#include "pch.h"
#include "MapUnits.h"
#include "BasePlayerController.h"

namespace
{
const unordered_map<int32, MapUnitInfo> Units = {
    {Protocol::PLAYER_TYPE_MINION_MELEE,
     {L"MinionOrderMelee", L"MinionChaosMelee", L"SRU_OrderMinionMelee", 0.48f, 0.f, MapUnitBar::Minion}},
    {Protocol::PLAYER_TYPE_MINION_RANGED,
     {L"MinionOrderRanged", L"MinionChaosRanged", L"SRU_OrderMinionRanged", 0.48f, 0.f, MapUnitBar::Minion,
      "SRU_OrderMinionRanged_Base_BA_Mis", "SRU_ChaosMinionRanged_Base_BA_Mis", "SRU_OrderMinionRanged_Base_BA_Tar",
      "SRU_ChaosMinionRanged_Base_BA_Tar", 0.9f}},
    {Protocol::PLAYER_TYPE_MINION_SIEGE,
     {L"MinionOrderSiege", L"MinionChaosSiege", L"SRU_OrderMinionSiege", 0.65f, 0.f, MapUnitBar::Minion,
      "SRU_OrderMinionSiege_Base_SRU_Order_CanM_BA1", "SRU_ChaosMinionSiege_Base_SRU_Chaos_CanM_BA1",
      "SRU_OrderMinionSiege_Base_SRU_Order_CanM_Tar", "SRU_ChaosMinionSiege_Base_SRU_Chaos_CanM_Tar", 1.1f}},
    {Protocol::PLAYER_TYPE_TURRET,
     {L"TurretOrder", L"TurretChaos", L"Turret", 0.884f, 5.25f, MapUnitBar::Structure, "SRU_Order_Turret_BA",
      "SRU_Chaos_Turret_BA", "SRU_Order_Turret_BA_Tar", "SRU_Chaos_Turret_BA_Tar", 4.3f}},
    {Protocol::PLAYER_TYPE_INHIBITOR,
     {L"InhibitorOrder", L"InhibitorChaos", nullptr, 2.14f, 3.7f, MapUnitBar::Structure}},
    {Protocol::PLAYER_TYPE_NEXUS, {L"NexusOrder", L"NexusChaos", nullptr, 3.04f, 5.0f, MapUnitBar::Structure}},
    {Protocol::PLAYER_TYPE_SRU_BLUE, {L"SRU_Blue", L"SRU_Blue", L"SRU_Blue", 1.2f, 0.f, MapUnitBar::Monster}},
    {Protocol::PLAYER_TYPE_SRU_RED, {L"SRU_Red", L"SRU_Red", L"SRU_Red", 1.2f, 0.f, MapUnitBar::Monster}},
    {Protocol::PLAYER_TYPE_SRU_GROMP, {L"SRU_Gromp", L"SRU_Gromp", L"SRU_Gromp", 1.0f, 0.f, MapUnitBar::Monster}},
    {Protocol::PLAYER_TYPE_SRU_MURKWOLF,
     {L"SRU_Murkwolf", L"SRU_Murkwolf", L"SRU_Murkwolf", 0.8f, 0.f, MapUnitBar::Monster}},
    {Protocol::PLAYER_TYPE_SRU_RAZORBEAK,
     {L"SRU_Razorbeak", L"SRU_Razorbeak", L"SRU_Razorbeak", 0.8f, 0.f, MapUnitBar::Monster}},
    {Protocol::PLAYER_TYPE_SRU_KRUG, {L"SRU_Krug", L"SRU_Krug", L"SRU_Krug", 0.95f, 0.f, MapUnitBar::Monster}},
    {Protocol::PLAYER_TYPE_SRU_DRAGON_FIRE,
     {L"SRU_Dragon_Fire", L"SRU_Dragon_Fire", L"SRU_Dragon_Fire", 1.4f, 0.f, MapUnitBar::Monster}},
    {Protocol::PLAYER_TYPE_SRU_DRAGON_EARTH,
     {L"SRU_Dragon_Earth", L"SRU_Dragon_Earth", L"SRU_Dragon_Earth", 1.4f, 0.f, MapUnitBar::Monster}},
    {Protocol::PLAYER_TYPE_SRU_DRAGON_WATER,
     {L"SRU_Dragon_Water", L"SRU_Dragon_Water", L"SRU_Dragon_Water", 1.4f, 0.f, MapUnitBar::Monster}},
    {Protocol::PLAYER_TYPE_SRU_DRAGON_AIR,
     {L"SRU_Dragon_Air", L"SRU_Dragon_Air", L"SRU_Dragon_Air", 1.4f, 0.f, MapUnitBar::Monster}},
    {Protocol::PLAYER_TYPE_SRU_BARON, {L"SRU_Baron", L"SRU_Baron", L"SRU_Baron", 2.0f, 0.f, MapUnitBar::Monster}},

    {Protocol::PLAYER_TYPE_SRU_DRAGON_ELDER,
     {L"SRU_Dragon_Elder", L"SRU_Dragon_Elder", L"SRU_Dragon_Elder", 1.6f, 0.f, MapUnitBar::Monster}},
    {Protocol::PLAYER_TYPE_SRU_RIFTHERALD,
     {L"SRU_RiftHerald", L"SRU_RiftHerald", L"SRU_RiftHerald", 1.5f, 0.f, MapUnitBar::Monster}},
    {Protocol::PLAYER_TYPE_MINION_SUPER,
     {L"MinionOrderSuper", L"MinionChaosSuper", L"SRU_OrderMinionSuper", 0.75f, 0.f, MapUnitBar::Minion}},
    {Protocol::PLAYER_TYPE_SRU_KRUG_MINI,
     {L"SRU_KrugMini", L"SRU_KrugMini", L"SRU_KrugMini", 0.6f, 0.f, MapUnitBar::Monster}},
    {Protocol::PLAYER_TYPE_SRU_KRUG_MINIMINI,
     {L"SRU_KrugMiniMini", L"SRU_KrugMiniMini", L"SRU_KrugMiniMini", 0.4f, 0.f, MapUnitBar::Minion}},
    {Protocol::PLAYER_TYPE_SRU_MURKWOLF_MINI,
     {L"SRU_MurkwolfMini", L"SRU_MurkwolfMini", L"SRU_MurkwolfMini", 0.55f, 0.f, MapUnitBar::Minion}},
    {Protocol::PLAYER_TYPE_SRU_RAZORBEAK_MINI,
     {L"SRU_RazorbeakMini", L"SRU_RazorbeakMini", L"SRU_RazorbeakMini", 0.4f, 0.f, MapUnitBar::Minion}},

    {Protocol::PLAYER_TYPE_WARD, {L"YellowTrinket", L"YellowTrinket", nullptr, 0.35f, 1.1f, MapUnitBar::Minion}},
};
}

const MapUnitInfo* FindMapUnit(Protocol::PLAYER_CHAMPION_TYPE type)
{
    auto it = Units.find(type);

    return it != Units.end() ? &it->second : nullptr;
}

wstring MapUnitModel(const Protocol::ObjectInfo& info)
{
    const MapUnitInfo* unit = FindMapUnit(info.champtype());

    if (!unit)
        return L"";

    return info.teamid() == 2 ? unit->chaosModel : unit->orderModel;
}

const char* MapUnitMissileVfx(const Protocol::ObjectInfo& info)
{
    const MapUnitInfo* unit = FindMapUnit(info.champtype());
    const char* vfx = !unit ? nullptr : info.teamid() == 2 ? unit->chaosMissile : unit->orderMissile;

    return vfx ? vfx : "";
}

const char* MapUnitHitVfx(const Protocol::ObjectInfo& info)
{
    const MapUnitInfo* unit = FindMapUnit(info.champtype());
    const char* vfx = !unit ? nullptr : info.teamid() == 2 ? unit->chaosHit : unit->orderHit;

    return vfx ? vfx : "";
}

float TargetRadius(const shared_ptr<GameObject>& target)
{
    if (!target)
        return 0.f;
    auto controller = target->GetScript<BasePlayerController>();

    if (!controller || !controller->_playerInfo)
        return 0.f;
    const MapUnitInfo* unit = FindMapUnit(controller->_playerInfo->champtype());

    return unit ? unit->radius : 0.f;
}
