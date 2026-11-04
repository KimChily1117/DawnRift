#pragma once

#define DECLARE_SINGLE(classname)                                                                                      \
private:                                                                                                               \
    classname() {}                                                                                                     \
                                                                                                                       \
public:                                                                                                                \
    static classname* GetInstance()                                                                                    \
    {                                                                                                                  \
        static classname s_instance;                                                                                   \
        return &s_instance;                                                                                            \
    }

#define GET_SINGLE(classname) classname::GetInstance()

#define DEBUG_LOG(x)                                                                                                   \
    {                                                                                                                  \
        std::wostringstream woss;                                                                                      \
        woss << x << L"\n";                                                                                            \
        std::wstring wstr = woss.str();                                                                                \
        std::string str(wstr.begin(), wstr.end());                                                                     \
        OutputDebugStringA(str.c_str());                                                                               \
    }

#define CHECK(p)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        const HRESULT check_hr_ = (p);                                                                                 \
        assert(SUCCEEDED(check_hr_));                                                                                  \
        (void)check_hr_;                                                                                               \
    } while (0)
#define GAME GET_SINGLE(Game)
#define GRAPHICS GET_SINGLE(Graphics)
#define DEVICE GRAPHICS->GetDevice()
#define DC GRAPHICS->GetDeviceContext()
#define INPUT GET_SINGLE(InputManager)
#define TIME GET_SINGLE(TimeManager)
#define DT TIME->GetDeltaTime()
#define RESOURCES GET_SINGLE(ResourceManager)
#define INSTANCING GET_SINGLE(InstancingManager)
#define GUI GET_SINGLE(ImGuiManager)
#define SCENE GET_SINGLE(SceneManager)
#define CUR_SCENE SCENE->GetCurrentScene()
#define NETWORK GET_SINGLE(NetworkManager)
#define GAMEMANAGER GET_SINGLE(GameManager)
#define UI GET_SINGLE(UIManager)
#define SOUND GET_SINGLE(SoundManager)
#define PARTICLE GET_SINGLE(ParticleManager)

enum LayerMask
{
    Layer_Default = 0,
    Layer_UI,
};
