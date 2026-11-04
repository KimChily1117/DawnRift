// 애니 E 스킬.
#pragma once
#include "ISkill.h"

class AnnieESpell : public ISkill
{
public:
    void Use(shared_ptr<GameObject> caster, shared_ptr<GameObject> target) override;
};
