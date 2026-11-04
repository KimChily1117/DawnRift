#pragma once
#include "MonoBehaviour.h"

class CameraController : public MonoBehaviour
{
public:
    static constexpr float LolFieldOfViewDegrees = 40.f;
    static constexpr float LolPitchDegrees = 56.f;
    static constexpr float LolZoomDistance = 22.5f;
    static constexpr float LolMinZoomDistance = 13.f;
    static constexpr float ZoomStep = 1.5f;
    static constexpr float EdgePanSpeed = 30.f;
    static constexpr int32 EdgePanPixels = 8;

    void Awake() override;
    void Update() override;

    shared_ptr<GameObject> _target;
    Vec3 _offset;

    bool IsLocked() const
    {
        return _locked;
    }

    void SetLocked(bool locked);

    void ToggleLocked()
    {
        SetLocked(!_locked);
    }

    void Peek(const Vec3& point);

    void UpdateDeathView();
    bool _deadView = false;
    bool _lockedBeforeDeath = true;

    // Where the camera looks on the ground now.
    Vec3 GetFocus() const
    {
        return _focus;
    }

    static shared_ptr<CameraController> Find();

    static inline float FilmZoom = 0.f, FilmPitchDegrees = 0.f, FilmYawDegrees = 0.f;

private:
    float _zoom = LolZoomDistance;
    float _shownZoom = LolZoomDistance;
    bool _locked = true;
    Vec3 _focus = Vec3::Zero;
    bool _peeking = false;
    Vec3 _peek = Vec3::Zero;
};
