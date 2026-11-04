#include "pch.h"
#include "SummonerSpellsClient.h"
#include "GameSounds.h"
#include "PlayerController.h"
#include "ClientPacketHandler.h"
#include "UnitStatus.h"
#include "MapUnits.h"
#include "LolVfx.h"
#include "GroundHeight.h"
#include <fstream>

namespace
{
const wstring DataFile = L"..\\Resources\\Data\\SummonerSpells.json";
const wstring IconFolder = L"..\\Resources\\Textures\\UI\\Hud\\Summoners\\";

SummonerSpellsClient::Info Spells[] = {
    {Protocol::SUMMONER_FLASH, "Flash", false},  {Protocol::SUMMONER_SMITE, "Smite", true},
    {Protocol::SUMMONER_IGNITE, "Ignite", true}, {Protocol::SUMMONER_HEAL, "Heal", false},
    {Protocol::SUMMONER_GHOST, "Ghost", false},
};

const char* FlashCastVfx = "Summoner_Snowball_Dash_Cast";
const char* FlashArriveVfx = "Summoner_Snowball_Dash_Arrival";
const char* SmiteVfx = "SS_BaseSmite";
const char* HealVfx = "Monster_Heal_Impact";

bool g_loaded = false;

void Load()
{
    if (g_loaded)
        return;
    g_loaded = true;
    std::ifstream file(DataFile);
    json data = json::parse(file, nullptr, false);

    if (data.is_discarded() || !data.contains("spells"))
    {
        DEBUG_LOG("[SummonerSpells] missing Resources/Data/SummonerSpells.json");

        return;
    }

    const float unit = data.value("unitScale", 0.01f);

    for (const auto& spell : data["spells"])
        for (auto& info : Spells)
            if (spell.value("name", "") == info.name)
            {
                info.castRange = spell.value("castRange", 0.f) * unit;
                const float distance = spell.value("distance", 0.f), allies = spell.value("allyRange", 0.f);
                info.indicatorRange = distance > 0.f  ? distance * unit
                                      : allies > 0.f  ? allies * unit
                                      : info.targeted ? info.castRange
                                                      : 0.f;
            }
}

shared_ptr<GameObject> UnitById(uint64 id)
{
    auto me = GAMEMANAGER->_myPlayer;

    if (me && me->_playerInfo && me->_playerInfo->objectid() == id)
        return me->GetGameObject();

    return CUR_SCENE ? CUR_SCENE->FindObjectById(id) : nullptr;
}

shared_ptr<BasePlayerController> ControllerOf(const shared_ptr<GameObject>& unit)
{
    return unit ? unit->GetScript<BasePlayerController>() : nullptr;
}

float DistanceXZ(const Vec3& a, const Vec3& b)
{
    Vec3 d = a - b;
    d.y = 0.f;

    return d.Length();
}

bool ValidTarget(PlayerController& me, Protocol::SUMMONER_SPELL spell, const shared_ptr<GameObject>& unit)
{
    auto controller = ControllerOf(unit);

    if (!controller || !controller->_playerInfo || controller->_playerInfo->hp() <= 0 || !me.IsEnemy(unit))
        return false;
    const Protocol::PLAYER_CHAMPION_TYPE type = controller->_playerInfo->champtype();
    const MapUnitInfo* mapUnit = FindMapUnit(type);

    if (spell == Protocol::SUMMONER_SMITE)
        return mapUnit && mapUnit->bar != MapUnitBar::Structure;

    return !mapUnit && type != Protocol::PLAYER_TYPE_TIBBERS;
}
}

namespace SummonerSpellsClient
{
const Info* Find(Protocol::SUMMONER_SPELL spell)
{
    Load();

    for (const auto& info : Spells)
        if (info.spell == spell)
            return &info;

    return nullptr;
}

wstring IconFile(Protocol::SUMMONER_SPELL spell)
{
    const Info* info = Find(spell);

    return info ? IconFolder + Utils::ToWString(info->name) + L".dds" : L"";
}

void PreloadVfx()
{
    for (const char* system : {FlashCastVfx, FlashArriveVfx, SmiteVfx, HealVfx, "Global_SS_Ignite", "Global_Haste"})
        LolVfx::Get()->Preload(system);
}

shared_ptr<GameObject> TargetFor(PlayerController& me, Protocol::SUMMONER_SPELL spell,
                                 const shared_ptr<GameObject>& picked, const Vec3& point)
{
    if (ValidTarget(me, spell, picked))
        return picked;
    shared_ptr<GameObject> best;
    float bestDistance = CursorForgiveness;

    for (const auto& [id, object] : CUR_SCENE->_players)
    {
        if (!object || !ValidTarget(me, spell, object))
            continue;
        const float distance = DistanceXZ(object->GetTransform()->GetPosition(), point);

        if (distance <= bestDistance)
        {
            best = object;
            bestDistance = distance;
        }
    }

    return best;
}

bool Cast(PlayerController& me, int32 slot, int32 mouseX, int32 mouseY)
{
    Vec3 point = me.GetTransform()->GetPosition();
    auto picked = CUR_SCENE->Pick(mouseX, mouseY, point);

    return CastAt(me, slot, picked, point);
}

bool CastAt(PlayerController& me, int32 slot, const shared_ptr<GameObject>& picked, Vec3 point)
{
    if (slot < 0 || slot > 1 || !me._playerInfo || me._playerInfo->summonerspells_size() <= slot)
        return false;
    const auto spell = static_cast<Protocol::SUMMONER_SPELL>(me._playerInfo->summonerspells(slot));
    const Info* info = Find(spell);

    if (!info)
        return false;
    Protocol::C_SummonerSpell packet;
    packet.set_slot(slot);

    if (info->targeted)
    {
        auto target = TargetFor(me, spell, picked, point);
        auto controller = ControllerOf(target);

        if (!controller || !controller->_playerInfo)
            return false;
        packet.set_targetid(controller->_playerInfo->objectid());
        point = target->GetTransform()->GetPosition();
    }

    packet.mutable_targetpos()->set_x(point.x);
    packet.mutable_targetpos()->set_y(point.y);
    packet.mutable_targetpos()->set_z(point.z);
    NETWORK->SendPacket(ClientPacketHandler::MakeSendBuffer(packet, C_SUMMONER_SPELL));

    return true;
}

void OnCast(const Protocol::S_SummonerSpell& packet)
{
    auto caster = UnitById(packet.casterid());
    auto target = UnitById(packet.targetid());
    const Vec3 from(packet.from().x(), packet.from().y(), packet.from().z());
    Vec3 to(packet.to().x(), packet.to().y(), packet.to().z());

    if (GroundHeight::IsActive())
        to.y = GroundHeight::At(to.x, to.z);
    auto vfx = LolVfx::Get();
    const Vec3 forward = to - from;
    GameSounds::Summoner(packet.spell(), caster, target);

    switch (packet.spell())
    {
    case Protocol::SUMMONER_FLASH:
        vfx->Play(FlashCastVfx, from, forward);

        if (auto controller = ControllerOf(caster))
        {
            controller->SnapTo(to);

            if (auto me = GAMEMANAGER->_myPlayer; me && controller == me)
                me->InterruptForCrowdControl();
        }

        vfx->Play(FlashArriveVfx, to, forward);
        break;
    case Protocol::SUMMONER_SMITE:
        if (target)
            vfx->Play(SmiteVfx, target->GetTransform()->GetPosition(), forward);
        break;
    case Protocol::SUMMONER_HEAL:
        for (const auto& unit : {caster, target})
            if (unit)
            {
                LolVfx::PlayOptions options;
                options.follow = unit;
                vfx->Play(HealVfx, unit->GetTransform()->GetPosition(), Vec3(0.f, 0.f, 1.f), options);
            }
        break;
    default:
        break;
    }
}

void OnCooldown(const Protocol::S_SummonerCooldown& packet)
{
    if (packet.slot() < 0 || packet.slot() > 1)
        return;
    auto& slot = UnitStatus::MyProgress().summoners[packet.slot()];
    const float now = TIME->GetGameTime();
    slot.spell = packet.spell();
    slot.readyAt = now + packet.remaining();
    slot.duration = packet.duration();
    slot.charges = packet.charges();
    slot.rechargeAt = packet.recharge() > 0.f ? now + packet.recharge() : 0.f;
}
}
