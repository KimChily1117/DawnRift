#pragma once
#include "BaseCollider.h"

class OBBBoxCollider : public BaseCollider
{
public:
    OBBBoxCollider();
    virtual ~OBBBoxCollider();

    virtual void Update() override;
    virtual bool Intersects(Ray& ray, OUT float& distance) override;
    virtual bool Intersects(shared_ptr<BaseCollider>& other) override;

    BoundingOrientedBox& GetBoundingBox()
    {
        return _boundingBox;
    }

private:
    BoundingOrientedBox _boundingBox;
};
