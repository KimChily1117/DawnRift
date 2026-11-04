// 플레이어 조작 공통 처리.
#pragma once
#include "MonoBehaviour.h"
#include "AnnieSkillTuning.h"
#include "ChampionData.h"
#include "ChampionKits.h"

enum class SkillTargetType
{
    SingleTarget,
    Instant,
    Area
};

enum class PlayerState
{
    IDLE,
    RUN,
    ATK1,
    ATK2,
    Q,
    W,
    E,
    R,
    SKILL,
    DIE,
    RECALL

};

enum class SkillType
{
    GeneralAtk,
    QSpell,
    WSpell,
    ESpell,
    RSpell
};

struct SkillData
{
    int SkillId;
    SkillTargetType targetType;
    float range;
    float cooldown;
    bool requiresTarget;
};

inline constexpr float SkillRangeFromData = -1.f;
inline constexpr float SkillCooldownFallback = 10.f;

static unordered_map<Protocol::PLAYER_CHAMPION_TYPE, vector<SkillData>> skillTable = {
    {Protocol::PLAYER_CHAMPION_TYPE::PLAYER_TYPE_GAREN,
     {{(int)SkillType::QSpell, SkillTargetType::SingleTarget, SkillRangeFromData, 0.f, true},
      {(int)SkillType::WSpell, SkillTargetType::Instant, 0.0f, 0.f, false},
      {(int)SkillType::ESpell, SkillTargetType::Area, 3.25f, 0.f, false},
      {(int)SkillType::RSpell, SkillTargetType::SingleTarget, 4.0f, 0.f, true}}},

    {Protocol::PLAYER_CHAMPION_TYPE::PLAYER_TYPE_ANNIE,
     {{(int)SkillType::QSpell, SkillTargetType::SingleTarget, SkillRangeFromData, 0.f, true},
      {(int)SkillType::WSpell, SkillTargetType::Area, SkillRangeFromData, 0.f, false},
      {(int)SkillType::ESpell, SkillTargetType::Instant, 0.0f, 0.f, false},
      {(int)SkillType::RSpell, SkillTargetType::Area, SkillRangeFromData, 0.f, false}}}};

static SkillData WithChampionData(Protocol::PLAYER_CHAMPION_TYPE champ, SkillData skill)
{
    const ChampionSpellData& spell = GetChampionData(champ).Spell(skill.SkillId);

    if (skill.range == SkillRangeFromData)
        skill.range = spell.CastRangeUnits();
    skill.cooldown = spell.CooldownAt(1, SkillCooldownFallback);

    return skill;
}

static vector<SkillData> KitSkillTable(Protocol::PLAYER_CHAMPION_TYPE champ)
{
    vector<SkillData> skills;

    if (const ChampionKit* kit = FindKit(champ); kit && kit->serverCast)
        for (int32 slot = 0; slot < 4; ++slot)
            skills.push_back(
                {slot + 1, kit->skills[slot].aim == SkillAim::None ? SkillTargetType::Instant : SkillTargetType::Area,
                 kit->skills[slot].range, 0.f, false});

    return skills;
}

static SkillData GetSkillData(Protocol::PLAYER_CHAMPION_TYPE champ, int skillId)
{
    for (const SkillData& skill : KitSkillTable(champ))
        if (skill.SkillId == skillId)
            return WithChampionData(champ, skill);

    auto it = skillTable.find(champ);

    if (it != skillTable.end())
    {
        for (const SkillData& skill : it->second)
        {
            if (skill.SkillId == skillId)
                return WithChampionData(champ, skill);
        }
    }

    // 기본값 리턴 (잘못된 ID)
    return {0, SkillTargetType::Instant, 0.0f, 0.0f, false};
}

// ✅ 챔피언의 모든 스킬 반환
static vector<SkillData> GetChampionSkills(Protocol::PLAYER_CHAMPION_TYPE champ)
{
    vector<SkillData> skills;

    for (const SkillData& skill : KitSkillTable(champ))
        skills.push_back(WithChampionData(champ, skill));
    auto it = skillTable.find(champ);

    if (it != skillTable.end())
        for (const SkillData& skill : it->second)
            skills.push_back(WithChampionData(champ, skill));

    return skills;
}

class BasePlayerController : public MonoBehaviour
{
public:
    virtual void Awake() override;
    virtual void Update() override;

    bool ApplyAuthoritativeSnapshot(uint64 serverMoveId, uint64 serverTick, const Vec3& position,
                                    Protocol::MOVEMENT_SNAPSHOT_STATE state);
    void SetMinimumAuthoritativeMoveId(uint64 serverMoveId);
    bool IsMovementLocked() const;
    bool IsActionBusy() const;

    bool IsBasicAttacking() const
    {
        return _isAttackMode && _isBasicAttack;
    }

    bool IsInBasicAttackWindup() const;
    void CancelBasicAttackAnimation();
    void HoldMovementForSkillRequest();

    bool HasAuthoritativeSnapshotStream() const
    {
        return _hasAuthoritativeSnapshotStream;
    }

    uint64 GetLatestAuthoritativeMoveId() const
    {
        return _latestAuthoritativeMoveId;
    }

    static Vec3 GetHitVfxPosition(const shared_ptr<GameObject>& target);

    void PlayDeath();
    void Respawn(const Vec3& position);

    void SnapTo(const Vec3& position);

    void PlayRecall(bool on, float duration = 8.f);
    vector<uint64> _recallEffects;

    void UpdateRecallModels();

    static void PreloadRecallModels(int32 champType, int32 skinId);
    vector<pair<float, wstring>> _recallModels;
    size_t _recallModelStep = 0;
    float _recallStartedAt = 0.f;
    wstring _recallBaseFolder;
    bool _recallModelSwapped = false;

    bool IsDead() const
    {
        return _currentState == PlayerState::DIE;
    }

    virtual void ProcSkill(int32 skillId) = 0;

    void PlayKitSkillAnimation(int32 skillId);
    static constexpr float KitCastLockSeconds = 0.25f;

    // For the kit controllers (KitPlayerController.cpp).
    void FaceDirection(const Vec3& direction)
    {
        AlignToDirection(direction);
    }

    void StartActionAnimation()
    {
        BeginActionAnimation();
    }

    void EndActionAnimation()
    {
        FinishActionAnimation();
    }

    void UpdateKitChannel();
    virtual void PlayServerSkillResult(int32 skillId, const Vec3& castOrigin, const Vec3& castDirection);

protected:
    void AlignToDirection(const Vec3& direction);
    void FinishActionAnimation();
    void BeginActionAnimation();

    void ClearPendingSkillRequest()
    {
        _pendingActionDeadline = 0.f;
    }

    virtual void StopMovementForAction();
    Vec3 _serverSkillCastOrigin = Vec3::Zero;
    Vec3 _serverSkillCastDirection = Vec3::Zero;

    bool IsAuthoritativeMoving() const
    {
        return _hasAuthoritativeSnapshotStream && _authoritativeState == Protocol::MOVEMENT_SNAPSHOT_STATE_MOVING;
    }

    SkillData GetSkillInfo(int32 skillId)
    {
        return GetSkillData(_playerInfo->champtype(), skillId);
    }

public:
    PlayerState _currentState = PlayerState::IDLE;
    int32 _lastAttackAnim = 1;
    bool _isAttackMode = false;

    float _timeToIdle = 0.1f;
    shared_ptr<Protocol::ObjectInfo> _playerInfo;

public:
    void SetLastAttackAnim(int32 anim)
    {
        _lastAttackAnim = anim;
    }

    int32 GetLastAttackAnim()
    {
        return _lastAttackAnim;
    }

    void SetTarget(shared_ptr<GameObject> t)
    {
        _target = t;
    }

    Vec3 direction;

public:
    shared_ptr<GameObject> _target;

private:
    bool IsFinitePosition(const Vec3& position) const;
    void UpdateAuthoritativeInterpolation();
    void SetMovementAnimation(Protocol::MOVEMENT_SNAPSHOT_STATE state);

    float _pendingActionDeadline = 0.f;
    bool _isKitCast = false;
    bool _kitChannel = false;
    float _kitCastLockEndsAt = 0.f;

    bool _isBasicAttack = false;
    bool _basicAttackHitPending = false;
    float _basicAttackWindupEndsAt = 0.f;
    uint64 _basicAttackStartMoveId = 0;
    Protocol::MOVEMENT_SNAPSHOT_STATE _lastSnapshotState = Protocol::MOVEMENT_SNAPSHOT_STATE_UNKNOWN;
    bool _hasAuthoritativeSnapshotStream = false;
    bool _hasAuthoritativeSnapshot = false;
    uint64 _latestAuthoritativeMoveId = 0;
    uint64 _latestAuthoritativeServerTick = 0;
    Vec3 _authoritativeFrom = Vec3(0.f);
    Vec3 _authoritativeTarget = Vec3(0.f);
    float _authoritativeInterpolationElapsed = 0.f;
    float _authoritativeInterpolationDuration = 0.1f;
    Protocol::MOVEMENT_SNAPSHOT_STATE _authoritativeState = Protocol::MOVEMENT_SNAPSHOT_STATE_UNKNOWN;
};
