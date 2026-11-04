#include "pch.h"
#include "GameSounds.h"
#include "ChampionKits.h"
#include "BasePlayerController.h"
#include "PlayerController.h"
#include "CameraController.h"
#include "FogOfWar.h"
#include "MapUnits.h"
#include "UnitStatus.h"

namespace
{
const wstring Manifest = L"..\\Resources\\Data\\Sounds.json";
const wstring ResourceRoot = L"..\\Resources";
const string AmbienceBed = "Map11_AmbienceBed";
constexpr float OrderVoiceCooldown = 6.f;
bool g_loaded = false;
float g_nextOrderVoice = 0.f;
uint32 g_attackCount = 0;

const Protocol::ObjectInfo* InfoOf(const shared_ptr<GameObject>& unit)
{
    if (!unit)
        return nullptr;
    auto controller = unit->GetScript<BasePlayerController>();

    return controller && controller->_playerInfo ? controller->_playerInfo.get() : nullptr;
}

const char* SlotName(int32 skillId)
{
    switch (skillId)
    {
    case 1:
        return "Q";
    case 2:
        return "W";
    case 3:
        return "E";
    case 4:
        return "R";
    default:
        return "";
    }
}

string MinionPrefix(const Protocol::ObjectInfo& info)
{
    const char* side = info.teamid() == 2 ? "Chaos" : "Order";
    const char* kind = info.champtype() == Protocol::PLAYER_TYPE_MINION_RANGED  ? "Ranged"
                       : info.champtype() == Protocol::PLAYER_TYPE_MINION_SIEGE ? "Siege"
                                                                                : "Melee";

    return string("Play_sfx_HA_") + side + "Minion" + kind + "BasicAttack";
}

bool IsMinion(int32 type)
{
    return type == Protocol::PLAYER_TYPE_MINION_MELEE || type == Protocol::PLAYER_TYPE_MINION_RANGED ||
           type == Protocol::PLAYER_TYPE_MINION_SIEGE;
}

bool IsDragon(int32 type)
{
    return type >= Protocol::PLAYER_TYPE_SRU_DRAGON_FIRE && type <= Protocol::PLAYER_TYPE_SRU_DRAGON_AIR;
}
}

namespace GameSounds
{
void Init()
{
    if (g_loaded)
        return;
    g_loaded = true;

    if (!SOUND->LoadCues(Manifest, ResourceRoot))
        DEBUG_LOG("[Sound] Resources/Data/Sounds.json is missing");
}

void Update()
{
    Init();

    if (auto camera = CameraController::Find())
    {
        Vec3 right(1.f, 0.f, 0.f);

        if (auto main = CUR_SCENE ? CUR_SCENE->GetMainCamera() : nullptr)
            right = main->GetTransform()->GetRight();
        SOUND->SetListener(camera->GetFocus(), right);
    }

    if (GAMEMANAGER->_myPlayer)
        SOUND->PlayLoop(AmbienceBed, 0.6f);
}

string ChampionName(int32 champType)
{
    const ChampionKit* kit = FindKit(champType);

    return kit ? kit->name : "";
}

string FirstCue(std::initializer_list<string> candidates)
{
    for (const string& cue : candidates)
        if (SOUND->HasCue(cue))
            return cue;

    return "";
}

bool IsHiddenFromMe(const shared_ptr<GameObject>& unit)
{
    const Protocol::ObjectInfo* info = InfoOf(unit);
    auto fog = FogOfWar::Get();

    return info && fog && fog->IsHidden(info->objectid());
}

string SkinCue(const string& cue, const Protocol::ObjectInfo* skinOf)
{
    if (!skinOf || skinOf->skinid() <= 0 || cue.empty())
        return cue;
    const string c = ChampionName(skinOf->champtype());

    if (c.empty())
        return cue;

    for (const char* kind : {"Play_sfx_", "Play_vo_"})
    {
        const string prefix = kind + c + "_";

        if (cue.compare(0, prefix.size(), prefix) != 0)
            continue;

        for (const string& skinPrefix : SkinSoundPrefixes(skinOf->champtype(), skinOf->skinid()))
        {
            const string candidate = kind + skinPrefix + "_" + cue.substr(prefix.size());

            if (SOUND->HasCue(candidate))
                return candidate;
        }

        return cue;
    }

    return cue;
}

bool PlayAt(const string& baseCue, const shared_ptr<GameObject>& unit, float volume, const Protocol::ObjectInfo* skinOf)
{
    const string cue = SkinCue(baseCue, skinOf);

    if (cue.empty() || !unit)
        return false;
    const Vec3 position = unit->GetTransform()->GetPosition();

    if (IsHiddenFromMe(unit))
    {
        SOUND->RecordSkipped(cue, position, "fog");

        return false;
    }

    return SOUND->PlayCueAt(cue, position, volume);
}

bool PlayAt(const string& cue, const shared_ptr<GameObject>& unit, float volume)
{
    return PlayAt(cue, unit, volume, InfoOf(unit));
}

void Cast(const shared_ptr<GameObject>& caster, int32 skillId)
{
    const Protocol::ObjectInfo* info = InfoOf(caster);

    if (!info)
        return;
    const int32 type = info->champtype();
    const string c = ChampionName(type);

    if (!c.empty())
    {
        if (skillId == 0)
        {
            const bool second = (++g_attackCount & 1) == 0;
            const UnitStatus::State* state = UnitStatus::Find(info->objectid());
            const bool focus = state && state->asheQEndsAt > TIME->GetGameTime();
            PlayAt(FirstCue({focus ? "Play_sfx_" + c + "_" + c + "QAttack_OnCast" : string(),
                             "Play_sfx_" + c + "_" + c + (second ? "BasicAttack2" : "BasicAttack") + "_OnCast",
                             "Play_sfx_" + c + "_" + c + "BasicAttack_OnCast"}),
                   caster);

            if (g_attackCount % 3 == 0)
                PlayAt(FirstCue({"Play_vo_" + c + "_" + c + "BasicAttack_cast3D", "Play_vo_" + c + "_Spell3DBasic"}),
                       caster);

            return;
        }

        const string slot = SlotName(skillId);
        const string spell = KitSpellName(type, skillId);
        PlayAt(FirstCue({"Play_sfx_" + c + "_" + spell + "_OnCast", "Play_sfx_" + c + "_" + spell + "Attack_OnCast"}),
               caster);
        PlayAt(FirstCue({"Play_vo_" + c + "_" + spell + "_cast3D", "Play_vo_" + c + "_" + spell + "_cast2D",
                         "Play_vo_" + c + "_Spell3D" + slot + "Cast", "Play_vo_" + c + "_Spell2D" + slot + "Cast"}),
               caster);

        return;
    }

    if (IsMinion(type) && skillId == 0)
        PlayAt(FirstCue({MinionPrefix(*info) + "_OnCast", MinionPrefix(*info) + "_OnMissileCast"}), caster, 0.7f);
    else if (IsDragon(type))
        PlayAt("Play_sfx_SRU_Dragon_DragonFireball_OnCast", caster);
    else if (type == Protocol::PLAYER_TYPE_SRU_BARON)
        PlayAt("Play_sfx_SRU_Baron_BaronBasicAttack_cast_mini", caster);
}

void MissileLaunch(const shared_ptr<GameObject>& caster, const shared_ptr<GameObject>& target, int32 skillId,
                   int32 variant)
{
    const Protocol::ObjectInfo* info = InfoOf(caster);

    if (!info)
        return;

    if (skillId > 0)
    {
        const string c = ChampionName(info->champtype());
        const string spell = KitSpellName(info->champtype(), skillId);

        if (c.empty() || spell.empty())
            return;
        const string prefix = "Play_sfx_" + c + "_" + spell;
        PlayAt(FirstCue({KitMissileSound(info->champtype(), skillId),
                         variant == 1 ? prefix + "ReturnMissile_OnMissileLaunch" : string(),
                         prefix + "Missile_OnMissileLaunch", prefix + "_OnMissileLaunch",
                         prefix + "AttackWithSound_OnMissileLaunch", prefix + "DamageMissile_missilelaunch"}),
               caster, 0.8f);

        return;
    }

    if (info->champtype() == Protocol::PLAYER_TYPE_TURRET)
    {
        const Protocol::ObjectInfo* aimed = InfoOf(target);
        const bool champion = aimed && !ChampionName(aimed->champtype()).empty();
        const string side = info->teamid() == 2 ? "Chaos" : "Order";
        PlayAt(
            FirstCue({"Play_sfx_Env_map11_" + side + "Turret" + (champion ? "Champion" : "Minion") + "BasicAttack_cast",
                      "Play_sfx_Env_TurretBasicAttack_missilelaunch"}),
            caster);

        return;
    }

    const string c = ChampionName(info->champtype());

    const string empowered = variant > 0 ? "Play_sfx_" + c + "_" + KitSpellName(info->champtype(), variant) : string();

    if (!c.empty())
        PlayAt(FirstCue({variant > 0 ? empowered + "Attack_OnMissileLaunch" : string(),
                         variant > 0 ? empowered + "PassiveAttack_OnMissileLaunch" : string(),
                         "Play_sfx_" + c + "_" + c + "BasicAttack_OnMissileLaunch"}),
               caster);
}

void Hit(const shared_ptr<GameObject>& source, int32 skillId, const shared_ptr<GameObject>& target)
{
    const Protocol::ObjectInfo* info = InfoOf(source);

    if (!info || !target)
        return;
    const int32 type = info->champtype();
    const string c = ChampionName(type);

    if (!c.empty())
    {
        if (skillId == 0)
        {
            const UnitStatus::State* state = UnitStatus::Find(info->objectid());
            const bool focus = state && state->asheQEndsAt > TIME->GetGameTime();
            PlayAt(FirstCue({focus ? "Play_sfx_" + c + "_" + c + "QAttack_OnHit" : string(),
                             "Play_sfx_" + c + "_" + c + "BasicAttack_OnHit"}),
                   target, 0.8f, info);

            return;
        }

        const string p = "Play_sfx_" + c + "_" + KitSpellName(type, skillId);
        PlayAt(FirstCue({KitHitSound(type, skillId), p + "_OnHit", p + "_hit", p + "Attack_OnHit", p + "Damage_hit",
                         p + "DamageMissile_hit", p + "Missile_OnHit", p + "Missile_hit", p + "Attack_OnCast",
                         p + "_OnHitLocation"}),
               target, 1.f, info);

        return;
    }

    if (IsMinion(type))
        PlayAt(MinionPrefix(*info) + "_OnHit", target, 0.6f);
    else if (type == Protocol::PLAYER_TYPE_TURRET)
    {
        const Protocol::ObjectInfo* hit = InfoOf(target);
        const bool champion = hit && !ChampionName(hit->champtype()).empty();
        const string side = info->teamid() == 2 ? "Chaos" : "Order";
        PlayAt(
            FirstCue({"Play_sfx_Env_map11_" + side + "Turret" + (champion ? "Champion" : "Minion") + "BasicAttack_hit",
                      "Play_sfx_Env_TurretBasicAttack_hit"}),
            target);
    }
    else if (IsDragon(type))
        PlayAt("Play_sfx_SRU_Dragon_DragonFireball_hit", target);
}

void Death(const shared_ptr<GameObject>& unit, bool mine)
{
    const Protocol::ObjectInfo* info = InfoOf(unit);

    if (!info)
        return;
    const int32 type = info->champtype();
    const string c = ChampionName(type);

    if (!c.empty())
    {
        PlayAt("Play_vo_" + c + "_Death3D", unit);
        PlayAt("Play_sfx_" + c + "_Death3D_cast", unit);

        if (mine)
            SOUND->PlayCue("Play_sfx_Death_Self");

        return;
    }

    switch (type)
    {
    case Protocol::PLAYER_TYPE_TURRET:
        PlayAt("Play_sfx_Env_Turret_Destroyed", unit);
        PlayAt("Play_sfx_Env_Turret_shatter", unit);
        break;
    case Protocol::PLAYER_TYPE_INHIBITOR:
        PlayAt("Play_sfx_Env_SRUAP_OrderInhibitor_death_cast", unit);
        break;
    case Protocol::PLAYER_TYPE_NEXUS:
        // The game's end: heard wherever the camera is.
        SOUND->PlayCue(FirstCue({info->teamid() == 2 ? "Play_sfx_Env_Global_EoG_ChaosNexus_death_cast"
                                                     : "Play_sfx_Env_Global_EoG_OrderNexus_death_oc"}));
        break;
    case Protocol::PLAYER_TYPE_SRU_BARON:
        PlayAt("Play_sfx_SRU_Baron_death_reverb", unit);
        break;
    default:
        if (IsDragon(type))
            PlayAt("Play_sfx_SRU_Dragon_death_cast", unit);
        break;
    }
}

void Spawn(const shared_ptr<GameObject>& unit)
{
    const Protocol::ObjectInfo* info = InfoOf(unit);

    if (!info)
        return;

    if (IsDragon(info->champtype()))
        PlayAt("Play_sfx_SRU_Dragon_spawn_cast_vox", unit);
    else if (info->champtype() == Protocol::PLAYER_TYPE_SRU_BARON)
        PlayAt("Play_sfx_SRU_Baron_spawn_out_cast_vox_reverb", unit);
    else if (info->champtype() == Protocol::PLAYER_TYPE_WARD)
        PlayAt("Play_sfx_YellowTrinket_SightWardAudioBuff_OnBuffActivate", unit, 0.8f);
}

void Summoner(int32 spell, const shared_ptr<GameObject>& caster, const shared_ptr<GameObject>& target)
{
    switch (spell)
    {
    case Protocol::SUMMONER_FLASH:
        PlayAt("Play_sfx_SummonerFlash_OnCast", caster);
        break;
    case Protocol::SUMMONER_HEAL:
        PlayAt("Play_sfx_SummonerHeal_OnCast", caster);
        break;
    case Protocol::SUMMONER_GHOST:
        PlayAt("Play_sfx_SummonerHaste_OnCast", caster);
        break;
    case Protocol::SUMMONER_SMITE:
        PlayAt("Play_sfx_SummonerSmite_OnHit", target ? target : caster);
        break;
    case Protocol::SUMMONER_IGNITE:
        PlayAt("Play_sfx_SummonerDot_OnHit", target ? target : caster);
        break;
    default:
        break;
    }
}

void Order(PlayerController& me, bool attack)
{
    const float now = TIME->GetGameTime();

    if (now < g_nextOrderVoice || !me._playerInfo)
        return;
    const string c = ChampionName(me._playerInfo->champtype());

    if (c.empty())
        return;
    g_nextOrderVoice = now + OrderVoiceCooldown;
    SOUND->PlayCue(FirstCue({"Play_vo_" + c + (attack ? "_Attack2DGeneral" : "_Move2DStandard")}));
}
}
