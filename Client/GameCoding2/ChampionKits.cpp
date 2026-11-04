#include "pch.h"
#include "ChampionKits.h"
#include <fstream>

namespace
{
vector<ChampionKit> MakeKits()
{
    vector<ChampionKit> kits;

    kits.push_back({Protocol::PLAYER_TYPE_GAREN,
                    L"Garen",
                    "Garen",
                    L"garen",
                    11,
                    0.01f,
                    false,
                    {},
                    {},
                    {"GarenQ", "GarenW", "GarenE", "GarenR"}});
    kits.push_back({Protocol::PLAYER_TYPE_ANNIE,
                    L"Annie",
                    "Annie",
                    L"annie",
                    2,
                    0.0001f,
                    false,
                    {},
                    {},
                    {"AnnieQ", "AnnieW", "AnnieE", "AnnieR"}});

    kits.push_back({Protocol::PLAYER_TYPE_ASHE,
                    L"Ashe",
                    "Ashe",
                    L"ashe",
                    11,
                    0.01f,
                    true,
                    {{SkillAim::None},
                     {SkillAim::Direction, 12.f, 0.2f, 40.f},
                     {SkillAim::Point, 150.f, 0.5f},
                     {SkillAim::Direction, 30.f, 1.3f}},
                    {{0, 0, "Ashe_Base_BA_mis"},
                     {0, 1, "Ashe_Base_Q_mis"},
                     {2, 0, "Ashe_Base_W_mis"},
                     {3, 0, "Ashe_Base_E_mis"},
                     {4, 0, "Ashe_Base_R_mis"}},
                    {"AsheQ", "Volley", "AsheSpiritOfTheHawk", "EnchantedCrystalArrow"}});

    kits.push_back({Protocol::PLAYER_TYPE_AHRI,
                    L"Ahri",
                    "Ahri",
                    L"ahri",
                    11,
                    0.01f,
                    true,
                    {{SkillAim::Direction, 9.7f, 1.f},
                     {SkillAim::None},
                     {SkillAim::Direction, 10.f, 0.6f},
                     {SkillAim::Point, 5.f, 0.6f}},
                    {{0, 0, "Ahri_Base_BA_mis"},
                     {1, 0, "Ahri_Base_Q_mis"},
                     {1, 1, "Ahri_Base_Q_mis_02"},
                     {2, 0, "Ahri_Base_W_mis"},
                     {3, 0, "Ahri_Base_E_mis"},
                     {4, 0, "Ahri_Base_R_mis"}},
                    {"AhriQ", "AhriW", "AhriE", "AhriR"}});

    kits.push_back({Protocol::PLAYER_TYPE_SONA,
                    L"Sona",
                    "Sona",
                    L"sona",
                    11,
                    0.01f,
                    true,
                    {{SkillAim::None}, {SkillAim::None}, {SkillAim::None}, {SkillAim::Direction, 10.f, 1.4f}},
                    {{0, 0, "Sona_Base_BA_Mis"},
                     {0, 1, "Sona_Base_Q_PowerChord_Mis"},
                     {0, 2, "Sona_Base_W_Powerchord_Mis"},
                     {0, 3, "Sona_Base_E_Powerchord_Mis"},
                     {1, 0, "Sona_Base_Q_Mis"},
                     {4, 0, "Sona_Base_R_Missile"}},
                    {"SonaQ", "SonaW", "SonaE", "SonaR"}});

    kits.back().skinSoundPrefixes = {{6, {"SonaSkin06Genre01", "SonaDJGenre01Skin06", "SonaSkin06"}},
                                     {46, {"SonaSkin45"}}};

    {
        ChampionKit graves{Protocol::PLAYER_TYPE_GRAVES,
                           L"Graves",
                           "Graves",
                           L"graves",
                           11,
                           0.01f,
                           true,
                           {{SkillAim::Direction, 8.f, 0.4f},
                            {SkillAim::Point, 9.5f, 2.5f},
                            {SkillAim::Point, 3.75f, 0.6f},
                            {SkillAim::Direction, 10.f, 1.f}},
                           {{0, 0, "Graves_Base_BA_mis"},
                            {1, 0, "Graves_Base_Q_Mis"},
                            {2, 0, "Graves_Base_W_mis"},
                            {4, 0, "Graves_Base_R_mis"}},
                           {"GravesQLineSpell", "GravesSmokeGrenade", "GravesMove", "GravesChargeShot"}};
        graves.missileSounds = {nullptr, "Play_sfx_Graves_GravesSmokeGrenadeBoom_OnMissileLaunch", nullptr,
                                "Play_sfx_Graves_GravesChargeShotShot_OnMissileLaunch"};
        graves.hitSounds = {"Play_sfx_Graves_GravesQLineMis_OnHit", "Play_sfx_Graves_GravesSmokeGrenadeBoom_hit",
                            nullptr, "Play_sfx_Graves_GravesChargeShotShot_hit"};
        graves.followUpVfx = {"Graves_Base_Q_Explosion", "Graves_Base_W_tar_red", nullptr,
                              "Graves_Base_R_Explosion_Cone"};
        graves.followUpBurstVfx = {nullptr, "Graves_Base_W_hit", nullptr, nullptr};
        graves.followUpSounds = {"Play_sfx_Graves_GravesQZoneTimer_boom", "Play_sfx_Graves_GravesSmokeGrenadeBoom_loop",
                                 nullptr, nullptr};
        kits.push_back(graves);
    }

    {
        ChampionKit katarina{
            Protocol::PLAYER_TYPE_KATARINA,
            L"Katarina",
            "Katarina",
            L"katarina",
            11,
            0.01f,
            true,
            {{SkillAim::Unit, 6.25f}, {SkillAim::None}, {SkillAim::Unit, 7.25f, 0.f, 0.f, true}, {SkillAim::None}},
            {{1, 0, "Katarina_base_Q_mis"}, {4, 0, "Katarina_Base_R_mis"}},
            {"KatarinaQ", "KatarinaW", "KatarinaE", "KatarinaR"}};
        katarina.missileSounds = {"Play_sfx_Katarina_KatarinaQMis_OnMissileLaunch", nullptr, nullptr, nullptr};
        katarina.hitSounds = {"Play_sfx_Katarina_Katarina_Q_OnHit", nullptr, nullptr,
                              "Play_sfx_Katarina_KatarinaRmis_OnHit"};
        katarina.blinkSkill = 3;
        katarina.channelSkill = 4;
        kits.push_back(katarina);
    }

    return kits;
}
}

string KitMissileSound(int32 type, int32 skillId)
{
    const ChampionKit* kit = FindKit(type);

    return kit && skillId >= 1 && skillId <= 4 && kit->missileSounds[skillId - 1] ? kit->missileSounds[skillId - 1]
                                                                                  : "";
}

string KitHitSound(int32 type, int32 skillId)
{
    const ChampionKit* kit = FindKit(type);

    return kit && skillId >= 1 && skillId <= 4 && kit->hitSounds[skillId - 1] ? kit->hitSounds[skillId - 1] : "";
}

string SkinVfx(const string& system, int32 skinId)
{
    if (skinId <= 0 || system.empty())
        return system;
    size_t at = system.find("_Base_");

    if (at == string::npos)
        at = system.find("_base_");

    if (at == string::npos)
        return system;
    char skin[16];
    snprintf(skin, sizeof(skin), "_Skin%02d_", skinId);

    static unordered_map<string, bool> exists;
    auto has = [](const string& name)
    {
        auto it = exists.find(name);

        if (it == exists.end())
        {
            bool usable = false;
            std::ifstream file("../Resources/Vfx/" + name + ".json");

            if (file)
            {
                const string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                usable = text.find("\"emitters\": []") == string::npos;
            }

            it = exists.emplace(name, usable).first;
        }

        return it->second;
    };
    const string candidate = system.substr(0, at) + skin + system.substr(at + 6);

    if (has(candidate))
        return candidate;

    const string form = system.substr(0, at) + skin + "DAG1_" + system.substr(at + 6);

    return has(form) ? form : system;
}

vector<string> SkinSoundPrefixes(int32 type, int32 skinId)
{
    const ChampionKit* kit = FindKit(type);

    if (!kit || skinId <= 0)
        return {};

    if (auto it = kit->skinSoundPrefixes.find(skinId); it != kit->skinSoundPrefixes.end())
        return vector<string>(it->second.begin(), it->second.end());
    char skin[16];
    snprintf(skin, sizeof(skin), "Skin%02d", skinId);

    return {string(kit->name) + skin};
}

const vector<ChampionKit>& AllKits()
{
    static const vector<ChampionKit> kits = MakeKits();

    return kits;
}

const ChampionKit* FindKit(int32 type)
{
    for (const ChampionKit& kit : AllKits())
        if (kit.type == type)
            return &kit;

    return nullptr;
}

const char* KitMissileVfx(int32 type, int32 skillId, int32 variant)
{
    if (const ChampionKit* kit = FindKit(type))
        for (const auto& missile : kit->missiles)
            if (missile.skillId == skillId && missile.variant == variant)
                return missile.vfx;

    return "";
}

string KitSpellName(int32 type, int32 skillId)
{
    const ChampionKit* kit = FindKit(type);

    return kit && skillId >= 1 && skillId <= 4 && kit->spells[skillId - 1] ? kit->spells[skillId - 1] : "";
}

wstring KitIcon(int32 type)
{
    const ChampionKit* kit = FindKit(type);

    return kit ? kit->icon : L"garen";
}
