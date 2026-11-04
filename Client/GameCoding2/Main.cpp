// 클라이언트 진입점.
#include "pch.h"
#include "Main.h"
#include "EngineCore/Game.h"
#include "SceneExcuter.h"
#include "VfxAutoTest.h"
#include "MapAutoTest.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
    if (strstr(lpCmdLine, "--annie-q-smoke") != nullptr)
    {
        extern int RunAnnieQSmokeTest();

        return RunAnnieQSmokeTest();
    }

    if (strstr(lpCmdLine, "--m1-smoke") != nullptr)
    {
        extern int RunM1SmokeTest();

        return RunM1SmokeTest();
    }

    if (strstr(lpCmdLine, "--vfx-autotest"))
        VfxAutoTest::Enable();

    if (strstr(lpCmdLine, "--map-autotest"))
        MapAutoTest::Enable(strstr(lpCmdLine, "--epics") != nullptr, strstr(lpCmdLine, "--showcase") != nullptr,
                            strstr(lpCmdLine, "--gallery") != nullptr);

    if (strstr(lpCmdLine, "--map-autotest") && strstr(lpCmdLine, "--combo"))
        MapAutoTest::EnableCombo();

    if (strstr(lpCmdLine, "--map-autotest") && strstr(lpCmdLine, "--skins"))
        MapAutoTest::EnableSkins();

    if (strstr(lpCmdLine, "--map-autotest") && strstr(lpCmdLine, "--demo"))
        MapAutoTest::EnableDemo(strstr(lpCmdLine, "--record") != nullptr);

    {
        const string line = lpCmdLine;
        auto argument = [&](const string& name)
        {
            const size_t at = line.find(name + " ");

            if (at == string::npos)
                return string();
            const size_t begin = at + name.size() + 1, end = line.find(' ', begin);

            return line.substr(begin, end == string::npos ? string::npos : end - begin);
        };
        const string server = argument("--server");
        const string token = argument("--token");

        if (!server.empty() || !token.empty())
        {
            const size_t colon = server.rfind(':');
            const string host = server.empty() ? "127.0.0.1" : server.substr(0, colon);
            const uint16 port = colon == string::npos ? 8080 : static_cast<uint16>(std::stoi(server.substr(colon + 1)));
            NETWORK->Configure(wstring(host.begin(), host.end()), port, token);
        }
    }

    // ImGui windows start hidden; F2 toggles them in game.
    if (strstr(lpCmdLine, "--hide-gui"))
        GUI->SetVisible(false);

    if (strstr(lpCmdLine, "--profile"))
    {
        std::filesystem::create_directories(L"../Tests/Profile");
        Profiler::Enable(L"../Tests/Profile/client_" + std::to_wstring(::GetCurrentProcessId()) + L".txt");
    }

    GameDesc desc;
    desc.appName = L"GameCoding";
    desc.hInstance = hInstance;
    desc.vsync = strstr(lpCmdLine, "--vsync") != nullptr;

    desc.hWnd = NULL;

    {
        RECT work = {};
        ::SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
        RECT frame = {0, 0, 0, 0};
        ::AdjustWindowRect(&frame, WS_OVERLAPPEDWINDOW, false);
        const float fitWidth = static_cast<float>((work.right - work.left) - (frame.right - frame.left));
        const float fitHeight = static_cast<float>((work.bottom - work.top) - (frame.bottom - frame.top));
        float height = min(1080.f, min(fitHeight, fitWidth * 9.f / 16.f));
        int width = 0, requestedHeight = 0;

        if (const char* resolution = strstr(lpCmdLine, "--resolution ");
            resolution && sscanf_s(resolution + 13, "%dx%d", &width, &requestedHeight) == 2 && width > 0 &&
            requestedHeight > 0)
        {
            desc.width = static_cast<float>(width);
            desc.height = static_cast<float>(requestedHeight);
        }
        else
        {
            height = max(720.f, std::floor(height / 9.f) * 9.f);
            desc.width = height * 16.f / 9.f;
            desc.height = height;
        }
    }

    desc.clearColor = Color(0.5f, 0.5f, 0.5f, 0.5f);
    desc.app = make_shared<SceneExcuter>();

    GAME->Run(desc);

    return 0;
}
