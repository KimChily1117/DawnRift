#pragma once
#include "pch.h"
#include "GameObject.h"

class ISkill
{
public:
    virtual ~ISkill() = default;
    virtual void Use(shared_ptr<GameObject> caster, shared_ptr<GameObject> target) = 0;
};
