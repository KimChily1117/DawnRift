// 애니 W 스킬.
#pragma once
#include "ISkill.h"

class AnnieWSpell : public ISkill
{
public:
    void Use(shared_ptr<GameObject> caster, shared_ptr<GameObject> target) override;
    void UseAt(const shared_ptr<GameObject>& caster, const Vec3& targetPosition);
};
