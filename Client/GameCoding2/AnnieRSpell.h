// 애니 R 스킬.
#pragma once
#include "ISkill.h"

class AnnieRSpell : public ISkill
{
public:
    void Use(shared_ptr<GameObject> caster, shared_ptr<GameObject> target) override;
};
