#pragma once

enum class KEY_TYPE
{
    UP = VK_UP,
    DOWN = VK_DOWN,
    LEFT = VK_LEFT,
    RIGHT = VK_RIGHT,

    W = 'W',
    A = 'A',
    S = 'S',
    D = 'D',

    Q = 'Q',
    E = 'E',
    Z = 'Z',
    Y = 'Y',
    C = 'C',
    R = 'R',
    F = 'F',
    TAB = VK_TAB,
    SPACE = VK_SPACE,
    F1 = VK_F1,
    F2 = VK_F2,
    F3 = VK_F3,
    F4 = VK_F4,
    F5 = VK_F5,
    F8 = VK_F8,

    KEY_1 = '1',
    KEY_2 = '2',
    KEY_3 = '3',
    KEY_4 = '4',
    KEY_5 = '5',
    KEY_6 = '6',
    KEY_7 = '7',
    P = 'P',
    G = 'G',
    V = 'V',
    B = 'B',
    ALT = VK_MENU,
    SHIFT = VK_SHIFT,
    BACK = VK_BACK,

    LBUTTON = VK_LBUTTON,
    RBUTTON = VK_RBUTTON,
    ENTER = VK_RETURN,
    MINUS = VK_OEM_MINUS,
    PLUS = VK_OEM_PLUS,
    ESCAPE = VK_ESCAPE,
    CONTROL = VK_CONTROL
};

enum class KEY_STATE
{
    NONE,
    PRESS,
    DOWN,
    UP,
    END
};

enum
{
    KEY_TYPE_COUNT = static_cast<int32>(UINT8_MAX + 1),
    KEY_STATE_COUNT = static_cast<int32>(KEY_STATE::END),
};

class InputManager
{
    DECLARE_SINGLE(InputManager);

public:
    void Init(HWND hwnd);
    void Update();

    // ������ ���� ��
    bool GetButton(KEY_TYPE key)
    {
        return GetState(key) == KEY_STATE::PRESS;
    }

    // �� ó�� ������ ��
    bool GetButtonDown(KEY_TYPE key)
    {
        return GetState(key) == KEY_STATE::DOWN;
    }

    // �� ó�� ������ ���� ��
    bool GetButtonUp(KEY_TYPE key)
    {
        return GetState(key) == KEY_STATE::UP;
    }

    const POINT& GetMousePos()
    {
        return _mousePos;
    }

    void SetTextCapture(bool capture)
    {
        _textCapture = capture;

        if (!capture)
        {
            _typed.clear();
            _composition.clear();
        }
    }

    bool IsTextCaptured() const
    {
        return _textCapture;
    }

    void AddChar(uint32 codePoint)
    {
        if (_textCapture)
            _typed.push_back(codePoint);
    }

    void SetComposition(const std::wstring& text)
    {
        _composition = text;
    }

    const std::wstring& GetComposition() const
    {
        return _composition;
    }

    std::vector<uint32> TakeTyped()
    {
        std::vector<uint32> typed;
        typed.swap(_typed);

        return typed;
    }

    int32 FirstKeyDown() const
    {
        for (uint32 key = 1; key < KEY_TYPE_COUNT; ++key)
            if (_states[key] == KEY_STATE::DOWN)
                return static_cast<int32>(key);

        return -1;
    }

    float GetWheel() const
    {
        return _wheel;
    }

    void AddWheelDelta(int32 delta)
    {
        _wheelDelta += delta;
    }

private:
    inline KEY_STATE GetState(KEY_TYPE key)
    {
        if (_textCapture && key != KEY_TYPE::ENTER && key != KEY_TYPE::ESCAPE && key != KEY_TYPE::SHIFT)
            return KEY_STATE::NONE;

        return _states[static_cast<uint8>(key)];
    }

private:
    HWND _hwnd;
    vector<KEY_STATE> _states;
    POINT _mousePos = {};
    int32 _wheelDelta = 0;
    bool _textCapture = false;
    std::vector<uint32> _typed;
    std::wstring _composition;
    float _wheel = 0.f;
};
