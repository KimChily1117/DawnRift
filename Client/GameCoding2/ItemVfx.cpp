#include "pch.h"
#include "ItemVfx.h"
#include "LolVfx.h"
#include "BasePlayerController.h"

namespace
{
enum class Where
{
    Caster,
    Target,
    Point,
    Hits,
    TargetOrCaster
};

struct Look
{
    int32 itemId;
    const char* system;
    Where where;
    float duration;
    bool onlyWithHits;
};

const Look Looks[] = {
    {3157, "3157_Zhonya_Buf", Where::Caster, 2.5f, false},
    {2420, "3157_Zhonya_Buf", Where::Caster, 2.5f, false},
    {30260, "3026_Buff_Will_Revive", Where::Caster, 4.f, false},
    {3026, "3026_Buff_Revive", Where::Caster, 0.f, false},
    {3190, "3190_Buff_Activate", Where::Caster, 0.f, false},
    {3190, "3190_Locket_Shield_Buff", Where::Hits, 2.5f, false},
    {3107, "Item_Redemption_Cast", Where::Caster, 0.f, false},
    {3107, "Redemption_Ally_Indicator", Where::Point, 2.5f, false},
    {3107, "Item_Redemption_Heal_AOE", Where::Point, 0.f, true},
    {2065, "SRI_Shurelyas_Battlesong_AOE", Where::Caster, 0.f, false},
    {2065, "SRI_Shurelyas_Battlesong_Self_Buff", Where::Hits, 4.f, false},
    {3077, "3077_AoE_Activated", Where::TargetOrCaster, 0.f, false},
    {3074, "3074_AoE_Activated", Where::TargetOrCaster, 0.f, false},
    {6698, "6698_Active_Ring", Where::TargetOrCaster, 0.f, false},
    {3748, "3748_Arc_Activated", Where::TargetOrCaster, 0.f, false},
    {6631, "6631_Shockwave", Where::Caster, 0.f, false},
    {3152, "3152_Dash_Cast", Where::Caster, 0.f, false},
    {3152, "3152_Missile_Cast", Where::Point, 0.f, false},
    {3152, "3152_Missile_Impact_Tar", Where::Hits, 0.f, false},
    {3143, "3143_Active_Cas", Where::Caster, 0.f, false},
    {3143, "3143_Active_Debuff", Where::Hits, 2.f, false},
    {3128, "3128_Tar", Where::Target, 0.f, false},
    {3128, "3128_Debuff", Where::Target, 4.f, false},
    {3146, "3146_Cas", Where::Caster, 0.f, false},
    {3146, "3146_Tar", Where::Target, 0.f, false},
    {3140, "773140_Mercurial_Buf", Where::Caster, 0.f, false},
    {3139, "773140_Mercurial_Buf", Where::Caster, 0.f, false},
    {2138, "772037_ElixirFortitude", Where::Caster, 0.f, false},
    {2139, "772039_ElixirBrilliance", Where::Caster, 0.f, false},
    {2140, "772038_ElixirAgility", Where::Caster, 0.f, false},
    {2003, "Jade_Item_HealthPotion", Where::Caster, 0.f, false},
    {2031, "Jade_Item_HealthPotion", Where::Caster, 0.f, false},
    {3142, "3142_Activate", Where::Caster, 0.f, false},
    {3142, "3142_SpeedTrail", Where::Caster, 6.f, false},
    {3109, "3109_empower_buff", Where::Target, 0.f, false},
    {4016, "4016_Link", Where::Target, 0.f, false},
    {3050, "3050_FireIce_Storm", Where::Caster, 5.f, false},
    {4004, "4004_Dagger", Where::Point, 4.f, false},
    {4003, "4004_Dagger", Where::Point, 4.f, false},
    {40041, "4004_PullBack_Beam", Where::Point, 0.f, false},
    {40031, "4004_PullBack_Beam", Where::Point, 0.f, false},
    {2522, "2522_Self_Buff", Where::Caster, 8.f, false},
    {3005, "3005_ActiveBuff", Where::Caster, 6.f, false},
    // passives
    {3102, "4632_VerdantBarrierVFX", Where::Caster, 0.f, false},
    {3814, "4632_VerdantBarrierVFX", Where::Caster, 0.f, false},
    {4632, "4632_VerdantBarrierVFX", Where::Caster, 0.f, false},
    {3155, "3155_Shield_Burst", Where::Caster, 0.f, false},
    {3156, "3156_Shield_Burst", Where::Caster, 0.f, false},
    {3053, "SRI_6673_Shield", Where::Caster, 4.5f, false},
    {6673, "SRI_6673_Shield", Where::Caster, 3.f, false},
    {6672, "Projectile_Impact", Where::Target, 0.f, false},
    {3222, "Jade_Summoner_Cleanse", Where::Target, 0.f, false},
    {3131, "3073_AttackSpeed_Buff", Where::Caster, 3.f, false},
    {3084, "3084_Goliath_Impact", Where::Target, 0.f, false},
    {3181, "3181_Hit_Tar", Where::Target, 0.f, false},
    {6692, "6692_Shield", Where::Caster, 2.f, false},
    {3137, "3137_Heal_Nova", Where::Caster, 0.f, false},
    {6676, "6676_Execute_Tar", Where::Target, 0.f, false},
    {2502, "2502_Heal_Buff", Where::Caster, 0.f, false},
    {2504, "2504_MagicDMG_Shield", Where::Caster, 0.f, false},
    {4646, "4646_OnHIt_Tar", Where::Target, 0.f, false},
    {46461, "4646_Lightning_Bolt_Large", Where::Target, 0.f, false},
    {6699, "6699_Item_Proc", Where::Target, 0.f, false},
    {3179, "3179_tar", Where::Target, 0.f, false},
    {3742, "3742_Full_Impact", Where::Target, 0.f, false},
    {3302, "3302_Dark_Impact", Where::Target, 0.f, false},
    {4017, "4017_Mark", Where::Target, 0.f, false},
    {2512, "2512_Self_Buff", Where::Caster, 0.f, false},
    {3032, "3032_Buff_Hands", Where::Caster, 0.f, false},
    {3118, "3118_Burn_tar", Where::Target, 3.f, false},
    {2524, "2524_HandBuff", Where::Caster, 0.f, false},
    {4005, "4005_Debuff_Tar", Where::Target, 4.f, false},
    {4012, "4012_Self", Where::Caster, 0.f, false},
    {6695, "6695_tar", Where::Target, 0.f, false},
    {6620, "6620_Heal_Missile", Where::Target, 0.f, false},
    {3144, "3144_tar", Where::Target, 0.f, false},
    {2523, "2523_TargetMark", Where::Target, 0.f, false},
    {6617, "6617_Heal_Missile", Where::Target, 0.f, false},
    {6616, "6616_Buff", Where::Caster, 0.f, false},
    {4633, "4633_Damage_Tar", Where::Target, 0.f, false},
    {3073, "3073_AttackSpeed_Buff", Where::Caster, 8.f, false},
    {6610, "6610_Mark", Where::Target, 0.f, false},
    {6665, "6665_Icathia_Powered_State", Where::Caster, 3.f, false},
    {4401, "4401_buff", Where::Caster, 0.f, false},
    {2525, "2525_Heal_Buff", Where::Caster, 0.f, false},
    {4628, "4628_Vision_Explosion", Where::Target, 0.f, false},
    {6697, "6697_Player_Buff_Level_1", Where::Caster, 0.f, false},
    {6696, "6696_reset_mis", Where::Caster, 0.f, false},
    {3068, "3068_Indicator_On", Where::Caster, 1.f, false},
    {6660, "6660_Indicator_On", Where::Caster, 1.f, false},
    {6664, "6664_Indicator_On", Where::Caster, 1.f, false},
    {66641, "6664_Explosion_AoE", Where::Point, 0.f, false},
    {3075, "3075_Tar", Where::Target, 0.f, false},
    {3076, "3075_Tar", Where::Target, 0.f, false},
    {3078, "Item_3078_TrinityForce_Hand_Glow", Where::Caster, 0.f, false},
    {3057, "Item_3078_TrinityForce_Hand_Glow", Where::Caster, 0.f, false},
    {3100, "Item_3078_TrinityForce_Hand_Glow", Where::Caster, 0.f, false},
    {6662, "6662_Slow_Field", Where::Target, 2.f, false},
    {3508, "3508_Hit_Tar", Where::Target, 0.f, false},
    {2510, "2510_tar", Where::Target, 0.f, false},
    {3116, "3116_Slow_Hit", Where::Target, 0.f, false},
    {6653, "6653_Dot_Tar", Where::Target, 3.f, false},
    {2503, "2503_Dot_Tar", Where::Target, 3.f, false},
    {2508, "2508_Dot_Tar", Where::Target, 3.f, false},
    {6655, "6655_Hit_Tar", Where::Target, 0.f, false},
    {6655, "6655_Hit_Repeated", Where::Hits, 0.f, false},
    {3153, "3153_Max_Stacks", Where::Target, 0.f, false},
    {3087, "3087_Statikk_Shiv_Impact", Where::Hits, 0.f, false},
    {3087, "3087_Statikk_Shiv_Impact", Where::Target, 0.f, false},
    {3124, "3124_tar", Where::Target, 0.f, false},
};

shared_ptr<GameObject> Find(uint64 id)
{
    if (id == 0)
        return nullptr;
    auto me = GAMEMANAGER->_myPlayer;

    if (me && me->_playerInfo && me->_playerInfo->objectid() == id)
        return me->GetGameObject();

    return CUR_SCENE ? CUR_SCENE->FindObjectById(id) : nullptr;
}

Vec3 Forward(const shared_ptr<GameObject>& from, const Vec3& to)
{
    Vec3 d = from ? to - from->GetTransform()->GetPosition() : Vec3(0.f, 0.f, 1.f);
    d.y = 0.f;

    return d.LengthSquared() > 1e-4f ? d / d.Length() : Vec3(0.f, 0.f, 1.f);
}
}

namespace ItemVfx
{
int32 Play(const Protocol::S_ItemEffect& effect)
{
    auto vfx = LolVfx::Get();
    const shared_ptr<GameObject> caster = Find(effect.casterid());
    const shared_ptr<GameObject> target = Find(effect.targetid());
    const Vec3 point(effect.position().x(), effect.position().y(), effect.position().z());
    const bool hasHits = effect.hitobjects_size() > 0;
    int32 played = 0;
    auto at = [&](const shared_ptr<GameObject>& unit, const Vec3& fallback)
    {
        return unit ? unit->GetTransform()->GetPosition() : fallback;
    };
    auto play = [&](const Look& look, const shared_ptr<GameObject>& unit, const Vec3& position, const Vec3& forward)
    {
        LolVfx::PlayOptions options;
        options.duration = look.duration;

        if (unit && look.duration > 0.f)
            options.follow = unit;

        if (vfx->Play(look.system, position, forward, options) != 0)
            ++played;
    };

    for (const Look& look : Looks)
    {
        if (look.itemId != effect.itemid() || (look.onlyWithHits && !hasHits))
            continue;

        if (look.where == Where::Point && look.itemId == 3107 && hasHits && !look.onlyWithHits)
            continue;

        switch (look.where)
        {
        case Where::Caster:
            play(look, caster, at(caster, point), Forward(caster, point));
            break;
        case Where::Target:
            if (target)
                play(look, target, at(target, point), Forward(caster, at(target, point)));
            break;
        case Where::TargetOrCaster:
            break;
        case Where::Point:
            play(look, nullptr, point, Forward(caster, point));
            break;
        case Where::Hits:
            for (int32 i = 0; i < effect.hitobjects_size(); ++i)
                if (auto hit = Find(effect.hitobjects(i)))
                    play(look, hit, hit->GetTransform()->GetPosition(),
                         Forward(caster, hit->GetTransform()->GetPosition()));
            break;
        }
    }

    static const unordered_map<int32, pair<const char*, const char*>> Cleaves = {
        {3077, {"3077_AoE", "3077_AoE_Activated"}},
        {3074, {"3074_AoE", "3074_AoE_Activated"}},
        {6698, {"6698_AA_Splash", "6698_Active_Ring"}},
        {3748, {"3748_Arc", "3748_Arc_Activated"}},
    };

    if (auto cleave = Cleaves.find(effect.itemid()); cleave != Cleaves.end())
    {
        if (target)
            played += vfx->Play(cleave->second.first, at(target, point), Forward(caster, at(target, point))) != 0;
        else
            played += vfx->Play(cleave->second.second, at(caster, point), Vec3(0.f, 0.f, 1.f)) != 0;
    }

    return played;
}

string SystemOf(int32 itemId)
{
    for (const Look& look : Looks)
        if (look.itemId == itemId)
            return look.system;

    return "";
}
}
