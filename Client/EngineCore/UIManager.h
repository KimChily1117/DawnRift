#pragma once
#include "..\GameCoding2\Enum.pb.h"

class UIManager
{
    DECLARE_SINGLE(UIManager)

public:
    void Init();
    void Update();

    void SetTarget(Protocol::PLAYER_CHAMPION_TYPE type);

    void SetHUDControllerGameObject(shared_ptr<GameObject> go)
    {
        _hud = go;
    }

    void SetCursorControllerGameObject(shared_ptr<GameObject> go)
    {
        _cursorManagement = go;
    }

    void SetSkillIndicatorControllerGameObject(shared_ptr<GameObject> go)
    {
        _skillIndicator = go;
    }

    shared_ptr<GameObject> GetHUD()
    {
        return _hud;
    }

    shared_ptr<GameObject> GetCursorController()
    {
        return _cursorManagement;
    }

    shared_ptr<GameObject> GetSkillIndicatorController()
    {
        return _skillIndicator;
    }

private:
    // HUD에 필요한 기초 정보들 Setting
    void InitHUD();
    // 커서 , 인디케이터를 이걸로 바꿔줌
    void InitCursor();
    void InitIndicator();

    shared_ptr<GameObject> _hud;
    shared_ptr<GameObject> _cursorManagement;
    shared_ptr<GameObject> _skillIndicator;

    shared_ptr<Shader> _shader;
    // 일반적으로 보여주기 위한 Render 전용 쉐이더

    shared_ptr<Shader> _InDecreaseUIShader;
    // Hp,Mp 쿨타임 관련 UI Effect를 저장하기 위한 쉐이더
};
