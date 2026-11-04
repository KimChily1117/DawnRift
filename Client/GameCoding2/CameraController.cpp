#include "pch.h"
#include "CameraController.h"
#include "UnitStatus.h"
#include "BasePlayerController.h"
#include "Keybinds.h"
#include "Camera.h"
#include "GroundHeight.h"

void CameraController::Awake()
{
    if (auto camera = GetGameObject()->GetCamera())
        camera->SetFOV(XMConvertToRadians(LolFieldOfViewDegrees));
}

shared_ptr<CameraController> CameraController::Find()
{
    auto camera = CUR_SCENE ? CUR_SCENE->GetMainCamera() : nullptr;

    return camera ? camera->GetScript<CameraController>() : nullptr;
}

void CameraController::SetLocked(bool locked)
{
    _locked = locked;
}

void CameraController::Peek(const Vec3& point)
{
    _peeking = true;
    _peek = point;

    if (!_locked)
        _focus = point;
}

void CameraController::Update()
{
    if (!_target)
        return;
    UpdateDeathView();

    if (!ImGui::GetIO().WantCaptureKeyboard && Keybinds::Down(Keybinds::Action::CameraLock))
        ToggleLocked();

    _zoom = FilmZoom > 0.f ? FilmZoom
                           : std::clamp(_zoom - INPUT->GetWheel() * ZoomStep, LolMinZoomDistance, LolZoomDistance);
    _shownZoom += (_zoom - _shownZoom) * std::clamp(TIME->GetDeltaTime() * 10.f, 0.f, 1.f);
    const float pitch = XMConvertToRadians(FilmPitchDegrees > 0.f ? FilmPitchDegrees : LolPitchDegrees);
    const float yaw = XMConvertToRadians(FilmYawDegrees);

    const Vec3 champion = _target->GetTransform()->GetPosition();
    const bool holdOnChampion = !ImGui::GetIO().WantCaptureKeyboard && INPUT->GetButton(KEY_TYPE::SPACE);

    if (_focus == Vec3::Zero || (_locked && !_peeking) || holdOnChampion)
        _focus = champion;
    else if (!_locked && !_peeking)
    {
        // Free camera: the screen's edges pan it, faster zoomed out.
        const POINT mouse = INPUT->GetMousePos();
        const auto& desc = GAME->GetGameDesc();
        const float step = EdgePanSpeed * (_shownZoom / LolZoomDistance) * TIME->GetDeltaTime();

        if (::GetForegroundWindow() == desc.hWnd)
        {
            if (mouse.x <= EdgePanPixels)
                _focus.x -= step;

            if (mouse.x >= static_cast<int32>(desc.width) - EdgePanPixels)
                _focus.x += step;

            if (mouse.y <= EdgePanPixels)
                _focus.z += step;

            if (mouse.y >= static_cast<int32>(desc.height) - EdgePanPixels)
                _focus.z -= step;
        }

        _focus.x = std::clamp(_focus.x, 0.f, 147.f);
        _focus.z = std::clamp(_focus.z, 0.f, 147.f);
    }

    Vec3 focus = _peeking && !holdOnChampion ? _peek : _focus;
    _peeking = false;

    if (GroundHeight::IsActive())
        focus.y = GroundHeight::At(focus.x, focus.z);
    const Vec3 offset(-sinf(yaw) * cosf(pitch) * _shownZoom, sinf(pitch) * _shownZoom,
                      -cosf(yaw) * cosf(pitch) * _shownZoom);
    GetTransform()->SetRotation(Vec3(pitch, yaw, 0.f));
    GetTransform()->SetPosition(focus + offset);
}

void CameraController::UpdateDeathView()
{
    auto me = GAMEMANAGER->_myPlayer;
    const UnitStatus::State* status = me && me->_playerInfo ? UnitStatus::Find(me->_playerInfo->objectid()) : nullptr;
    const bool dead = status && status->respawnAt > TIME->GetGameTime();

    if (dead && !_deadView)
    {
        _deadView = true;
        _lockedBeforeDeath = _locked;
        SetLocked(false);
    }
    else if (!dead && _deadView)
    {
        _deadView = false;
        SetLocked(_lockedBeforeDeath);
    }

    if (!dead || ImGui::GetIO().WantCaptureKeyboard)
        return;

    if (INPUT->GetButtonDown(KEY_TYPE::F1) && me)
        _focus = me->GetTransform()->GetPosition();
    const KEY_TYPE keys[4] = {KEY_TYPE::F2, KEY_TYPE::F3, KEY_TYPE::F4, KEY_TYPE::F5};
    vector<shared_ptr<BasePlayerController>> allies;

    for (const auto& [id, object] : CUR_SCENE->_players)
        if (object)
            if (auto ally = object->GetScript<BasePlayerController>();
                ally && ally != me && ally->_playerInfo && me->_playerInfo &&
                ally->_playerInfo->teamid() == me->_playerInfo->teamid() &&
                UnitStatus::Find(ally->_playerInfo->objectid()) &&
                ally->_playerInfo->champtype() >= Protocol::PLAYER_TYPE_ANNIE &&
                ally->_playerInfo->champtype() <= Protocol::PLAYER_TYPE_SONA)
                allies.push_back(ally);
    std::sort(allies.begin(), allies.end(),
              [](const auto& a, const auto& b) { return a->_playerInfo->objectid() < b->_playerInfo->objectid(); });

    for (size_t i = 0; i < 4 && i < allies.size(); ++i)
        if (INPUT->GetButtonDown(keys[i]))
            _focus = allies[i]->GetTransform()->GetPosition();
}
