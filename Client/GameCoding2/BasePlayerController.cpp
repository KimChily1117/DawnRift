// 플레이어 조작 공통 처리.
#include "pch.h"
#include "UnitStatus.h"
#include "BasePlayerController.h"
#include "ModelAnimator.h"
#include "ChampionCombatStats.h"
#include "ClientPacketHandler.h"
#include "LolVfx.h"
#include "SkillVfx.h"
#include "ChampionKits.h"
#include "GameSounds.h"
#include <fstream>

bool BasePlayerController::IsActionBusy() const
{
    if (TIME->GetGameTime() < _pendingActionDeadline)
        return true;

    if (_isKitCast)
        return TIME->GetGameTime() < _kitCastLockEndsAt;

    if (IsBasicAttacking())
        return IsInBasicAttackWindup();

    return _isAttackMode;
}

Vec3 BasePlayerController::GetHitVfxPosition(const shared_ptr<GameObject>& target)
{
    Vec3 position = target->GetTransform()->GetPosition();
    auto controller = target->GetScript<BasePlayerController>();

    if (controller && controller->_playerInfo)
        position.y += GetChampionCombatStats(controller->_playerInfo->champtype()).centerHeight;

    return position;
}

bool BasePlayerController::IsInBasicAttackWindup() const
{
    return IsBasicAttacking() && TIME->GetGameTime() < _basicAttackWindupEndsAt;
}

void BasePlayerController::CancelBasicAttackAnimation()
{
    if (IsBasicAttacking() || (_isKitCast && TIME->GetGameTime() >= _kitCastLockEndsAt))
        FinishActionAnimation();
}

void BasePlayerController::PlayKitSkillAnimation(int32 skillId)
{
    if (skillId < 1 || skillId > 4 || IsDead())
        return;

    if (IsBasicAttacking())
        FinishActionAnimation();
    _currentState = static_cast<PlayerState>((int32)PlayerState::Q + skillId - 1);
    BeginActionAnimation();
    _isKitCast = true;
    _kitCastLockEndsAt = TIME->GetGameTime() + KitCastLockSeconds;
    const ChampionKit* kit = _playerInfo ? FindKit(_playerInfo->champtype()) : nullptr;
    _kitChannel = kit && kit->channelSkill == skillId;

    if (auto animator = GetGameObject()->GetModelAnimator())
    {
        animator->SetAnimation((int32)_currentState, _kitChannel);

        if (!_kitChannel)
            animator->SetAnimationEndCallback([this]() { FinishActionAnimation(); });
    }
}

void BasePlayerController::UpdateKitChannel()
{
    if (!_kitChannel || !_isKitCast || !_playerInfo || TIME->GetGameTime() < _kitCastLockEndsAt + 0.2f)
        return;

    if (!UnitStatus::IsChanneling(_playerInfo->objectid()))
    {
        _kitChannel = false;
        FinishActionAnimation();
    }
}

bool BasePlayerController::IsMovementLocked() const
{
    if (TIME->GetGameTime() < _pendingActionDeadline)
        return true;

    if (_isKitCast)
        return TIME->GetGameTime() < _kitCastLockEndsAt;

    if (IsBasicAttacking())
        return IsInBasicAttackWindup();

    return _isAttackMode &&
           !(_playerInfo && _playerInfo->champtype() == Protocol::PLAYER_TYPE_GAREN && _currentState == PlayerState::E);
}

void BasePlayerController::StopMovementForAction()
{
    _authoritativeFrom = _authoritativeTarget = GetTransform()->GetPosition();
    _authoritativeInterpolationElapsed = _authoritativeInterpolationDuration;
    _authoritativeState = Protocol::MOVEMENT_SNAPSHOT_STATE_CANCELLED;
}

void BasePlayerController::HoldMovementForSkillRequest()
{
    _pendingActionDeadline = TIME->GetGameTime() + 2.f;
    StopMovementForAction();
}

void BasePlayerController::BeginActionAnimation()
{
    _pendingActionDeadline = 0.f;
    _isAttackMode = true;
    auto animator = GetGameObject()->GetModelAnimator();
    float duration = animator ? animator->GetAnimationDuration((int32)_currentState) : 1.f;

    if (!std::isfinite(duration) || duration <= .01f)
        duration = 1.f;
    const float now = TIME->GetGameTime();
    _isBasicAttack = _currentState == PlayerState::ATK1 || _currentState == PlayerState::ATK2;
    float playbackSpeed = 1.f;

    if (_isBasicAttack && _playerInfo)
    {
        const ChampionCombatStats& stats = GetChampionCombatStats(_playerInfo->champtype());
        const float haste = 1.f + UnitStatus::AttackSpeedBonus(_playerInfo->objectid());
        playbackSpeed = duration / (stats.AttackCycle() / haste);
        duration = stats.AttackCycle() / haste;
        _basicAttackWindupEndsAt = now + stats.Windup() / haste;
        _basicAttackHitPending = !stats.ranged;
        _basicAttackStartMoveId = _latestAuthoritativeMoveId;
    }

    if (animator)
        animator->GetTweenDesc().curr.speed = playbackSpeed;
    _timeToIdle = now + duration;
    PlaySkillVfx(*this, _currentState);

    if (IsMovementLocked())
        StopMovementForAction();
}

void BasePlayerController::PlayServerSkillResult(int32 skillId, const Vec3& castOrigin, const Vec3& castDirection)
{
    _serverSkillCastOrigin = castOrigin;
    _serverSkillCastDirection = castDirection;

    if (castDirection.LengthSquared() > 0.0001f)
    {
        direction = castDirection;
        AlignToDirection(castDirection);
    }

    ProcSkill(skillId);
}

void BasePlayerController::Awake() {}

void BasePlayerController::Update()
{
    UpdateAuthoritativeInterpolation();
    UpdateRecallModels();

    if (_isKitCast && TIME->GetGameTime() >= _kitCastLockEndsAt && IsAuthoritativeMoving())
        FinishActionAnimation();

    if (_basicAttackHitPending && IsBasicAttacking() && !IsInBasicAttackWindup())
    {
        _basicAttackHitPending = false;

        if (_target && _playerInfo)
        {
            const Vec3 targetPosition = GetHitVfxPosition(_target);
            LolVfx::Get()->Play(
                SkinVfx(GetChampionCombatStats(_playerInfo->champtype()).basicAttackHitVfx, _playerInfo->skinid()),
                targetPosition, _target->GetTransform()->GetPosition() - GetTransform()->GetPosition());
        }
    }
}

bool BasePlayerController::ApplyAuthoritativeSnapshot(uint64 serverMoveId, uint64 serverTick, const Vec3& position,
                                                      Protocol::MOVEMENT_SNAPSHOT_STATE state)
{
    if (!IsFinitePosition(position) || serverMoveId == 0 || state == Protocol::MOVEMENT_SNAPSHOT_STATE_UNKNOWN)
        return false;

    if (serverMoveId < _latestAuthoritativeMoveId)
        return false;

    if (serverMoveId == _latestAuthoritativeMoveId && _hasAuthoritativeSnapshot)
    {
        const bool terminal =
            state == Protocol::MOVEMENT_SNAPSHOT_STATE_CANCELLED || state == Protocol::MOVEMENT_SNAPSHOT_STATE_ARRIVED;

        if (serverTick < _latestAuthoritativeServerTick ||
            (serverTick == _latestAuthoritativeServerTick &&
             !(_lastSnapshotState == Protocol::MOVEMENT_SNAPSHOT_STATE_MOVING && terminal)))
            return false;

        if (_lastSnapshotState != Protocol::MOVEMENT_SNAPSHOT_STATE_MOVING &&
            state == Protocol::MOVEMENT_SNAPSHOT_STATE_MOVING)
            return false;
    }

    if (serverMoveId > _latestAuthoritativeMoveId)
        _latestAuthoritativeServerTick = 0;
    _latestAuthoritativeMoveId = serverMoveId;
    _latestAuthoritativeServerTick = serverTick;
    _hasAuthoritativeSnapshotStream = true;
    _hasAuthoritativeSnapshot = true;
    _lastSnapshotState = state;
    _authoritativeState = state;

    if (IsBasicAttacking() && state == Protocol::MOVEMENT_SNAPSHOT_STATE_MOVING &&
        serverMoveId > _basicAttackStartMoveId)
        FinishActionAnimation();

    if (IsMovementLocked() && state == Protocol::MOVEMENT_SNAPSHOT_STATE_MOVING)
    {
        StopMovementForAction();

        return true;
    }

    Vec3 current = GetTransform()->GetPosition();
    Vec3 delta = position - current;
    const float distanceSquared = delta.LengthSquared();

    if (state == Protocol::MOVEMENT_SNAPSHOT_STATE_ARRIVED || state == Protocol::MOVEMENT_SNAPSHOT_STATE_CANCELLED ||
        distanceSquared > 9.0f)
    {
        GetTransform()->SetPosition(position);
        _authoritativeFrom = position;
        _authoritativeTarget = position;
        _authoritativeInterpolationElapsed = _authoritativeInterpolationDuration;
    }
    else
    {
        _authoritativeFrom = current;
        _authoritativeTarget = position;
        _authoritativeInterpolationElapsed = 0.f;
    }

    if (!IsMovementLocked() && delta.LengthSquared() > 0.0001f)
        AlignToDirection(delta);
    SetMovementAnimation(state);

    return true;
}

void BasePlayerController::SetMinimumAuthoritativeMoveId(uint64 serverMoveId)
{
    if (serverMoveId > _latestAuthoritativeMoveId)
    {
        _latestAuthoritativeMoveId = serverMoveId;
        _latestAuthoritativeServerTick = 0;
        _hasAuthoritativeSnapshot = false;
    }

    _hasAuthoritativeSnapshotStream = true;
}

void BasePlayerController::UpdateAuthoritativeInterpolation()
{
    if (IsMovementLocked() || !_hasAuthoritativeSnapshotStream || !_hasAuthoritativeSnapshot)
        return;

    if (_authoritativeInterpolationElapsed >= _authoritativeInterpolationDuration)
        return;

    float deltaTime = TIME->GetDeltaTime();

    if (!std::isfinite(deltaTime) || deltaTime <= 0.f)
        return;
    _authoritativeInterpolationElapsed =
        min(_authoritativeInterpolationElapsed + deltaTime, _authoritativeInterpolationDuration);
    float t = _authoritativeInterpolationDuration <= 0.f
                  ? 1.f
                  : _authoritativeInterpolationElapsed / _authoritativeInterpolationDuration;
    GetTransform()->SetPosition(Vec3::Lerp(_authoritativeFrom, _authoritativeTarget, t));
}

void BasePlayerController::SetMovementAnimation(Protocol::MOVEMENT_SNAPSHOT_STATE state)
{
    if (_isAttackMode || _currentState == PlayerState::DIE)
        return;
    PlayerState nextState = state == Protocol::MOVEMENT_SNAPSHOT_STATE_MOVING ? PlayerState::RUN : PlayerState::IDLE;

    if (_currentState == nextState)
        return;
    _currentState = nextState;

    if (GetGameObject()->GetModelAnimator())
        GetGameObject()->GetModelAnimator()->SetAnimation((int32)_currentState, true);
}

void BasePlayerController::FinishActionAnimation()
{
    _pendingActionDeadline = 0.f;
    _isKitCast = false;
    _isAttackMode = false;
    _isBasicAttack = false;
    _basicAttackHitPending = false;
    _basicAttackWindupEndsAt = 0.f;

    if (auto animator = GetGameObject()->GetModelAnimator())
        animator->GetTweenDesc().curr.speed = 1.f;

    if (_playerInfo)
        ClientPacketHandler::g_lastPlayedSkill.erase(_playerInfo->objectid());

    if (_currentState == PlayerState::DIE || (_playerInfo && _playerInfo->hp() <= 0))
        return;
    _currentState = IsAuthoritativeMoving() ? PlayerState::RUN : PlayerState::IDLE;

    if (GetGameObject()->GetModelAnimator())
        GetGameObject()->GetModelAnimator()->SetAnimation((int32)_currentState, true);
}

void BasePlayerController::PlayDeath()
{
    _pendingActionDeadline = 0.f;
    _isAttackMode = false;
    _isBasicAttack = false;
    _basicAttackHitPending = false;
    _basicAttackWindupEndsAt = 0.f;
    _target.reset();
    // Stay where the server last put the champion.
    _authoritativeFrom = _authoritativeTarget = GetTransform()->GetPosition();
    _authoritativeInterpolationElapsed = _authoritativeInterpolationDuration;
    _authoritativeState = Protocol::MOVEMENT_SNAPSHOT_STATE_CANCELLED;

    if (_playerInfo)
        ClientPacketHandler::g_lastPlayedSkill.erase(_playerInfo->objectid());
    _currentState = PlayerState::DIE;
    auto animator = GetGameObject()->GetModelAnimator();

    if (animator && animator->GetAnimationCount() > (uint32)PlayerState::DIE)
    {
        animator->GetTweenDesc().curr.speed = 1.f;
        animator->SetAnimation((int32)PlayerState::DIE, false);
    }
}

namespace
{
struct RecallEffect
{
    string system;
    float start = 0.f, end = -1.f;
    wstring bone;
};

const json& RecallTable()
{
    static json table = []
    {
        std::ifstream file(L"../Resources/Data/RecallEffects.json");
        json root = json::parse(file, nullptr, false);

        if (root.is_discarded())
            DEBUG_LOG("[Recall] Resources/Data/RecallEffects.json is missing");

        return root;
    }();

    return table;
}

vector<RecallEffect> RecallEffectsOf(const string& champion, int32 skin)
{
    vector<RecallEffect> effects;
    const json& table = RecallTable();

    if (table.is_discarded() || !table.contains("skins") || !table["skins"].contains(champion))
        return effects;
    const json& skins = table["skins"][champion];
    const string key = std::to_string(skin);

    if (!skins.contains(key))
        return effects;

    for (const json& entry : skins[key])
    {
        RecallEffect effect;
        effect.system = entry.value("system", "");
        effect.start = entry.value("start", 0.f);
        effect.end = entry.contains("end") && entry["end"].is_number() ? entry["end"].get<float>() : -1.f;
        const string bone = entry.value("bone", "");
        effect.bone = wstring(bone.begin(), bone.end());
        effects.push_back(effect);
    }

    return effects;
}
}

namespace
{
const json& RecallModelTable()
{
    static json table = []
    {
        std::ifstream file(L"../Resources/Data/RecallModels.json");

        return json::parse(file, nullptr, false);
    }();

    return table;
}
}

void BasePlayerController::PreloadRecallModels(int32 champType, int32 skinId)
{
    const wstring folder = ChampionSkinFolder(champType, skinId);
    const json& table = RecallModelTable();
    const string key = Utils::ToString(folder);

    if (folder.empty() || table.is_discarded() || !table.contains(key))
        return;

    for (const json& entry : table[key])
        LoadChampionModel(Utils::ToWString(entry.value("model", "")));
}

void BasePlayerController::UpdateRecallModels()
{
    auto animator = GetGameObject() ? GetGameObject()->GetModelAnimator() : nullptr;

    if (!animator)
        return;

    if (_currentState != PlayerState::RECALL)
    {
        if (_recallModelSwapped && !_recallBaseFolder.empty())
            if (auto model = LoadChampionModel(_recallBaseFolder))
                animator->SetModel(model);
        _recallModelSwapped = false;
        _recallModels.clear();

        return;
    }

    const float elapsed = TIME->GetGameTime() - _recallStartedAt;

    while (_recallModelStep < _recallModels.size() && elapsed >= _recallModels[_recallModelStep].first)
    {
        if (auto model = LoadChampionModel(_recallModels[_recallModelStep].second))
        {
            animator->SetModel(model);
            _recallModelSwapped = _recallModels[_recallModelStep].second != _recallBaseFolder;
        }

        ++_recallModelStep;
    }
}

void BasePlayerController::PlayRecall(bool on, float duration)
{
    auto animator = GetGameObject()->GetModelAnimator();
    auto vfx = LolVfx::Get();

    if (vfx)
        for (uint64 handle : _recallEffects)
            vfx->Stop(handle, true);
    _recallEffects.clear();

    if (on)
    {
        if (_currentState == PlayerState::DIE || (_playerInfo && _playerInfo->hp() <= 0) || IsActionBusy())
            return;
        _currentState = PlayerState::RECALL;

        if (animator && animator->GetAnimationCount() > (uint32)PlayerState::RECALL)
            animator->SetAnimation((int32)PlayerState::RECALL, true);

        _recallModels.clear();
        _recallModelStep = 0;
        _recallStartedAt = TIME->GetGameTime();

        if (_playerInfo)
        {
            _recallBaseFolder = ChampionSkinFolder(_playerInfo->champtype(), _playerInfo->skinid());
            const json& table = RecallModelTable();
            const string key = Utils::ToString(_recallBaseFolder);

            if (!table.is_discarded() && table.contains(key))
                for (const json& entry : table[key])
                {
                    const string folder = entry.value("model", "");
                    _recallModels.push_back({entry.value("start", 0.f), Utils::ToWString(folder)});
                    LoadChampionModel(Utils::ToWString(folder));
                }
        }

        if (_playerInfo)
        {
            vector<string> prefixes = SkinSoundPrefixes(_playerInfo->champtype(), _playerInfo->skinid());
            prefixes.push_back(GameSounds::ChampionName(_playerInfo->champtype()));
            static const char* const Effects[] = {
                "_Recall3D_buffactivate", "_Recall_buffactivate", "_Recall_leadin_buffactivate",
                "_Recall_leadin",         "_Recall3D_windup",     "_Recall_windup"};
            bool effectPlayed = false, voicePlayed = false;

            for (const string& prefix : prefixes)
            {
                for (const char* suffix : Effects)
                    if (!effectPlayed && SOUND->HasCue("Play_sfx_" + prefix + suffix) &&
                        SOUND->PlayCueAt("Play_sfx_" + prefix + suffix, GetTransform()->GetPosition()))
                        effectPlayed = true;

                if (!voicePlayed && SOUND->HasCue("Play_vo_" + prefix + "_Recall3DGeneral") &&
                    SOUND->PlayCueAt("Play_vo_" + prefix + "_Recall3DGeneral", GetTransform()->GetPosition()))
                    voicePlayed = true;
            }
        }

        if (vfx && _playerInfo)
        {
            const float channel = duration > 0.f ? duration : 8.f;
            const Vec3 forward = Vec3::Forward;
            LolVfx::PlayOptions shared;
            shared.follow = GetGameObject();
            shared.duration = channel;
            const json& table = RecallTable();

            if (!table.is_discarded() && table.contains("shared"))
                for (const json& system : table["shared"])
                    if (uint64 handle = vfx->Play(system.get<string>(), GetTransform()->GetPosition(), forward, shared))
                        _recallEffects.push_back(handle);

            for (const RecallEffect& effect :
                 RecallEffectsOf(GameSounds::ChampionName(_playerInfo->champtype()), _playerInfo->skinid()))
            {
                if (effect.start >= channel)
                    continue;
                LolVfx::PlayOptions options;
                options.follow = GetGameObject();
                options.followBone = effect.bone;
                options.delay = effect.start;

                options.duration = (effect.end > effect.start ? min(effect.end, channel) : channel) - effect.start;

                if (uint64 handle = vfx->Play(effect.system, GetTransform()->GetPosition(), forward, options))
                    _recallEffects.push_back(handle);
            }
        }

        return;
    }

    if (_currentState != PlayerState::RECALL)
        return;
    _currentState = IsAuthoritativeMoving() ? PlayerState::RUN : PlayerState::IDLE;

    if (animator)
        animator->SetAnimation((int32)_currentState, true);
}

void BasePlayerController::SnapTo(const Vec3& position)
{
    GetTransform()->SetPosition(position);
    _authoritativeFrom = _authoritativeTarget = position;
    _authoritativeInterpolationElapsed = _authoritativeInterpolationDuration;
}

void BasePlayerController::Respawn(const Vec3& position)
{
    GetTransform()->SetPosition(position);
    _authoritativeFrom = _authoritativeTarget = position;
    _authoritativeInterpolationElapsed = _authoritativeInterpolationDuration;
    _authoritativeState = Protocol::MOVEMENT_SNAPSHOT_STATE_ARRIVED;
    _currentState = PlayerState::IDLE;

    if (auto animator = GetGameObject()->GetModelAnimator())
        animator->SetAnimation((int32)PlayerState::IDLE, true);
}

bool BasePlayerController::IsFinitePosition(const Vec3& position) const
{
    return std::isfinite(position.x) && std::isfinite(position.y) && std::isfinite(position.z);
}

void BasePlayerController::AlignToDirection(const Vec3& direction)
{
    if (direction.Length() > 0.01f)
    {
        Vec3 normalizedDir = direction;
        normalizedDir.y = 0.0f;
        normalizedDir.Normalize();

        float angle = atan2f(normalizedDir.x, normalizedDir.z) + XM_PI;
        GetTransform()->SetRotation(Vec3(XMConvertToRadians(90.f), angle, 0.0f));
    }
}
