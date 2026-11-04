#pragma once
#include "MonoBehaviour.h"

class SkillIndicatorController : public MonoBehaviour
{
public:
    void Awake() override;
    void Update() override;

    void Show(const shared_ptr<GameObject>& owner, float range, float angleDegrees);
    void ShowCircle(const shared_ptr<GameObject>& owner, float range);

    void ShowLine(const shared_ptr<GameObject>& owner, float range, float halfWidth);
    void Hide();
    void SetAimPosition(const Vec3& worldPosition);

    bool IsVisible() const
    {
        return _visible;
    }

    bool IsCircle() const
    {
        return _circle;
    }

    float GetRange() const
    {
        return _range;
    }

private:
    weak_ptr<GameObject> _owner;
    Vec3 _aimPosition = Vec3::Zero;
    float _range = 4.f;
    float _angleDegrees = 50.f;
    bool _visible = false;
    bool _circle = false;
    bool _line = false;
    float _halfWidth = 0.5f;
    shared_ptr<class Material> _material;
};
