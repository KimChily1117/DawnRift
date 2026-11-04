// 가렌 Q 스킬.
#pragma once
#include "ISkill.h"

class GarenQSpell : public ISkill
{
    void Use(shared_ptr<GameObject> caster, shared_ptr<GameObject> target) override;
};
