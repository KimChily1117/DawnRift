// 엔진 메인 루프.
#include "pch.h"
#include <imm.h>
#pragma comment(lib, "imm32.lib")
#include "Game.h"
#include "IExecute.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

WPARAM Game::Run(GameDesc& desc)
{
    _desc = desc;
    assert(_desc.app != nullptr);

    // 1) 윈도우 창 정보 등록
    MyRegisterClass();

    // 2) 윈도우 창 생성
    if (!InitInstance(SW_SHOWNORMAL))
        return FALSE;

    GRAPHICS->Init(_desc.hWnd);
    TIME->Init();
    INPUT->Init(_desc.hWnd);
    GUI->Init();
    RESOURCES->Init();
    NETWORK->Init();
    SOUND->Init();
    UI->Init();

    _desc.app->Init();

    MSG msg = {0};

    while (msg.message != WM_QUIT)
    {
        if (::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
        }
        else
        {
            Update();
        }
    }

    return msg.wParam;
}

ATOM Game::MyRegisterClass()
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = _desc.hInstance;
    wcex.hIcon = ::LoadIcon(NULL, IDI_WINLOGO);
    wcex.hCursor = ::LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = NULL;
    wcex.lpszClassName = _desc.appName.c_str();
    wcex.hIconSm = wcex.hIcon;

    return RegisterClassExW(&wcex);
}

BOOL Game::InitInstance(int cmdShow)
{
    RECT windowRect = {0, 0, _desc.width, _desc.height};
    ::AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, false);

    _desc.hWnd = CreateWindowW(_desc.appName.c_str(), _desc.appName.c_str(), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 0,
                               windowRect.right - windowRect.left, windowRect.bottom - windowRect.top, nullptr, nullptr,
                               _desc.hInstance, nullptr);

    if (!_desc.hWnd)
        return FALSE;

    ::ShowWindow(_desc.hWnd, cmdShow);
    ::UpdateWindow(_desc.hWnd);

    return TRUE;
}

LRESULT CALLBACK Game::WndProc(HWND handle, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(handle, message, wParam, lParam))
        return true;

    switch (message)
    {
    case WM_SIZE:
        break;
    case WM_MOUSEWHEEL:
        INPUT->AddWheelDelta(GET_WHEEL_DELTA_WPARAM(wParam));
        break;

    case WM_CHAR:
    {
        static wchar_t highSurrogate = 0;
        const wchar_t ch = static_cast<wchar_t>(wParam);

        if (ch >= 0xD800 && ch <= 0xDBFF)
            highSurrogate = ch;
        else if (ch >= 0xDC00 && ch <= 0xDFFF && highSurrogate)
        {
            INPUT->AddChar(0x10000 + ((static_cast<uint32>(highSurrogate) - 0xD800) << 10) + (ch - 0xDC00));
            highSurrogate = 0;
        }
        else
            INPUT->AddChar(ch);
        break;
    }
    case WM_IME_COMPOSITION:
    {
        if (HIMC context = ::ImmGetContext(handle))
        {
            if (lParam & GCS_COMPSTR)
            {
                const LONG bytes = ::ImmGetCompositionStringW(context, GCS_COMPSTR, nullptr, 0);
                std::wstring text(bytes > 0 ? bytes / sizeof(wchar_t) : 0, L'\0');

                if (bytes > 0)
                    ::ImmGetCompositionStringW(context, GCS_COMPSTR, text.data(), bytes);
                INPUT->SetComposition(text);
            }

            if (lParam & GCS_RESULTSTR)
                INPUT->SetComposition(L"");
            ::ImmReleaseContext(handle, context);
        }

        return ::DefWindowProc(handle, message, wParam, lParam);
    }
    case WM_IME_ENDCOMPOSITION:
        INPUT->SetComposition(L"");

        return ::DefWindowProc(handle, message, wParam, lParam);
    case WM_CLOSE:
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return ::DefWindowProc(handle, message, wParam, lParam);
    }
}

void Game::Update()
{
    Profiler::EndFrame();

    TIME->Update();
    INPUT->Update();
    {
        PROFILE_SCOPE("network");
        NETWORK->Update();
    }

    SOUND->Update();

    ShowFps();

    GRAPHICS->RenderBegin();

    {
        PROFILE_SCOPE("gui update");
        GUI->Update();
        UI->Update();
    }

    SCENE->Update();
    {
        PROFILE_SCOPE("particles + app");
        PARTICLE->Update();
        _desc.app->Update();
        _desc.app->Render();
    }

    {
        PROFILE_SCOPE("gui render");
        GUI->Render();
        PARTICLE->Render();
    }

    {
        PROFILE_SCOPE("present");
        GRAPHICS->RenderEnd();
    }
}

void Game::ShowFps()
{
    uint32 fps = GET_SINGLE(TimeManager)->GetFps();

    static uint32 shownFps = UINT32_MAX;

    if (fps == shownFps)
        return;
    shownFps = fps;

    WCHAR text[100] = L"";
    ::wsprintf(text, L"FPS : %d", fps);

    ::SetWindowText(_desc.hWnd, text);
}
