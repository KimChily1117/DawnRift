// 애니 Q 스킬.
#pragma once
#include "ISkill.h"

class AnnieQSpell : public ISkill
{
public:
    void Use(shared_ptr<GameObject> caster, shared_ptr<GameObject> target) override;
};
