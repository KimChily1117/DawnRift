#pragma once

#include "MonoBehaviour.h"
#include "BasePlayerController.h"

class ClickEffectPool;

enum class PlayerCommand
{
    None,
    Move,
    Attack,
    AttackMove,
};

class PlayerController : public BasePlayerController
{
public:
    void LevelUpSkill(int32 skillId);

    void UseItem(int32 slot, int32 mouseX, int32 mouseY);

    bool UpdatePings(int32 mouseX, int32 mouseY, bool worldMouse);
    Vec2 _pingAnchor;
    Vec3 _pingPoint;
    uint64 _pingTarget = 0;

    void CastSkillAt(int32 skillId, const Vec3& point, uint64 targetId = 0)
    {
        CastKitSkill(skillId, point, targetId);
    }

    void Awake() override;
    void Update() override;
    void Start() override;
    void LateUpdate() override;
    void FixedUpdate() override;

    void MoveTo();
    void OnMoveAccepted(uint64 serverMoveId, const Vec3& acceptedDestination, bool wasAdjusted);
    void OnMoveRejected(const Vec3& serverPosition, Protocol::MOVE_REJECT_REASON reason);
    void OnMovementSnapshotState(Protocol::MOVEMENT_SNAPSHOT_STATE state);
    void ConfirmSkillCooldown(int32 skillId);

    void ApplyServerCooldown(int32 skillId, float duration);

    void OnSkillRejected(int32 skillId, Protocol::SKILL_REJECT_REASON reason);
    float GetCooldownRemaining(int32 skillId) const;
    float GetCooldownDuration(int32 skillId) const;
    // Mana the spell costs at its current rank (0 for Garen).
    float GetManaCost(int32 skillId) const;
    void SelectSkill(int32 skillId);

    void IssueMoveCommand(const Vec3& destination, bool attackMove);
    void IssueAttackCommand(shared_ptr<GameObject> target, bool showClickEffect);
    void IssueStopCommand();

    void InterruptForCrowdControl();

    PlayerCommand GetCommand() const
    {
        return _command;
    }

    bool IsEnemy(shared_ptr<GameObject> obj);
    bool HasArrivedAtDestination();

    unordered_map<int, float>& GetCooldowns();

    virtual void ProcSkill(int32 skillId) override;

protected:
    void StopMovementForAction() override;

private:
    float _speed = 2.f;
    bool _legacyMoveActive = false;
    int32 _currentSkillID = 0;

    PlayerCommand _command = PlayerCommand::None;
    weak_ptr<GameObject> _attackTarget;
    float _nextBasicAttackTime = 0.f;
    bool _chaseMoving = false;
    float _lastChaseRequestTime = -999.f;
    Vec3 _lastChaseDestination = Vec3::Zero;
    bool _attackMoveArmed = false;

    float _lastWalkSoundTime = -999.f;
    const float WALK_SOUND_COOLDOWN = 1.75f;

public:
    Vec3 _dest;
    Vec3 _correctPosition;

private:
    void ResolveAttackMoveClick(int32 mouseX, int32 mouseY);
    void UpdateAttackCommand();
    void UpdateAttackMoveCommand();
    bool RequestMove(const Vec3& destination);
    void SendStopCommand();
    void PlayWalkVoice();
    shared_ptr<GameObject> FindNearestEnemy(const Vec3& point, float radius);
    void EnsureSkillData();
    bool SendAuthoritativeMoveRequest(const Vec3& requestedDestination);
    void BeginAnnieWTargeting();
    bool UpdateAnnieWTargeting(int32 mouseX, int32 mouseY);
    void ConfirmAnnieWTarget(const Vec3& targetPosition);
    void TryCastPendingAnnieW();
    void CancelPendingAnnieW(bool hideIndicator);
    void UpdateAttackCursor(int32 mouseX, int32 mouseY);
    void SetAttackCursor(bool enabled);
    void UpdateHeldRangeIndicator();
    void PlaceWard(int32 mouseX, int32 mouseY);

    void BeginKitAim(int32 skillId);
    bool UpdateKitAim(int32 mouseX, int32 mouseY);
    void CancelKitAim();
    void CastKitSkill(int32 skillId, const Vec3& point, uint64 targetId = 0);
    int32 _kitAimSkill = 0;
    Vec3 _kitAimPoint = Vec3::Zero;
    bool _heldIndicator = false;

    unordered_map<int, float> _skillElapsedTime;
    unordered_map<int, SkillData> skillDataTable;
    shared_ptr<ClickEffectPool> _clickEffects;

    bool _annieWTargeting = false;
    bool _annieWPendingMove = false;
    Vec3 _annieWTargetPosition = Vec3::Zero;
    uint64 _annieWMoveId = 0;
    bool _attackCursorEnabled = false;

    static constexpr float AnnieWCastRange = AnnieSkillTuning::WRange;
    static constexpr float AnnieWConeAngleDegrees = AnnieSkillTuning::WConeAngleDegrees;
    static constexpr float AnnieWApproachMargin = 0.25f;
};
