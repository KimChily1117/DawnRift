#pragma once
#include "PlayerController.h"
#include "OtherPlayerController.h"

class KitPlayerController : public PlayerController
{
    using Super = PlayerController;

public:
    void Update() override;

protected:
    void ProcSkill(int32 skillId) override;
};

class KitOtherPlayerController : public OtherPlayerController
{
    using Super = OtherPlayerController;

public:
    void Update() override;

protected:
    void ProcSkill(int32 skillId) override;
};
