#include "pch.h"
#include "Utils.h"
#include "PlayerController.h"
#include "GameSounds.h"
#include "SummonerSpellsClient.h"
#include "GameCursor.h"
#include "HUDController.h"
#include "ModelAnimator.h"
#include "ModelRenderer.h"
#include "MeshRenderer.h"
#include "Terrain.h"
#include "OtherPlayerController.h"
#include "ClickEffectPool.h"
#include "ChampionCombatStats.h"
#include "AnnieWSpell.h"
#include "SkillIndicatorController.h"
#include "Button.h"
#include "UnitStatus.h"
#include "ItemsClient.h"
#include "Pings.h"
#include "Chat.h"
#include "GameFlow.h"
#include "Keybinds.h"
#include "MapUnits.h"

static float GetSkillRange(Protocol::PLAYER_CHAMPION_TYPE champType, int skillId)
{
    if (skillId == 0)
        return GetChampionCombatStats(champType).attackRange;

    return GetSkillData(champType, skillId).range;
}

void PlayerController::Awake() {}

namespace
{
constexpr float ChaseRepathInterval = 0.25f;
constexpr float ChaseRepathDistance = 0.5f;

constexpr float AttackMoveClickRadius = 2.5f;

float DistanceXZ(const Vec3& a, const Vec3& b)
{
    Vec3 delta = a - b;
    delta.y = 0.f;

    return delta.Length();
}
}

void PlayerController::Start()
{
    if (!_clickEffects && CUR_SCENE->_shader)
    {
        _clickEffects = make_shared<ClickEffectPool>();
        _clickEffects->Init(CUR_SCENE->_shader);
    }
}

void PlayerController::LateUpdate() {}

void PlayerController::FixedUpdate() {}

void PlayerController::StopMovementForAction()
{
    BasePlayerController::StopMovementForAction();
    _legacyMoveActive = false;
    _dest = _correctPosition = GetTransform()->GetPosition();
    CancelPendingAnnieW(true);
}

void PlayerController::Update()
{
    BasePlayerController::Update();

    if (!_clickEffects)
        Start();

    if (!ImGui::GetIO().WantCaptureKeyboard && INPUT->GetButtonDown(KEY_TYPE::F8))
    {
        Protocol::C_TESTMsg command;
        command.set_message("combat-gather-v1");
        NETWORK->SendPacket(ClientPacketHandler::MakeSendBuffer(command, C_TEST_MSG));
        DEBUG_LOG("[CombatTest] F8 gather request sent.");
    }

    int32 mouseX = INPUT->GetMousePos().x;
    int32 mouseY = INPUT->GetMousePos().y;

    float currentTime = TIME->GetGameTime();

    // ???�킬 ?�이�?초기??
    EnsureSkillData();

    if (_playerInfo->hp() <= 0)
    {
        SelectSkill(0);
        SetAttackCursor(false);
        _command = PlayerCommand::None;
        _attackTarget.reset();
        _attackMoveArmed = false;

        return;
    }

    // ??쿨�???갱신
    for (auto& skill : _skillElapsedTime)
    {
        int id = skill.first;
        _skillElapsedTime[id] += TIME->GetDeltaTime();
        _skillElapsedTime[id] = min(_skillElapsedTime[id], skillDataTable[id].cooldown);
    }

    if (UnitStatus::IsStunned(_playerInfo->objectid()))
    {
        SelectSkill(0);
        SetAttackCursor(false);

        return;
    }

    if (IsMovementLocked() && !IsBasicAttacking())
    {
        SelectSkill(0);
        SetAttackCursor(false);

        return;
    }

    if (!Chat::IsOpen() && !ImGui::GetIO().WantCaptureKeyboard && INPUT->GetButtonDown(KEY_TYPE::ENTER))
        Chat::Open(INPUT->GetButton(KEY_TYPE::SHIFT) ? Protocol::CHAT_ALL : Protocol::CHAT_TEAM);
    auto hudForShop = HUDController::Find();

    if (!CUR_SCENE->IsWorldInputBlocked() && !ImGui::GetIO().WantCaptureKeyboard && hudForShop &&
        Keybinds::Down(Keybinds::Action::Shop))
        hudForShop->ToggleShop();
    const bool worldKeyboard = !CUR_SCENE->IsWorldInputBlocked() && !ImGui::GetIO().WantCaptureKeyboard &&
                               !(hudForShop && hudForShop->IsShopOpen());
    bool worldMouse =
        !CUR_SCENE->IsWorldInputBlocked() && !ImGui::GetIO().WantCaptureMouse && !HUDController::IsMouseOverHud();
    const bool ctrl = INPUT->GetButton(KEY_TYPE::CONTROL);

    if (UpdatePings(mouseX, mouseY, worldMouse))
        worldMouse = false;

    if (worldKeyboard && ctrl)
    {
        // Ctrl+Q/W/E/R spends a skill point instead of casting.
        static const KEY_TYPE Keys[4] = {KEY_TYPE::Q, KEY_TYPE::W, KEY_TYPE::E, KEY_TYPE::R};

        for (int32 slot = 0; slot < 4; ++slot)
            if (INPUT->GetButtonDown(Keys[slot]))
                LevelUpSkill(slot + 1);
    }

    if (worldKeyboard)
    {
        if (!ctrl && INPUT->GetButtonDown(KEY_TYPE::Q))
            SelectSkill((int)SkillType::QSpell);

        if (!ctrl && INPUT->GetButtonDown(KEY_TYPE::W))
            SelectSkill((int)SkillType::WSpell);

        if (!ctrl && INPUT->GetButtonDown(KEY_TYPE::E))
            SelectSkill((int)SkillType::ESpell);

        if (!ctrl && INPUT->GetButtonDown(KEY_TYPE::R))
            SelectSkill((int)SkillType::RSpell);

        if (Keybinds::Down(Keybinds::Action::Stop))
            IssueStopCommand();

        if (Keybinds::Down(Keybinds::Action::Recall))
            GameFlow::SendRecall();

        if (Keybinds::Down(Keybinds::Action::Ward))
            PlaceWard(mouseX, mouseY);

        if (!ctrl && INPUT->GetButtonDown(KEY_TYPE::KEY_1))
            UseItem(0, mouseX, mouseY);

        if (!ctrl && INPUT->GetButtonDown(KEY_TYPE::KEY_2))
            UseItem(1, mouseX, mouseY);

        if (!ctrl && INPUT->GetButtonDown(KEY_TYPE::KEY_3))
            UseItem(2, mouseX, mouseY);

        if (!ctrl && INPUT->GetButtonDown(KEY_TYPE::KEY_5))
            UseItem(3, mouseX, mouseY);

        if (!ctrl && INPUT->GetButtonDown(KEY_TYPE::KEY_6))
            UseItem(4, mouseX, mouseY);

        if (!ctrl && INPUT->GetButtonDown(KEY_TYPE::KEY_7))
            UseItem(5, mouseX, mouseY);

        if (INPUT->GetButtonUp(KEY_TYPE::D))
            SummonerSpellsClient::Cast(*this, 0, mouseX, mouseY);

        if (INPUT->GetButtonUp(KEY_TYPE::F))
            SummonerSpellsClient::Cast(*this, 1, mouseX, mouseY);
        UpdateHeldRangeIndicator();

        if (Keybinds::Down(Keybinds::Action::AttackMove))
        {
            SelectSkill(0);
            _attackMoveArmed = true;
        }

        if (Keybinds::QuickCast() && _kitAimSkill != 0)
        {
            const int32 skillId = _kitAimSkill;
            Vec3 point = GetTransform()->GetPosition();
            auto picked = CUR_SCENE->Pick(mouseX, mouseY, point);
            uint64 target = 0;

            if (auto controller = picked ? picked->GetScript<BasePlayerController>() : nullptr;
                controller && controller->_playerInfo)
                target = controller->_playerInfo->objectid();
            CancelKitAim();
            CastKitSkill(skillId, point, target);
        }
    }

    if (INPUT->GetButtonDown(KEY_TYPE::ESCAPE))
    {
        if (_currentSkillID != 0 || _annieWTargeting || _kitAimSkill != 0)
            SelectSkill(0);
        else if (auto hud = HUDController::Find())
            hud->ToggleOptions();
    }

    if (_annieWTargeting && UpdateAnnieWTargeting(mouseX, mouseY))
    {
        UpdateAttackCursor(mouseX, mouseY);

        return;
    }

    if (_kitAimSkill != 0 && UpdateKitAim(mouseX, mouseY))
    {
        UpdateAttackCursor(mouseX, mouseY);

        return;
    }

    UpdateAttackCursor(mouseX, mouseY);

    if (_attackMoveArmed && worldMouse && INPUT->GetButtonDown(KEY_TYPE::LBUTTON))
    {
        _attackMoveArmed = false;
        ResolveAttackMoveClick(mouseX, mouseY);
    }

    // ???�킬 처리
    if (_currentSkillID > 0)
    {
        SkillData skill = GetSkillInfo(_currentSkillID);

        if (_skillElapsedTime[_currentSkillID] < skill.cooldown)
        {
            float remain = skill.cooldown - _skillElapsedTime[_currentSkillID];
            DEBUG_LOG("[Client] ??Skill on Cooldown (" << remain << "s)");
            SelectSkill(0);

            return;
        }

        if (skill.requiresTarget)
        {
            if (!CUR_SCENE->IsWorldInputBlocked() && !ImGui::GetIO().WantCaptureMouse &&
                INPUT->GetButtonDown(KEY_TYPE::LBUTTON))
            {
                auto pickObj = CUR_SCENE->Pick(mouseX, mouseY, _dest);

                if (pickObj && IsEnemy(pickObj))
                {
                    _target = pickObj;

                    // ???�동 중일 경우 ?��? 처리

                    // ???�거�?체크
                    const float skillRange = GetSkillRange(_playerInfo->champtype(), _currentSkillID);
                    Vec3 targetDelta = _target->GetTransform()->GetPosition() - GetTransform()->GetPosition();

                    if (_playerInfo->champtype() == Protocol::PLAYER_TYPE_ANNIE &&
                        _currentSkillID == (int)SkillType::QSpell)
                        targetDelta.y = 0.f;
                    float distance = targetDelta.Length();

                    if (distance > skillRange)
                    {
                        DEBUG_LOG("[Client] ??Target out of range");
                        SelectSkill(0);

                        return;
                    }

                    ProcSkill(_currentSkillID);
                    _skillElapsedTime[_currentSkillID] = 0.f;
                    UI->GetHUD()->GetScript<HUDController>()->TriggerSkillCoolDown(_currentSkillID);
                    SelectSkill(0);
                }
            }
        }
        else
        {
            ProcSkill(_currentSkillID);
            _skillElapsedTime[_currentSkillID] = 0.f;
            UI->GetHUD()->GetScript<HUDController>()->TriggerSkillCoolDown(_currentSkillID);
            SelectSkill(0);
        }
    }

    if (worldMouse && INPUT->GetButtonDown(KEY_TYPE::RBUTTON))
    {
        if (_currentSkillID > 0)
            SelectSkill(0);
        _attackMoveArmed = false;

        if (_annieWPendingMove)
            CancelPendingAnnieW(true);
        Vec3 pickedPosition;
        auto pickObj = CUR_SCENE->Pick(mouseX, mouseY, pickedPosition);

        if (pickObj)
        {
            if (IsEnemy(pickObj))
                IssueAttackCommand(pickObj, true);
            else
                IssueMoveCommand(pickedPosition, false);
        }
    }

    if (_command == PlayerCommand::Attack)
        UpdateAttackCommand();
    else if (_command == PlayerCommand::AttackMove)
        UpdateAttackMoveCommand();

    if (!NETWORK->IsAuthoritativeMoveRequestsEnabled())
        MoveTo();
}

void PlayerController::IssueMoveCommand(const Vec3& destination, bool attackMove)
{
    if (IsInBasicAttackWindup())
        _nextBasicAttackTime = TIME->GetGameTime();
    CancelBasicAttackAnimation();

    _command = PlayerCommand::None;
    _attackTarget.reset();
    _chaseMoving = false;

    if (!RequestMove(destination))
        return;
    _command = attackMove ? PlayerCommand::AttackMove : PlayerCommand::Move;

    if (_clickEffects)
        _clickEffects->PlayAt(attackMove ? ClickEffectPool::Kind::Attack : ClickEffectPool::Kind::Move, destination);
    PlayWalkVoice();
}

void PlayerController::IssueAttackCommand(shared_ptr<GameObject> target, bool showClickEffect)
{
    if (!IsEnemy(target))
        return;

    if (_command != PlayerCommand::Attack || _attackTarget.lock() != target)
    {
        _chaseMoving = IsAuthoritativeMoving() || _legacyMoveActive;
        _lastChaseRequestTime = -999.f;
    }

    if (showClickEffect && (_command != PlayerCommand::Attack || _attackTarget.lock() != target))
        GameSounds::Order(*this, true);
    _command = PlayerCommand::Attack;
    _attackTarget = target;
    _target = target;

    if (showClickEffect && _clickEffects)
        _clickEffects->PlayOnTarget(target);
}

void PlayerController::IssueStopCommand()
{
    if (IsInBasicAttackWindup())
        _nextBasicAttackTime = TIME->GetGameTime();
    CancelBasicAttackAnimation();
    _command = PlayerCommand::None;
    _attackTarget.reset();
    _attackMoveArmed = false;
    _chaseMoving = false;
    SendStopCommand();
}

void PlayerController::InterruptForCrowdControl()
{
    CancelBasicAttackAnimation();
    SelectSkill(0);
    SetAttackCursor(false);
    _command = PlayerCommand::None;
    _attackTarget.reset();
    _attackMoveArmed = false;
    _chaseMoving = false;
}

void PlayerController::ResolveAttackMoveClick(int32 mouseX, int32 mouseY)
{
    Vec3 pickedPosition;
    auto pickObj = CUR_SCENE->Pick(mouseX, mouseY, pickedPosition);

    if (!pickObj)
        return;

    if (IsEnemy(pickObj))
    {
        IssueAttackCommand(pickObj, true);

        return;
    }

    if (auto enemy = FindNearestEnemy(pickedPosition, AttackMoveClickRadius))
    {
        IssueAttackCommand(enemy, true);

        return;
    }

    IssueMoveCommand(pickedPosition, true);
}

void PlayerController::UpdateAttackCommand()
{
    auto target = _attackTarget.lock();

    if (!IsEnemy(target))
    {
        _command = PlayerCommand::None;
        _attackTarget.reset();

        return;
    }

    if (IsActionBusy())
        return;

    const ChampionCombatStats& stats = GetChampionCombatStats(_playerInfo->champtype());
    const float now = TIME->GetGameTime();
    const Vec3 targetPosition = target->GetTransform()->GetPosition();

    if (DistanceXZ(targetPosition, GetTransform()->GetPosition()) <= stats.attackRange + TargetRadius(target))
    {
        if (now < _nextBasicAttackTime)
        {
            if (_chaseMoving)
            {
                SendStopCommand();
                _chaseMoving = false;
            }

            return;
        }

        _chaseMoving = false;
        _target = target;

        _nextBasicAttackTime =
            now + stats.AttackCycle() / (1.f + UnitStatus::AttackSpeedBonus(_playerInfo->objectid()));
        ProcSkill((int32)SkillType::GeneralAtk);

        return;
    }

    if (IsMovementLocked() || now - _lastChaseRequestTime < ChaseRepathInterval)
        return;

    if (_chaseMoving && DistanceXZ(targetPosition, _lastChaseDestination) < ChaseRepathDistance)
        return;

    Vec3 destination = targetPosition;

    if (const float radius = TargetRadius(target); radius > 0.f)
    {
        Vec3 away = GetTransform()->GetPosition() - targetPosition;
        away.y = 0.f;

        if (away.LengthSquared() > 0.0001f)
        {
            away.Normalize();
            destination = targetPosition + away * (radius + stats.attackRange * 0.5f);
        }
    }

    if (!RequestMove(destination))
        return;
    _chaseMoving = true;
    _lastChaseRequestTime = now;
    _lastChaseDestination = targetPosition;
}

void PlayerController::UpdateAttackMoveCommand()
{
    const ChampionCombatStats& stats = GetChampionCombatStats(_playerInfo->champtype());

    if (auto enemy = FindNearestEnemy(GetTransform()->GetPosition(), stats.attackRange))
        IssueAttackCommand(enemy, false);
}

bool PlayerController::RequestMove(const Vec3& destination)
{
    if (NETWORK->IsAuthoritativeMoveRequestsEnabled())
        return SendAuthoritativeMoveRequest(destination);

    if (IsMovementLocked())
        return false;
    _dest = _correctPosition = destination;
    _legacyMoveActive = true;

    if (!_isAttackMode && _currentState != PlayerState::RUN)
    {
        _currentState = PlayerState::RUN;
        GetGameObject()->GetModelAnimator()->SetAnimation((int32)PlayerState::RUN, true);
    }

    Protocol::C_Move movePacket;
    movePacket.set_objectid(GAMEMANAGER->_myPlayer->_playerInfo->objectid());
    movePacket.mutable_targetpos()->set_x(_dest.x);
    movePacket.mutable_targetpos()->set_y(_dest.y);
    movePacket.mutable_targetpos()->set_z(_dest.z);
    movePacket.mutable_cellpos()->set_x(_correctPosition.x);
    movePacket.mutable_cellpos()->set_z(_correctPosition.z);
    NETWORK->SendPacket(ClientPacketHandler::MakeSendBuffer(movePacket, C_MOVE));

    return true;
}

void PlayerController::SendStopCommand()
{
    if (NETWORK->IsAuthoritativeMoveRequestsEnabled())
    {
        Protocol::C_StopCommand stop;
        NETWORK->SendPacket(ClientPacketHandler::MakeSendBuffer(stop, C_STOP_COMMAND));

        return;
    }

    _legacyMoveActive = false;
    _dest = _correctPosition = GetTransform()->GetPosition();

    if (!_isAttackMode && _currentState != PlayerState::IDLE)
    {
        _currentState = PlayerState::IDLE;
        GetGameObject()->GetModelAnimator()->SetAnimation((int32)PlayerState::IDLE, true);
    }
}

void PlayerController::PlayWalkVoice()
{
    const float currentTime = TIME->GetGameTime();

    if (currentTime - _lastWalkSoundTime <= WALK_SOUND_COOLDOWN)
        return;

    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(1, 4);
    _lastWalkSoundTime = currentTime;
    GameSounds::Order(*this, false);
}

shared_ptr<GameObject> PlayerController::FindNearestEnemy(const Vec3& point, float radius)
{
    shared_ptr<GameObject> nearest;
    float nearestDistance = radius;

    for (auto& [objectId, object] : CUR_SCENE->_players)
    {
        if (!IsEnemy(object))
            continue;
        const float distance = DistanceXZ(object->GetTransform()->GetPosition(), point) - TargetRadius(object);

        if (distance <= nearestDistance)
        {
            nearestDistance = distance;
            nearest = object;
        }
    }

    return nearest;
}

unordered_map<int, float>& PlayerController::GetCooldowns()
{
    return _skillElapsedTime;
}

void PlayerController::MoveTo()
{
    // 공격 중이�??�동 금�?
    if (IsMovementLocked())
        return;

    // IDLE ?�태�????�상 ?�동?��? ?�음
    if (!_legacyMoveActive)
        return;

    if (!_isAttackMode && _currentState != PlayerState::RUN)
    {
        _currentState = PlayerState::RUN;
        GetGameObject()->GetModelAnimator()->SetAnimation((int32)PlayerState::RUN, true);
    }

    Vec3 currentPosition = GetTransform()->GetPosition();
    direction = _dest - currentPosition;
    direction.y = 0.0f;

    float distance = direction.Length();

    if (distance >= 0.05f)
    {
        direction.Normalize();
        float angle = atan2f(direction.x, direction.z) + XM_PI;
        GetTransform()->SetRotation(Vec3(XMConvertToRadians(90.f), angle, 0.0f));

        const float speed = _playerInfo ? GetChampionData(_playerInfo->champtype()).MoveSpeedUnits() : _speed;
        Vec3 newPosition = currentPosition + direction * speed * TIME->GetDeltaTime();
        newPosition.y = 1.6f;
        GetTransform()->SetPosition(newPosition);
    }
    else
    {
        _legacyMoveActive = false;

        if (_isAttackMode)
            return;
        _currentState = PlayerState::IDLE;
        GetGameObject()->GetModelAnimator()->SetAnimation((int32)PlayerState::IDLE, true);
        DEBUG_LOG("[Client] Stopped Moving -> IDLE");
    }
}

bool PlayerController::IsEnemy(shared_ptr<GameObject> obj)
{
    if (!obj || !_playerInfo)
        return false;
    auto other = obj->GetScript<OtherPlayerController>();

    return other && other->_playerInfo && other->_playerInfo->hp() > 0 &&
           other->_playerInfo->teamid() != _playerInfo->teamid();
}

void PlayerController::ProcSkill(int32 skillId)
{
    // ???�식 ?�래?�에??구현
}

bool PlayerController::HasArrivedAtDestination()
{
    Vec3 currentPos = GetTransform()->GetPosition();
    Vec3 direction = _dest - currentPos;
    direction.y = 0.0f;

    return direction.LengthSquared() < 0.05f * 0.05f;
}

bool PlayerController::SendAuthoritativeMoveRequest(const Vec3& requestedDestination)
{
    if (IsMovementLocked() || !_playerInfo || _playerInfo->hp() <= 0)
        return false;

    if (!NETWORK->HasNavigationInfo())
    {
        DEBUG_LOG("[Movement] MoveRequest blocked: NavigationInfo has not arrived.");

        return false;
    }

    uint32 sequence = 0;

    if (!NETWORK->TryGetNextMoveSequence(sequence))
    {
        DEBUG_LOG("[Movement] MoveRequest blocked: sequence exhausted or NavigationInfo unavailable.");

        return false;
    }

    Protocol::C_MoveRequest request;
    request.set_clientmovesequence(sequence);
    request.set_requesteddestinationx(requestedDestination.x);
    request.set_requesteddestinationz(requestedDestination.z);
    request.set_navigationmapid(NETWORK->GetNavigationMapId());
    request.set_navigationcontenthash(NETWORK->GetNavigationContentHash());
    NETWORK->SendPacket(ClientPacketHandler::MakeSendBuffer(request, C_MOVE_REQUEST));
    DEBUG_LOG("[Movement] MoveRequest sent: sequence=" << sequence << " destination=(" << requestedDestination.x << ", "
                                                       << requestedDestination.z << ")");

    return true;
}

void PlayerController::OnMoveAccepted(uint64 serverMoveId, const Vec3& acceptedDestination, bool wasAdjusted)
{
    SetMinimumAuthoritativeMoveId(serverMoveId);

    if (IsMovementLocked())
        return;
    _dest = acceptedDestination;
    _correctPosition = acceptedDestination;

    if (_annieWPendingMove)
        _annieWMoveId = serverMoveId;

    if (_clickEffects && (_command == PlayerCommand::Move || _command == PlayerCommand::AttackMove))
        _clickEffects->MoveLastMoveEffect(acceptedDestination);
    DEBUG_LOG("[Movement] MoveAccepted destination=(" << acceptedDestination.x << ", " << acceptedDestination.z
                                                      << ") adjusted=" << wasAdjusted);
}

void PlayerController::OnMoveRejected(const Vec3& serverPosition, Protocol::MOVE_REJECT_REASON reason)
{
    if (_annieWPendingMove)
        CancelPendingAnnieW(true);
    _dest = serverPosition;
    _correctPosition = serverPosition;
    _chaseMoving = false;

    if (_command == PlayerCommand::Move || _command == PlayerCommand::AttackMove)
        _command = PlayerCommand::None;

    if (!_isAttackMode)
    {
        _currentState = PlayerState::IDLE;
        GetGameObject()->GetModelAnimator()->SetAnimation((int32)PlayerState::IDLE, true);
    }

    DEBUG_LOG("[Movement] MoveRejected reason=" << static_cast<int>(reason) << " serverPosition=(" << serverPosition.x
                                                << ", " << serverPosition.z << ")");
}

void PlayerController::OnMovementSnapshotState(Protocol::MOVEMENT_SNAPSHOT_STATE state)
{
    if (_annieWPendingMove)
    {
        if (state == Protocol::MOVEMENT_SNAPSHOT_STATE_ARRIVED && GetLatestAuthoritativeMoveId() == _annieWMoveId)
            TryCastPendingAnnieW();
        else if (state == Protocol::MOVEMENT_SNAPSHOT_STATE_CANCELLED &&
                 GetLatestAuthoritativeMoveId() == _annieWMoveId)
            CancelPendingAnnieW(true);
    }

    if (state != Protocol::MOVEMENT_SNAPSHOT_STATE_ARRIVED && state != Protocol::MOVEMENT_SNAPSHOT_STATE_CANCELLED)
        return;

    _chaseMoving = false;

    if (_command == PlayerCommand::Move || _command == PlayerCommand::AttackMove)
        _command = PlayerCommand::None;
}

void PlayerController::ConfirmSkillCooldown(int32 skillId)
{
    auto elapsed = _skillElapsedTime.find(skillId);
    auto data = skillDataTable.find(skillId);

    if (elapsed == _skillElapsedTime.end() || data == skillDataTable.end())
        return;
    elapsed->second = 0.f;

    if (UI->GetHUD() && UI->GetHUD()->GetScript<HUDController>())
        UI->GetHUD()->GetScript<HUDController>()->TriggerSkillCoolDown(skillId);
}

void PlayerController::ApplyServerCooldown(int32 skillId, float duration)
{
    EnsureSkillData();
    auto data = skillDataTable.find(skillId);

    if (data == skillDataTable.end() || !std::isfinite(duration) || duration <= 0.f)
        return;
    data->second.cooldown = duration;
    _skillElapsedTime[skillId] = 0.f;
}

void PlayerController::OnSkillRejected(int32 skillId, Protocol::SKILL_REJECT_REASON reason)
{
    auto data = skillDataTable.find(skillId);

    if (data == skillDataTable.end() || reason == Protocol::SKILL_REJECT_COOLDOWN)
        return;
    _skillElapsedTime[skillId] = data->second.cooldown;
}

float PlayerController::GetCooldownRemaining(int32 skillId) const
{
    auto data = skillDataTable.find(skillId);
    auto elapsed = _skillElapsedTime.find(skillId);

    if (data == skillDataTable.end() || elapsed == _skillElapsedTime.end())
        return 0.f;

    return max(0.f, data->second.cooldown - elapsed->second);
}

float PlayerController::GetCooldownDuration(int32 skillId) const
{
    auto data = skillDataTable.find(skillId);

    return data == skillDataTable.end() ? 0.f : data->second.cooldown;
}

float PlayerController::GetManaCost(int32 skillId) const
{
    if (!_playerInfo || skillId < (int32)SkillType::QSpell || skillId > (int32)SkillType::RSpell)
        return 0.f;
    const int32 rank = UnitStatus::MyProgress().ranks[skillId - 1];

    return GetChampionData(_playerInfo->champtype()).Spell(skillId).ManaCostAt(rank);
}

void PlayerController::EnsureSkillData()
{
    if (!_playerInfo || !skillDataTable.empty())
        return;

    for (const SkillData& skill : GetChampionSkills(_playerInfo->champtype()))
    {
        skillDataTable[skill.SkillId] = skill;
        _skillElapsedTime[skill.SkillId] = skill.cooldown;
    }
}

void PlayerController::SelectSkill(int32 skillId)
{
    if (!_playerInfo || skillId < 0 || skillId > (int32)SkillType::RSpell)
        return;
    EnsureSkillData();
    CancelPendingAnnieW(true);
    CancelKitAim();
    _currentSkillID = 0;

    if (skillId != 0)
        _attackMoveArmed = false;

    if (skillId == 0 || IsActionBusy())
        return;

    if (UnitStatus::MyProgress().ranks[skillId - 1] <= 0)
    {
        if (auto hud = HUDController::Find())
            hud->ShowRejection(Protocol::SKILL_REJECT_NOT_LEARNED);

        return;
    }

    if (UnitStatus::IsSilenced(_playerInfo->objectid()) || UnitStatus::IsStunned(_playerInfo->objectid()) ||
        UnitStatus::IsCharmed(_playerInfo->objectid()))
        return;
    const auto data = skillDataTable.find(skillId);

    if (data == skillDataTable.end() || _skillElapsedTime[skillId] < data->second.cooldown)
        return;

    if (GetManaCost(skillId) > static_cast<float>(_playerInfo->mp()))
        return;

    if (const ChampionKit* kit = FindKit(_playerInfo->champtype()); kit && kit->serverCast)
    {
        if (_playerInfo->hp() <= 0)
            return;
        // Ashe's Ranger's Focus opens at four Focus stacks.
        if (kit->type == Protocol::PLAYER_TYPE_ASHE && skillId == (int32)SkillType::QSpell)
            if (const UnitStatus::State* me = UnitStatus::Find(_playerInfo->objectid()); !me || me->asheFocus < 4)
                return;

        if (kit->skills[skillId - 1].aim == SkillAim::None)
            CastKitSkill(skillId, GetTransform()->GetPosition());
        else
            BeginKitAim(skillId);

        return;
    }

    const bool annie = _playerInfo->champtype() == Protocol::PLAYER_TYPE_ANNIE;

    if (annie && _playerInfo->hp() <= 0)
        return;

    if (annie && skillId == (int32)SkillType::WSpell)
    {
        BeginAnnieWTargeting();

        return;
    }

    _currentSkillID = skillId;

    if (annie && skillId == (int32)SkillType::QSpell)
        if (auto object = UI->GetSkillIndicatorController())
            if (auto indicator = object->GetScript<SkillIndicatorController>())
                indicator->ShowCircle(GetGameObject(), data->second.range);
}

void PlayerController::BeginKitAim(int32 skillId)
{
    const ChampionKit* kit = _playerInfo ? FindKit(_playerInfo->champtype()) : nullptr;
    auto object = UI->GetSkillIndicatorController();
    auto indicator = object ? object->GetScript<SkillIndicatorController>() : nullptr;

    if (!kit || !indicator)
        return;
    _kitAimSkill = skillId;
    const KitSkill& skill = kit->skills[skillId - 1];
    _kitAimPoint = GetTransform()->GetPosition() + GetTransform()->GetLook() * skill.range;
    indicator->SetAimPosition(_kitAimPoint);

    if (skill.aim == SkillAim::Unit)
        indicator->ShowCircle(GetGameObject(), skill.range);
    else if (skill.coneDegrees > 0.f)
        indicator->Show(GetGameObject(), skill.range, skill.coneDegrees);
    else
        indicator->ShowLine(GetGameObject(), skill.range, max(0.2f, skill.width));
}

void PlayerController::CancelKitAim()
{
    if (_kitAimSkill == 0)
        return;
    _kitAimSkill = 0;

    if (auto object = UI->GetSkillIndicatorController())
        if (auto indicator = object->GetScript<SkillIndicatorController>())
            indicator->Hide();
}

bool PlayerController::UpdateKitAim(int32 mouseX, int32 mouseY)
{
    const ChampionKit* kit = _playerInfo ? FindKit(_playerInfo->champtype()) : nullptr;

    if (!kit || _kitAimSkill == 0)
        return false;

    if (INPUT->GetButtonDown(KEY_TYPE::ESCAPE) || INPUT->GetButtonDown(KEY_TYPE::RBUTTON))
    {
        CancelKitAim();

        return true;
    }

    const KitSkill& skill = kit->skills[_kitAimSkill - 1];
    Vec3 picked = _kitAimPoint;
    CUR_SCENE->Pick(mouseX, mouseY, picked);
    _kitAimPoint = picked;

    if (auto object = UI->GetSkillIndicatorController())
        if (auto indicator = object->GetScript<SkillIndicatorController>())
        {
            indicator->SetAimPosition(picked);

            if (skill.aim == SkillAim::Point)
            {
                Vec3 delta = picked - GetTransform()->GetPosition();
                delta.y = 0.f;
                const float length = std::clamp(delta.Length(), 0.5f, skill.range);

                if (fabsf(indicator->GetRange() - length) > 0.05f)
                    indicator->ShowLine(GetGameObject(), length, max(0.2f, skill.width));
            }
        }
    const bool worldMouse =
        !CUR_SCENE->IsWorldInputBlocked() && !ImGui::GetIO().WantCaptureMouse && !HUDController::IsMouseOverHud();

    if (worldMouse && INPUT->GetButtonDown(KEY_TYPE::LBUTTON))
    {
        const int32 skillId = _kitAimSkill;

        if (skill.aim == SkillAim::Unit)
        {
            Vec3 ground = picked;
            auto unit = CUR_SCENE->Pick(mouseX, mouseY, ground);
            auto unitController = unit ? unit->GetScript<BasePlayerController>() : nullptr;
            const bool valid = unitController && unitController->_playerInfo && unit != GetGameObject() &&
                               unitController->_playerInfo->hp() > 0 && (skill.anyUnit || IsEnemy(unit));

            if (valid)
            {
                Vec3 delta = unit->GetTransform()->GetPosition() - GetTransform()->GetPosition();
                delta.y = 0.f;

                if (delta.Length() > skill.range + 0.65f)
                    return true;
                CancelKitAim();
                CastKitSkill(skillId, unit->GetTransform()->GetPosition(), unitController->_playerInfo->objectid());
            }
            else if (skill.anyUnit)
            {
                CancelKitAim();
                CastKitSkill(skillId, picked);
            }

            return true;
        }

        CancelKitAim();
        CastKitSkill(skillId, picked);
    }

    return true;
}

void PlayerController::CastKitSkill(int32 skillId, const Vec3& point, uint64 targetId)
{
    if (!_playerInfo)
        return;
    Protocol::C_SkillCast packet;
    packet.set_casterid(_playerInfo->objectid());
    packet.set_skillid(skillId);

    if (targetId != 0)
        packet.set_targetid(targetId);
    packet.mutable_targetpos()->set_x(point.x);
    packet.mutable_targetpos()->set_y(point.y);
    packet.mutable_targetpos()->set_z(point.z);
    NETWORK->SendPacket(ClientPacketHandler::MakeSendBuffer(packet, C_SKILL_CAST));
    _skillElapsedTime[skillId] = 0.f;
}

void PlayerController::LevelUpSkill(int32 skillId)
{
    auto& progress = UnitStatus::MyProgress();

    if (skillId < 1 || skillId > 4 || progress.skillPoints <= 0)
        return;
    Protocol::C_LevelUpSkill packet;
    packet.set_skillid(skillId);
    NETWORK->SendPacket(ClientPacketHandler::MakeSendBuffer(packet, C_LEVEL_UP_SKILL));
    SOUND->PlayCue("Play_sfx_hud_base_spell_levelup");
}

bool PlayerController::UpdatePings(int32 mouseX, int32 mouseY, bool worldMouse)
{
    Pings::Wheel& wheel = Pings::GetWheel();
    const bool pingKey = Keybinds::Held(Keybinds::Action::Ping) || INPUT->GetButton(KEY_TYPE::ALT);
    const bool retreatKey = Keybinds::Held(Keybinds::Action::PingRetreat);
    const Vec2 mouse(static_cast<float>(mouseX), static_cast<float>(mouseY));

    if (wheel.open)
    {
        const Vec2 delta = mouse - wheel.center;
        const float distance = delta.Length();
        wheel.hovered = -1;

        if (distance > 24.f * GAME->GetGameDesc().height / 1080.f)
        {
            float angle = std::atan2(delta.x, -delta.y);

            if (angle < 0.f)
                angle += XM_2PI;
            wheel.hovered =
                static_cast<int32>(std::floor((angle + XM_PI / Pings::WheelSectors) / XM_2PI * Pings::WheelSectors)) %
                Pings::WheelSectors;
        }

        if (INPUT->GetButtonUp(KEY_TYPE::LBUTTON) || !INPUT->GetButton(KEY_TYPE::LBUTTON))
        {
            Pings::Send(wheel.hovered < 0 ? Protocol::PING_GENERIC : Pings::SectorType(wheel.hovered), _pingPoint,
                        _pingTarget);
            wheel.open = false;
        }

        return true;
    }

    if (!(pingKey || retreatKey) || !worldMouse)
        return pingKey || retreatKey;

    if (INPUT->GetButtonDown(KEY_TYPE::LBUTTON))
    {
        Vec3 point = GetTransform()->GetPosition();
        auto picked = CUR_SCENE->Pick(mouseX, mouseY, point);
        uint64 target = 0;

        if (auto controller = picked ? picked->GetScript<BasePlayerController>() : nullptr;
            controller && controller->_playerInfo)
        {
            target = controller->_playerInfo->objectid();
            point = picked->GetTransform()->GetPosition();
        }

        if (retreatKey || INPUT->GetButton(KEY_TYPE::ALT))
        {
            Pings::Send(retreatKey ? Protocol::PING_RETREAT : Protocol::PING_GENERIC, point, target);

            return true;
        }

        // G: the wheel opens where the mouse went down.
        wheel.open = true;
        wheel.center = mouse;
        wheel.hovered = -1;
        _pingAnchor = mouse;
        _pingPoint = point;
        _pingTarget = target;
    }

    return true;
}

void PlayerController::UseItem(int32 slot, int32 mouseX, int32 mouseY)
{
    if (slot < 0 || slot >= 6)
        return;
    const auto& item = UnitStatus::MyProgress().items[slot];
    const Items::Item* data = Items::Find(item.itemId);

    if (!data || !data->active)
        return;

    if (item.cooldownAt > TIME->GetGameTime())
    {
        if (auto hud = HUDController::Find())
            hud->ShowRejection(Protocol::SKILL_REJECT_COOLDOWN);

        return;
    }

    Protocol::C_ItemAction packet;
    packet.set_action(Protocol::ITEM_ACTION_USE);
    packet.set_slot(slot);
    Vec3 point = GetTransform()->GetPosition();
    auto picked = CUR_SCENE->Pick(mouseX, mouseY, point);

    if (auto controller = picked ? picked->GetScript<BasePlayerController>() : nullptr;
        controller && controller->_playerInfo)
    {
        packet.set_targetid(controller->_playerInfo->objectid());
        point = picked->GetTransform()->GetPosition();
    }

    packet.mutable_position()->set_x(point.x);
    packet.mutable_position()->set_y(point.y);
    packet.mutable_position()->set_z(point.z);
    NETWORK->SendPacket(ClientPacketHandler::MakeSendBuffer(packet, C_ITEM_ACTION));
}

void PlayerController::PlaceWard(int32 mouseX, int32 mouseY)
{
    Vec3 point = GetTransform()->GetPosition();

    if (!CUR_SCENE->Pick(mouseX, mouseY, point))
        return;
    Protocol::C_PlaceWard packet;
    packet.mutable_position()->set_x(point.x);
    packet.mutable_position()->set_y(point.y);
    packet.mutable_position()->set_z(point.z);
    NETWORK->SendPacket(ClientPacketHandler::MakeSendBuffer(packet, C_PLACE_WARD));
}

void PlayerController::UpdateHeldRangeIndicator()
{
    auto object = UI->GetSkillIndicatorController();
    auto indicator = object ? object->GetScript<SkillIndicatorController>() : nullptr;

    if (!indicator || !_playerInfo)
        return;

    if (_annieWTargeting || _currentSkillID != 0 || _kitAimSkill != 0)
    {
        _heldIndicator = false;

        return;
    }

    float range = 0.f;
    const KEY_TYPE skillKeys[4] = {KEY_TYPE::Q, KEY_TYPE::W, KEY_TYPE::E, KEY_TYPE::R};

    for (int32 i = 0; i < 4 && range <= 0.f; ++i)
        if (INPUT->GetButton(skillKeys[i]))
        {
            const auto& spell = GetChampionData(_playerInfo->champtype()).Spell(i + 1);
            const float cast = spell.CastRangeUnits();
            range = cast > 0.f && cast < 50.f ? cast : spell.CastRadiusUnits();
        }
    const KEY_TYPE summonerKeys[2] = {KEY_TYPE::D, KEY_TYPE::F};

    for (int32 slot = 0; slot < 2 && range <= 0.f; ++slot)
        if (INPUT->GetButton(summonerKeys[slot]) && _playerInfo->summonerspells_size() > slot)
            if (const auto* info = SummonerSpellsClient::Find(
                    static_cast<Protocol::SUMMONER_SPELL>(_playerInfo->summonerspells(slot))))
                range = info->indicatorRange;

    if (range > 0.f)
    {
        if (!_heldIndicator || fabsf(indicator->GetRange() - range) > 0.001f || !indicator->IsVisible())
            indicator->ShowCircle(GetGameObject(), range);
        _heldIndicator = true;
    }
    else if (_heldIndicator)
    {
        indicator->Hide();
        _heldIndicator = false;
    }
}

void PlayerController::BeginAnnieWTargeting()
{
    const int32 skillId = (int32)SkillType::WSpell;
    auto elapsed = _skillElapsedTime.find(skillId);
    auto data = skillDataTable.find(skillId);

    if (elapsed == _skillElapsedTime.end() || data == skillDataTable.end() || elapsed->second < data->second.cooldown)
    {
        DEBUG_LOG("[AnnieW] Targeting blocked by cooldown or missing skill data.");

        return;
    }

    CancelPendingAnnieW(false);
    _currentSkillID = 0;
    _annieWTargeting = true;
    _annieWTargetPosition = GetTransform()->GetPosition() + GetTransform()->GetLook() * AnnieWCastRange;
    auto indicatorObject = UI->GetSkillIndicatorController();

    if (indicatorObject)
    {
        auto indicator = indicatorObject->GetScript<SkillIndicatorController>();

        if (indicator)
        {
            indicator->SetAimPosition(_annieWTargetPosition);
            indicator->Show(GetGameObject(), AnnieWCastRange, AnnieWConeAngleDegrees);
        }
    }
}

bool PlayerController::UpdateAnnieWTargeting(int32 mouseX, int32 mouseY)
{
    if (!_annieWTargeting)
        return false;

    if (INPUT->GetButtonDown(KEY_TYPE::ESCAPE) || INPUT->GetButtonDown(KEY_TYPE::RBUTTON))
    {
        CancelPendingAnnieW(true);

        return true;
    }

    Vec3 pickedPosition = _annieWTargetPosition;
    auto pickedObject = CUR_SCENE->Pick(mouseX, mouseY, pickedPosition);

    if (pickedObject)
    {
        _annieWTargetPosition = pickedPosition;
        auto indicatorObject = UI->GetSkillIndicatorController();

        if (indicatorObject)
        {
            auto indicator = indicatorObject->GetScript<SkillIndicatorController>();

            if (indicator)
                indicator->SetAimPosition(_annieWTargetPosition);
        }
    }

    if (!CUR_SCENE->IsWorldInputBlocked() && !ImGui::GetIO().WantCaptureMouse && pickedObject &&
        INPUT->GetButtonDown(KEY_TYPE::LBUTTON))
        ConfirmAnnieWTarget(_annieWTargetPosition);

    return true;
}

void PlayerController::ConfirmAnnieWTarget(const Vec3& targetPosition)
{
    if (IsActionBusy())
        return;
    Vec3 delta = targetPosition - GetTransform()->GetPosition();
    delta.y = 0.f;
    const float distance = delta.Length();

    if (!std::isfinite(distance) || distance <= 0.01f)
    {
        CancelPendingAnnieW(true);

        return;
    }

    _annieWTargeting = false;
    _annieWTargetPosition = targetPosition;

    if (distance <= AnnieWCastRange)
    {
        AnnieWSpell spell;
        spell.UseAt(GetGameObject(), _annieWTargetPosition);
        CancelPendingAnnieW(true);

        return;
    }

    delta /= distance;
    const Vec3 approach = targetPosition - delta * (AnnieWCastRange - AnnieWApproachMargin);
    _command = PlayerCommand::None;
    _annieWPendingMove = true;
    _annieWMoveId = 0;

    if (!SendAuthoritativeMoveRequest(approach))
        CancelPendingAnnieW(true);
}

void PlayerController::TryCastPendingAnnieW()
{
    if (!_annieWPendingMove || IsActionBusy())
        return;

    Vec3 delta = _annieWTargetPosition - GetTransform()->GetPosition();
    delta.y = 0.f;
    const float distance = delta.Length();

    if (!std::isfinite(distance) || distance > AnnieWCastRange + 0.05f || distance <= 0.01f)
    {
        DEBUG_LOG("[AnnieW] Move arrived outside cast range; pending cast cancelled.");
        CancelPendingAnnieW(true);

        return;
    }

    AnnieWSpell spell;
    spell.UseAt(GetGameObject(), _annieWTargetPosition);
    CancelPendingAnnieW(true);
}

void PlayerController::CancelPendingAnnieW(bool hideIndicator)
{
    _annieWTargeting = false;
    _annieWPendingMove = false;
    _annieWMoveId = 0;

    if (!hideIndicator)
        return;
    auto indicatorObject = UI->GetSkillIndicatorController();

    if (!indicatorObject)
        return;
    auto indicator = indicatorObject->GetScript<SkillIndicatorController>();

    if (indicator)
        indicator->Hide();
}

void PlayerController::UpdateAttackCursor(int32 mouseX, int32 mouseY)
{
    if (CUR_SCENE->IsWorldInputBlocked() || ImGui::GetIO().WantCaptureMouse || _annieWTargeting || _annieWPendingMove)
    {
        SetAttackCursor(false);

        return;
    }

    auto hovered = CUR_SCENE->Pick(mouseX, mouseY);
    const bool enemyHovered = IsEnemy(hovered);
    auto hoveredController = hovered ? hovered->GetScript<BasePlayerController>() : nullptr;
    const bool allyHovered = !enemyHovered && hoveredController && hoveredController.get() != this &&
                             hoveredController->_playerInfo && _playerInfo &&
                             hoveredController->_playerInfo->teamid() == _playerInfo->teamid();
    GameCursor::Kind kind = GameCursor::Kind::Normal;

    if (_attackMoveArmed || _currentSkillID != 0)
        kind = enemyHovered  ? GameCursor::Kind::TargetEnemy
               : allyHovered ? GameCursor::Kind::TargetAlly
                             : GameCursor::Kind::Target;
    else if (enemyHovered)
        kind = GameCursor::Kind::HoverEnemy;
    else if (allyHovered)
        kind = GameCursor::Kind::HoverFriendly;
    GameCursor::Set(kind);
    _attackCursorEnabled = kind != GameCursor::Kind::Normal;
}

void PlayerController::SetAttackCursor(bool enabled)
{
    if (_attackCursorEnabled == enabled)
        return;
    GameCursor::Set(enabled ? GameCursor::Kind::TargetEnemy : GameCursor::Kind::Normal);
    _attackCursorEnabled = enabled;
}
