#include "pch.h"
#include "UnitStatus.h"
#include "SkillVfx.h"
#include "LolVfx.h"
#include "BasePlayerController.h"

namespace
{
unordered_map<uint64, UnitStatus::State>& States()
{
    static unordered_map<uint64, UnitStatus::State> states;

    return states;
}

shared_ptr<GameObject> FindUnit(uint64 objectId)
{
    if (GAMEMANAGER->_myPlayer && GAMEMANAGER->_myPlayer->_playerInfo &&
        GAMEMANAGER->_myPlayer->_playerInfo->objectid() == objectId)
        return GAMEMANAGER->_myPlayer->GetGameObject();

    return CUR_SCENE ? CUR_SCENE->FindObjectById(objectId) : nullptr;
}

void StopVfx(UnitStatus::State& state, int32 type)
{
    if (auto it = state.vfx.find(type); it != state.vfx.end())
    {
        LolVfx::Get()->Stop(it->second, true);
        state.vfx.erase(it);
    }
}
}

namespace UnitStatus
{
State& Of(uint64 objectId)
{
    return States()[objectId];
}

namespace
{
float g_gameTimeBase = -1.f, g_gameTimeAt = 0.f;
}

void SetGameTime(float serverSeconds)
{
    g_gameTimeBase = serverSeconds;
    g_gameTimeAt = TIME->GetGameTime();
}

float GameTime()
{
    return g_gameTimeBase < 0.f ? 0.f : g_gameTimeBase + (TIME->GetGameTime() - g_gameTimeAt);
}

const State* Find(uint64 objectId)
{
    auto it = States().find(objectId);

    return it == States().end() ? nullptr : &it->second;
}

void ApplyBuff(const Protocol::S_Buff& buff)
{
    State& state = Of(buff.targetid());
    const float now = TIME->GetGameTime();
    const bool ended = buff.duration() <= 0.f;

    switch (buff.bufftype())
    {
    case Protocol::BUFF_STUN:
        state.stunEndsAt = now + buff.duration();
        state.stunDuration = buff.duration();
        break;
    case Protocol::BUFF_SILENCE:
        state.silenceEndsAt = now + buff.duration();
        state.silenceDuration = buff.duration();
        break;
    case Protocol::BUFF_STUN_READY:
        state.stunReady = buff.amount() > 0.f;
        break;
    case Protocol::BUFF_PYROMANIA:
        state.pyromania = static_cast<int32>(buff.amount());
        break;
    case Protocol::BUFF_DRAGON_INFERNAL:
    case Protocol::BUFF_DRAGON_MOUNTAIN:
    case Protocol::BUFF_DRAGON_OCEAN:
    case Protocol::BUFF_DRAGON_CLOUD:
        state.dragons[buff.bufftype() - Protocol::BUFF_DRAGON_INFERNAL] = static_cast<int32>(buff.amount());
        break;
    case Protocol::BUFF_HAND_OF_BARON:
        state.baronEndsAt = ended ? 0.f : now + buff.duration();
        break;
    case Protocol::BUFF_CHARM:
        state.charmEndsAt = ended ? 0.f : now + buff.duration();
        break;
    case Protocol::BUFF_SLOW:
        state.slowEndsAt = ended ? 0.f : max(state.slowEndsAt, now + buff.duration());
        break;
    case Protocol::BUFF_ATTACK_SPEED:
        state.attackSpeedBonus = ended ? 0.f : buff.amount();
        state.attackSpeedEndsAt = ended ? 0.f : now + buff.duration();
        break;
    case Protocol::BUFF_ASHE_FOCUS:
        state.asheFocus = static_cast<int32>(buff.amount());
        break;
    case Protocol::BUFF_ASHE_Q:
        state.asheQEndsAt = ended ? 0.f : now + buff.duration();
        break;
    case Protocol::BUFF_AHRI_R:
        state.ahriDashes = static_cast<int32>(buff.amount());
        state.ahriDashWindowEndsAt = ended ? 0.f : now + buff.duration();
        break;
    case Protocol::BUFF_AHRI_PASSIVE:
        state.ahriEssence = static_cast<int32>(buff.amount());
        break;
    case Protocol::BUFF_SONA_CHORD:
        state.sonaChord = static_cast<int32>(buff.amount());
        break;
    case Protocol::BUFF_SONA_AURA:
        state.sonaSong = static_cast<int32>(buff.amount());
        state.sonaAuraEndsAt = ended ? 0.f : now + buff.duration();
        break;
    case Protocol::BUFF_NEARSIGHT:
        state.nearsightEndsAt = ended ? 0.f : now + buff.duration();
        break;
    case Protocol::BUFF_GRAVES_SHELLS:
        state.gravesShells = static_cast<int32>(buff.amount());
        state.gravesReloadEndsAt = state.gravesShells == 0 ? now + buff.duration() : 0.f;
        break;
    case Protocol::BUFF_GRAVES_STACKS:
        state.gravesStacks = ended ? 0 : static_cast<int32>(buff.amount());
        break;
    case Protocol::BUFF_KATARINA_R:
        state.katarinaREndsAt = ended ? 0.f : now + buff.duration();
        break;
    case Protocol::BUFF_GRIEVOUS_WOUNDS:
        state.grievousEndsAt = ended ? 0.f : now + buff.duration();
        break;
    case Protocol::BUFF_STASIS:
        state.stasisEndsAt = ended ? 0.f : now + buff.duration();
        break;
    case Protocol::BUFF_SPELL_SHIELD:
        state.spellShield = buff.amount() > 0.f && !ended;
        break;
    case Protocol::BUFF_RECALL:
        state.recallEndsAt = ended ? 0.f : now + buff.duration();
        state.recallDuration = ended ? 0.f : buff.duration();
        break;
    case Protocol::BUFF_TURRET_PLATES:
        state.turretPlates = ended ? 0 : static_cast<int32>(buff.amount());
        break;
    case Protocol::BUFF_DRAGON_SOUL:
        state.dragonSoul = ended ? -1 : static_cast<int32>(buff.amount());
        break;
    case Protocol::BUFF_ELDER:
        state.elderEndsAt = ended ? 0.f : now + buff.duration();
        break;
    case Protocol::BUFF_HERALD_EYE:
        state.heraldEyeEndsAt = ended || buff.amount() <= 0.f ? 0.f : now + buff.duration();
        break;
    default:
        break;
    }

    StopVfx(state, buff.bufftype());

    if (ended)
        return;
    auto target = FindUnit(buff.targetid());
    auto source = FindUnit(buff.sourceid());
    auto controller = source ? source->GetScript<BasePlayerController>() : nullptr;

    if (!target || !controller || !controller->_playerInfo)
        return;

    if (const uint64 handle =
            PlayBuffVfx(controller->_playerInfo->champtype(), buff, target, controller->_playerInfo->skinid()))
        state.vfx[buff.bufftype()] = handle;
}

void SetShield(uint64 objectId, int32 shield)
{
    Of(objectId).shield = max(0, shield);
}

bool IsStunned(uint64 objectId)
{
    const State* state = Find(objectId);

    return state && (TIME->GetGameTime() < state->stunEndsAt || TIME->GetGameTime() < state->stasisEndsAt);
}

bool InStasis(uint64 objectId)
{
    const State* state = Find(objectId);

    return state && TIME->GetGameTime() < state->stasisEndsAt;
}

bool IsSilenced(uint64 objectId)
{
    const State* state = Find(objectId);

    return state && TIME->GetGameTime() < state->silenceEndsAt;
}

bool IsChanneling(uint64 objectId)
{
    const State* state = Find(objectId);

    return state && state->katarinaREndsAt > TIME->GetGameTime();
}

bool HasGrievousWounds(uint64 objectId)
{
    const State* state = Find(objectId);

    return state && state->grievousEndsAt > TIME->GetGameTime();
}

bool IsNearsighted(uint64 objectId)
{
    const State* state = Find(objectId);

    return state && TIME->GetGameTime() < state->nearsightEndsAt;
}

bool IsCharmed(uint64 objectId)
{
    const State* state = Find(objectId);

    return state && TIME->GetGameTime() < state->charmEndsAt;
}

float AttackSpeedBonus(uint64 objectId)
{
    const State* state = Find(objectId);

    if (!state)
        return 0.f;

    return max(-0.8f, (TIME->GetGameTime() < state->attackSpeedEndsAt ? state->attackSpeedBonus : 0.f) +
                          state->itemAttackSpeed);
}

void Remove(uint64 objectId)
{
    auto it = States().find(objectId);

    if (it == States().end())
        return;

    for (const auto& [type, handle] : it->second.vfx)
        LolVfx::Get()->Stop(handle, true);
    States().erase(it);
}

Progress& MyProgress()
{
    static Progress progress;

    return progress;
}

void Clear()
{
    States().clear();
    MyProgress() = Progress();
}
}
