#pragma once
#include "OtherPlayerController.h"

class NpcController : public OtherPlayerController
{
    using Super = OtherPlayerController;

public:
    void Update() override;

protected:
    void ProcSkill(int32 skillId) override;
};
