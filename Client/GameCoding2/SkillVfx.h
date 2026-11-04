#pragma once
#include "BasePlayerController.h"

void PlaySkillVfx(BasePlayerController& caster, PlayerState skill);

bool PlaySkillHitVfx(Protocol::PLAYER_CHAMPION_TYPE champion, int32 skillId, const shared_ptr<GameObject>& source,
                     const shared_ptr<GameObject>& target);

void PreloadChampionVfx(Protocol::PLAYER_CHAMPION_TYPE champion, int32 skinId = 0);

uint64 PlayBuffVfx(Protocol::PLAYER_CHAMPION_TYPE champion, const Protocol::S_Buff& buff,
                   const shared_ptr<GameObject>& target, int32 skinId = 0);
