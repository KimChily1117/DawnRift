#include "pch.h"
#include "Keybinds.h"
#include <fstream>

namespace
{
const wstring File = L"..\\Saved\\Keybinds.json";
const KEY_TYPE Defaults[static_cast<int32>(Keybinds::Action::Count)] = {
    KEY_TYPE::S, KEY_TYPE::A, KEY_TYPE::KEY_4, KEY_TYPE::P, KEY_TYPE::G, KEY_TYPE::V, KEY_TYPE::B, KEY_TYPE::Y,
};
const char* const Names[static_cast<int32>(Keybinds::Action::Count)] = {
    "stop", "attackMove", "ward", "shop", "ping", "pingRetreat", "recall", "cameraLock",
};
KEY_TYPE g_keys[static_cast<int32>(Keybinds::Action::Count)];
bool g_quickCast = false;
bool g_loaded = false;

void EnsureLoaded()
{
    if (!g_loaded)
        Keybinds::Load();
}
}

namespace Keybinds
{
void ResetDefaults()
{
    std::copy(std::begin(Defaults), std::end(Defaults), g_keys);
    g_quickCast = false;
}

void Load()
{
    g_loaded = true;
    ResetDefaults();
    std::ifstream file(File);
    json j = json::parse(file, nullptr, false);

    if (j.is_discarded())
        return;

    for (int32 i = 0; i < static_cast<int32>(Action::Count); ++i)
        if (j.contains(Names[i]) && j[Names[i]].is_number_integer())
        {
            const int32 code = j[Names[i]].get<int32>();

            if (code > 0 && code < 256)
                g_keys[i] = static_cast<KEY_TYPE>(code);
        }
    if (j.contains("quickCast") && j["quickCast"].is_boolean())
        g_quickCast = j["quickCast"].get<bool>();
}

void Save()
{
    json j;

    for (int32 i = 0; i < static_cast<int32>(Action::Count); ++i)
        j[Names[i]] = static_cast<int32>(g_keys[i]);
    j["quickCast"] = g_quickCast;
    std::error_code error;
    std::filesystem::create_directories(std::filesystem::path(File).parent_path(), error);
    std::ofstream file(File);
    file << j.dump(1);
}

KEY_TYPE Key(Action action)
{
    EnsureLoaded();

    return g_keys[static_cast<int32>(action)];
}

void SetKey(Action action, KEY_TYPE key)
{
    EnsureLoaded();
    g_keys[static_cast<int32>(action)] = key;
}

bool Down(Action action)
{
    return INPUT->GetButtonDown(Key(action));
}

bool Held(Action action)
{
    return INPUT->GetButton(Key(action));
}

bool QuickCast()
{
    EnsureLoaded();

    return g_quickCast;
}

void SetQuickCast(bool on)
{
    EnsureLoaded();
    g_quickCast = on;
}

const char* LabelKey(Action action)
{
    static const char* const Labels[] = {"key.stop", "key.attackMove",  "key.ward",   "key.shop",
                                         "key.ping", "key.pingRetreat", "key.recall", "key.cameraLock"};

    return Labels[static_cast<int32>(action)];
}

string KeyName(KEY_TYPE key)
{
    const int32 code = static_cast<int32>(key);

    if ((code >= 'A' && code <= 'Z') || (code >= '0' && code <= '9'))
        return string(1, static_cast<char>(code));

    switch (code)
    {
    case VK_SPACE:
        return "Space";
    case VK_TAB:
        return "Tab";
    case VK_MENU:
        return "Alt";
    case VK_SHIFT:
        return "Shift";
    case VK_CONTROL:
        return "Ctrl";
    case VK_RETURN:
        return "Enter";
    case VK_BACK:
        return "Backspace";
    case VK_OEM_MINUS:
        return "-";
    case VK_OEM_PLUS:
        return "=";
    case VK_UP:
        return "Up";
    case VK_DOWN:
        return "Down";
    case VK_LEFT:
        return "Left";
    case VK_RIGHT:
        return "Right";
    default:
        if (code >= VK_F1 && code <= VK_F12)
            return "F" + std::to_string(code - VK_F1 + 1);

        return "#" + std::to_string(code);
    }
}
}
