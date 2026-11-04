#include "pch.h"
#include "SkillVfx.h"
#include "LolVfx.h"
#include "ChampionCombatStats.h"
#include "ChampionKits.h"
#include "BasePlayerController.h"
#include "ChampionKits.h"
#include "BasePlayerController.h"

namespace
{
enum class Anchor
{
    CasterGround,
    CasterCenter,
    TargetGround,
    TargetCenter,
    TargetOrCaster,
};

struct Cue
{
    Protocol::PLAYER_CHAMPION_TYPE champion;
    PlayerState skill;
    const char* system;
    Anchor anchor;
    bool follow;
    float delay;
    float duration;
    const wchar_t* bone = nullptr;
};

const Cue Cues[] = {

    {Protocol::PLAYER_TYPE_GAREN, PlayerState::Q, "Garen_Base_Q_Cas", Anchor::CasterCenter, true, 0.f, 0.f},
    {Protocol::PLAYER_TYPE_GAREN, PlayerState::Q, "Garen_Base_Q_Tar", Anchor::TargetCenter, true, 0.25f, 0.f},
    {Protocol::PLAYER_TYPE_GAREN, PlayerState::Q, "Garen_Base_Q_Silence", Anchor::TargetCenter, true, 0.25f, 1.5f},
    {Protocol::PLAYER_TYPE_GAREN, PlayerState::E, "Garen_Base_E_Cas", Anchor::CasterGround, true, 0.f, 0.f},
    {Protocol::PLAYER_TYPE_GAREN, PlayerState::E, "Garen_Base_E_Spin", Anchor::CasterGround, true, 0.f, 3.f},
    {Protocol::PLAYER_TYPE_GAREN, PlayerState::R, "Garen_Base_R_Cas", Anchor::CasterGround, true, 0.f, 0.f},
    {Protocol::PLAYER_TYPE_GAREN, PlayerState::R, "Garen_Base_R_Sword_Tar", Anchor::TargetGround, true, 0.f, 0.f},
    {Protocol::PLAYER_TYPE_GAREN, PlayerState::R, "Garen_Base_R_Tar_Impact", Anchor::TargetGround, false, 0.45f, 0.f},
    {Protocol::PLAYER_TYPE_ANNIE, PlayerState::Q, "Annie_Base_Q_cas", Anchor::CasterCenter, true, 0.f, 0.f},
    {Protocol::PLAYER_TYPE_ANNIE, PlayerState::R, "Annie_Base_R_cas", Anchor::TargetOrCaster, false, 0.f, 0.f},
    {Protocol::PLAYER_TYPE_AHRI, PlayerState::Q, "Ahri_Base_Q_cas", Anchor::CasterCenter, true, 0.f, 0.f},
    {Protocol::PLAYER_TYPE_AHRI, PlayerState::W, "Ahri_Base_W_cas", Anchor::CasterGround, true, 0.f, 0.f},
    {Protocol::PLAYER_TYPE_AHRI, PlayerState::E, "Ahri_Base_E_cas", Anchor::CasterCenter, true, 0.f, 0.f},
    {Protocol::PLAYER_TYPE_AHRI, PlayerState::R, "Ahri_Base_R_cas", Anchor::CasterGround, true, 0.f, 0.f},
    // Sona's melodies: the aura around her for its 3 s.
    {Protocol::PLAYER_TYPE_SONA, PlayerState::Q, "Sona_Base_Q_Zone", Anchor::CasterGround, true, 0.f, 3.f},
    {Protocol::PLAYER_TYPE_SONA, PlayerState::W, "Sona_Base_W_Zone", Anchor::CasterGround, true, 0.f, 3.f},
    {Protocol::PLAYER_TYPE_SONA, PlayerState::E, "Sona_Base_E_Zone", Anchor::CasterGround, true, 0.f, 3.f},
    {Protocol::PLAYER_TYPE_SONA, PlayerState::R, "Sona_Base_R_Cas", Anchor::CasterCenter, true, 0.f, 0.f},
    {Protocol::PLAYER_TYPE_GRAVES, PlayerState::Q, "Graves_Base_Q_cas", Anchor::CasterCenter, true, 0.f, 0.f},
    {Protocol::PLAYER_TYPE_GRAVES, PlayerState::W, "Graves_Base_W_cas", Anchor::CasterCenter, true, 0.f, 0.f},
    {Protocol::PLAYER_TYPE_GRAVES, PlayerState::E, "Graves_Base_E_cas", Anchor::CasterGround, false, 0.f, 0.f},
    {Protocol::PLAYER_TYPE_GRAVES, PlayerState::R, "Graves_Base_R_ChargeShot_cas_01", Anchor::CasterCenter, true, 0.f,
     0.f},
    {Protocol::PLAYER_TYPE_KATARINA, PlayerState::E, "Katarina_Base_E_Trail_mis", Anchor::CasterCenter, true, 0.f, 0.f},
};

struct HitCue
{
    Protocol::PLAYER_CHAMPION_TYPE champion;
    SkillType skill;
    const char* system;
};

const HitCue HitCues[] = {
    {Protocol::PLAYER_TYPE_GAREN, SkillType::ESpell, "Garen_Base_E_Tar"},
    {Protocol::PLAYER_TYPE_ANNIE, SkillType::WSpell, "Annie_Base_W_Hit"},
    {Protocol::PLAYER_TYPE_ANNIE, SkillType::ESpell, "Annie_Base_E_tar"},
    {Protocol::PLAYER_TYPE_ASHE, SkillType::WSpell, "Ashe_Base_W_tar"},
    {Protocol::PLAYER_TYPE_ASHE, SkillType::RSpell, "Ashe_Base_R_tar"},
    {Protocol::PLAYER_TYPE_AHRI, SkillType::QSpell, "Ahri_Base_Q_tar"},
    {Protocol::PLAYER_TYPE_AHRI, SkillType::WSpell, "Ahri_Base_W_tar"},
    {Protocol::PLAYER_TYPE_AHRI, SkillType::ESpell, "Ahri_Base_E_tar"},
    {Protocol::PLAYER_TYPE_AHRI, SkillType::RSpell, "Ahri_Base_R_tar"},
    {Protocol::PLAYER_TYPE_SONA, SkillType::QSpell, "Sona_Base_Q_Tar"},
    {Protocol::PLAYER_TYPE_SONA, SkillType::RSpell, "Sona_Base_R_Tar"},
    {Protocol::PLAYER_TYPE_GRAVES, SkillType::QSpell, "Graves_Base_Q_Tar"},
    {Protocol::PLAYER_TYPE_GRAVES, SkillType::RSpell, "Graves_Base_R_hit"},
    {Protocol::PLAYER_TYPE_KATARINA, SkillType::QSpell, "Katarina_base_Q_tar"},
    {Protocol::PLAYER_TYPE_KATARINA, SkillType::ESpell, "Katarina_Base_E_tar"},
    {Protocol::PLAYER_TYPE_KATARINA, SkillType::RSpell, "Katarina_Base_r_tar"},
};

struct BuffCue
{
    Protocol::PLAYER_CHAMPION_TYPE champion;
    Protocol::BUFF_TYPE buff;
    const char* system;
    bool center;
    bool untilEnded;
};

const BuffCue BuffCues[] = {
    {Protocol::PLAYER_TYPE_GAREN, Protocol::BUFF_DAMAGE_REDUCTION, "Garen_Base_W_Buff", false, false},
    {Protocol::PLAYER_TYPE_GAREN, Protocol::BUFF_SHIELD, "Garen_Base_W_Cas", false, false},
    {Protocol::PLAYER_TYPE_GAREN, Protocol::BUFF_ARMOR_SHRED, "Garen_Base_E_ArmorShred", true, false},
    {Protocol::PLAYER_TYPE_ANNIE, Protocol::BUFF_SHIELD, "Annie_Base_E_buf", false, false},
    {Protocol::PLAYER_TYPE_ANNIE, Protocol::BUFF_STUN_READY, "Annie_Base_StunReady", false, true},
    {Protocol::PLAYER_TYPE_ASHE, Protocol::BUFF_ASHE_Q, "Ashe_Base_Q_buf", true, false},
    {Protocol::PLAYER_TYPE_ASHE, Protocol::BUFF_SLOW, "Ashe_Base_freeze", true, false},
    {Protocol::PLAYER_TYPE_AHRI, Protocol::BUFF_CHARM, "Ahri_Base_E_buf", true, false},
    {Protocol::PLAYER_TYPE_SONA, Protocol::BUFF_SHIELD, "Sona_Base_W_Shield", true, false},
    {Protocol::PLAYER_TYPE_SONA, Protocol::BUFF_MOVE_SPEED, "Sona_Base_E_Buff", false, false},
    {Protocol::PLAYER_TYPE_SONA, Protocol::BUFF_STUN, "Sona_Base_R_buf", true, false},
    {Protocol::PLAYER_TYPE_GRAVES, Protocol::BUFF_NEARSIGHT, "Graves_Base_W_Nearsight", true, false},
    {Protocol::PLAYER_TYPE_GRAVES, Protocol::BUFF_GRAVES_STACKS, "Graves_Base_E_buf", true, false},
    {Protocol::PLAYER_TYPE_KATARINA, Protocol::BUFF_MOVE_SPEED, "Katarina_Base_W_SpeedBoost", false, false},
    {Protocol::PLAYER_TYPE_KATARINA, Protocol::BUFF_KATARINA_R, "Katarina_Base_R_cas", false, false},

    {Protocol::PLAYER_TYPE_NONE, Protocol::BUFF_IGNITE, "Global_SS_Ignite", true, false},
    {Protocol::PLAYER_TYPE_NONE, Protocol::BUFF_GHOST, "Global_Haste", false, false},
};
constexpr float UntilEndedSeconds = 3600.f;

struct ExtraCue
{
    Protocol::PLAYER_CHAMPION_TYPE champion;
    const char* system;
};

const ExtraCue ExtraCues[] = {
    {Protocol::PLAYER_TYPE_ANNIE, "Annie_Base_Q_mis"},
    {Protocol::PLAYER_TYPE_ANNIE, "Annie_Base_Q_tar"},
    {Protocol::PLAYER_TYPE_ANNIE, "Annie_Base_W_cas_Right"},
    {Protocol::PLAYER_TYPE_TIBBERS, "AnnieTibbers_Base_Idle_Ground"},
    {Protocol::PLAYER_TYPE_ASHE, "Ashe_Base_Q_mis"},
    {Protocol::PLAYER_TYPE_ASHE, "Ashe_Base_W_mis"},
    {Protocol::PLAYER_TYPE_ASHE, "Ashe_Base_E_mis"},
    {Protocol::PLAYER_TYPE_ASHE, "Ashe_Base_E_tar_explode"},
    {Protocol::PLAYER_TYPE_ASHE, "Ashe_Base_R_mis"},
    {Protocol::PLAYER_TYPE_AHRI, "Ahri_Base_Q_mis"},
    {Protocol::PLAYER_TYPE_AHRI, "Ahri_Base_Q_mis_02"},
    {Protocol::PLAYER_TYPE_AHRI, "Ahri_Base_W_mis"},
    {Protocol::PLAYER_TYPE_AHRI, "Ahri_Base_E_mis"},
    {Protocol::PLAYER_TYPE_AHRI, "Ahri_Base_R_mis"},
    {Protocol::PLAYER_TYPE_SONA, "Sona_Base_Q_Mis"},
    {Protocol::PLAYER_TYPE_SONA, "Sona_Base_R_Missile"},
    {Protocol::PLAYER_TYPE_SONA, "Sona_Base_Q_PowerChord_Mis"},
    {Protocol::PLAYER_TYPE_SONA, "Sona_Base_W_Powerchord_Mis"},
    {Protocol::PLAYER_TYPE_SONA, "Sona_Base_E_Powerchord_Mis"},
    {Protocol::PLAYER_TYPE_GRAVES, "Graves_Base_Q_Mis"},
    {Protocol::PLAYER_TYPE_GRAVES, "Graves_Base_Q_Explosion"},
    {Protocol::PLAYER_TYPE_GRAVES, "Graves_Base_W_mis"},
    {Protocol::PLAYER_TYPE_GRAVES, "Graves_Base_W_tar_red"},
    {Protocol::PLAYER_TYPE_GRAVES, "Graves_Base_W_hit"},
    {Protocol::PLAYER_TYPE_GRAVES, "Graves_Base_R_mis"},
    {Protocol::PLAYER_TYPE_GRAVES, "Graves_Base_R_Explosion_Cone"},
    {Protocol::PLAYER_TYPE_KATARINA, "Katarina_base_Q_mis"},
    {Protocol::PLAYER_TYPE_KATARINA, "Katarina_Base_R_mis"},
    {Protocol::PLAYER_TYPE_KATARINA, "Katarina_Base_W_mis"},
    {Protocol::PLAYER_TYPE_KATARINA, "Katarina_Base_W_Missile_TopArc"},
    {Protocol::PLAYER_TYPE_KATARINA, "Katarina_Base_Dagger_Ground_Indicator"},
    {Protocol::PLAYER_TYPE_KATARINA, "Katarina_Base_Dagger_Ground_Indicator_Death"},
    {Protocol::PLAYER_TYPE_KATARINA, "Katarina_Base_Dagger_PickUp_Cas"},
    {Protocol::PLAYER_TYPE_KATARINA, "Katarina_Base_Dagger_PickUp_Tar"},
    {Protocol::PLAYER_TYPE_KATARINA, "Katarina_Base_W_Indicator_Ally"},
    {Protocol::PLAYER_TYPE_KATARINA, "Katarina_Base_W_Indicator_Enemy"},
};

Vec3 FacingOf(const shared_ptr<GameObject>& object)
{
    const float yaw = object->GetTransform()->GetLocalRotation().y - XM_PI;

    return Vec3(sinf(yaw), 0.f, cosf(yaw));
}
}

void PreloadChampionVfx(Protocol::PLAYER_CHAMPION_TYPE champion, int32 skinId)
{
    PROFILE_SCOPE("  preload vfx");
    auto vfx = LolVfx::Get();

    for (const Cue& cue : Cues)
        if (cue.champion == champion)
            vfx->Preload(SkinVfx(cue.system, skinId));

    for (const HitCue& cue : HitCues)
        if (cue.champion == champion)
            vfx->Preload(SkinVfx(cue.system, skinId));

    for (const BuffCue& cue : BuffCues)
        if (cue.champion == champion)
            vfx->Preload(SkinVfx(cue.system, skinId));

    for (const ExtraCue& cue : ExtraCues)
        if (cue.champion == champion)
            vfx->Preload(SkinVfx(cue.system, skinId));
    const ChampionCombatStats& stats = GetChampionCombatStats(champion);

    if (stats.basicAttackHitVfx && *stats.basicAttackHitVfx)
        vfx->Preload(SkinVfx(stats.basicAttackHitVfx, skinId));

    if (stats.basicAttackMissileVfx && *stats.basicAttackMissileVfx)
        vfx->Preload(SkinVfx(stats.basicAttackMissileVfx, skinId));

    if (champion == Protocol::PLAYER_TYPE_ANNIE)
        PreloadChampionVfx(Protocol::PLAYER_TYPE_TIBBERS);
}

void PlaySkillVfx(BasePlayerController& caster, PlayerState skill)
{
    if (!caster._playerInfo)
        return;
    const auto champion = caster._playerInfo->champtype();
    const int32 skinId = caster._playerInfo->skinid();
    const shared_ptr<GameObject> self = caster.GetGameObject();
    const shared_ptr<GameObject> target = caster._target;
    auto vfx = LolVfx::Get();

    for (const Cue& cue : Cues)
    {
        if (cue.champion != champion || cue.skill != skill)
            continue;
        const bool onTarget = cue.anchor == Anchor::TargetGround || cue.anchor == Anchor::TargetCenter ||
                              (cue.anchor == Anchor::TargetOrCaster && target);

        if (onTarget && !target)
            continue;
        const shared_ptr<GameObject> anchor = onTarget ? target : self;
        const bool center = cue.anchor == Anchor::CasterCenter || cue.anchor == Anchor::TargetCenter;
        const Vec3 feet = anchor->GetTransform()->GetPosition();
        const Vec3 origin = center ? BasePlayerController::GetHitVfxPosition(anchor) : feet;
        const Vec3 forward = target && target != self
                                 ? target->GetTransform()->GetPosition() - self->GetTransform()->GetPosition()
                                 : FacingOf(self);

        LolVfx::PlayOptions options;
        options.delay = cue.delay;
        options.duration = cue.duration;

        if (cue.follow)
        {
            options.follow = anchor;
            options.followHeight = origin.y - feet.y;

            if (cue.bone)
                options.followBone = cue.bone;
        }

        vfx->Play(SkinVfx(cue.system, skinId), origin, forward, options);
    }
}

uint64 PlayBuffVfx(Protocol::PLAYER_CHAMPION_TYPE champion, const Protocol::S_Buff& buff,
                   const shared_ptr<GameObject>& target, int32 skinId)
{
    if (!target)
        return 0;

    for (const BuffCue& cue : BuffCues)
    {
        if ((cue.champion != champion && cue.champion != Protocol::PLAYER_TYPE_NONE) || cue.buff != buff.bufftype())
            continue;
        const Vec3 feet = target->GetTransform()->GetPosition();
        const Vec3 origin = cue.center ? BasePlayerController::GetHitVfxPosition(target) : feet;
        LolVfx::PlayOptions options;
        options.follow = target;
        options.followHeight = origin.y - feet.y;
        options.duration = cue.untilEnded ? UntilEndedSeconds : buff.duration();

        return LolVfx::Get()->Play(SkinVfx(cue.system, skinId), origin, FacingOf(target), options);
    }

    return 0;
}

bool PlaySkillHitVfx(Protocol::PLAYER_CHAMPION_TYPE champion, int32 skillId, const shared_ptr<GameObject>& source,
                     const shared_ptr<GameObject>& target)
{
    if (!target)
        return false;

    for (const HitCue& cue : HitCues)
    {
        if (cue.champion != champion || (int32)cue.skill != skillId)
            continue;

        const Vec3 center = BasePlayerController::GetHitVfxPosition(target);
        const Vec3 forward =
            source ? target->GetTransform()->GetPosition() - source->GetTransform()->GetPosition() : FacingOf(target);
        int32 skinId = 0;

        if (auto sourceController = source ? source->GetScript<BasePlayerController>() : nullptr;
            sourceController && sourceController->_playerInfo)
            skinId = sourceController->_playerInfo->skinid();

        return LolVfx::Get()->Play(SkinVfx(cue.system, skinId), center, forward) != 0;
    }

    return false;
}
