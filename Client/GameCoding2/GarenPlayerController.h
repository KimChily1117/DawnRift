// 가렌 조작과 서버 입력 전송.
#pragma once
#include "PlayerController.h"

class GarenPlayerController : public PlayerController
{
    using Super = PlayerController;

public:
    void Awake() override;
    void Update() override;
    void Start() override;
    void LateUpdate() override;
    void FixedUpdate() override;

protected:
    virtual void ProcSkill(int32 skillId) override;
    void AlignToTarget();
    shared_ptr<class ISkill> CreateSkillInstance(int32 skillId);

private:
};
