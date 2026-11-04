#pragma once
#include "MonoBehaviour.h"

class Material;

class FloatingDamageController : public MonoBehaviour
{
public:
    static void Spawn(const shared_ptr<GameObject>& target, int32 damage);
    void Configure(const shared_ptr<GameObject>& target, int32 damage);
    void Start() override;
    void Update() override;

private:
    void RemoveVisuals();

    weak_ptr<GameObject> _target;
    vector<shared_ptr<GameObject>> _digits;
    int32 _damage = 0;
    float _elapsed = 0.f;
    float _lifetime = 0.95f;
    float _verticalStackOffset = 0.f;
    bool _expired = false;
};
