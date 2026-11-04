#pragma once

enum class SkillAim
{
    None,
    Direction,
    Point,
    Unit,
};

struct KitSkill
{
    SkillAim aim = SkillAim::None;
    float range = 0.f;
    float width = 0.f;
    float coneDegrees = 0.f;
    bool anyUnit = false;
};

struct ChampionKit
{
    Protocol::PLAYER_CHAMPION_TYPE type;
    const wchar_t* key;
    const char* name;
    const wchar_t* icon;
    int32 pass;
    float scale;
    bool serverCast;
    KitSkill skills[4];

    struct Missile
    {
        int32 skillId;
        int32 variant;
        const char* vfx;
    };

    vector<Missile> missiles;

    std::array<const char*, 4> spells = {};

    std::array<const char*, 4> missileSounds = {};
    std::array<const char*, 4> hitSounds = {};

    std::array<const char*, 4> followUpVfx = {};
    std::array<const char*, 4> followUpBurstVfx = {};
    std::array<const char*, 4> followUpSounds = {};

    std::map<int32, vector<const char*>> skinSoundPrefixes;
    int32 blinkSkill = 0;
    int32 channelSkill = 0;
};

string KitSpellName(int32 type, int32 skillId);

string KitMissileSound(int32 type, int32 skillId);
string KitHitSound(int32 type, int32 skillId);

const ChampionKit* FindKit(int32 type);
const vector<ChampionKit>& AllKits();

const char* KitMissileVfx(int32 type, int32 skillId, int32 variant);

wstring KitIcon(int32 type);

string SkinVfx(const string& system, int32 skinId);

vector<string> SkinSoundPrefixes(int32 type, int32 skinId);
