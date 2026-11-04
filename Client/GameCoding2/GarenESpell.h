// 가렌 E 스킬.
#pragma once
#include "ISkill.h"

class GarenESpell : public ISkill
{
    void Use(shared_ptr<GameObject> caster, shared_ptr<GameObject> target) override;
};
