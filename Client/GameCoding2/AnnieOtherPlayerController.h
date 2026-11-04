#pragma once
#include "OtherPlayerController.h"

class AnnieOtherPlayerController : public OtherPlayerController
{
    using Super = OtherPlayerController;

public:
    void Awake() override;
    void Update() override;
    void Start() override;
    void LateUpdate() override;
    void FixedUpdate() override;

protected:
    virtual void ProcSkill(int32 skillId) override;
    void AlignToTarget();

    Vec3 CalculateRotationFromDirection(const Vec3& dir);
};
