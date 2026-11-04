#pragma once

namespace GameCursor
{
enum class Kind
{
    Normal,
    Pressed,
    HoverEnemy,
    HoverFriendly,
    Shop,
    Target,
    TargetAlly,
    TargetEnemy,
    Count,
};

void Set(Kind kind);
Kind Current();
const char* Name(Kind kind);
}
