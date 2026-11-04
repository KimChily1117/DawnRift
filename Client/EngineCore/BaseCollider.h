#pragma once
#include "Component.h"

enum class ColliderType
{
    Sphere,
    AABB,
    OBB
};

class BaseCollider : public Component
{
public:
    BaseCollider(ColliderType colliderType);
    virtual ~BaseCollider();

    virtual bool Intersects(Ray& ray, OUT float& distance) = 0;
    virtual bool Intersects(shared_ptr<BaseCollider>& other) = 0;

    ColliderType GetColliderType()
    {
        return _colliderType;
    }

protected:
    ColliderType _colliderType;
};
