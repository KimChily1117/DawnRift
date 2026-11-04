#pragma once
#include "../GameCoding2/Struct.pb.h"
#include "../GameCoding2/PlayerController.h"

class GameManager
{
    DECLARE_SINGLE(GameManager)
public:
    shared_ptr<PlayerController> _myPlayer;
};
