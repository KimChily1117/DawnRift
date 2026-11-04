#include "pch.h"
#include "UiStrings.h"
#include <fstream>

namespace
{
const wstring StringsFile = L"..\\Resources\\Data\\UI\\Strings.ko.json";

const unordered_map<string, string>& Strings()
{
    static unordered_map<string, string> strings;
    static bool loaded = false;

    if (!loaded)
    {
        loaded = true;
        std::ifstream file(StringsFile);
        json data = json::parse(file, nullptr, false);

        if (data.is_discarded())
        {
            DEBUG_LOG("[UiStrings] missing Resources/Data/UI/Strings.ko.json");
        }
        else
        {
            for (const auto& [key, value] : data.items())
                if (value.is_string())
                    strings[key] = value.get<string>();
        }
    }

    return strings;
}
}

namespace UiStrings
{
const string& Get(const string& key)
{
    const auto& strings = Strings();
    auto it = strings.find(key);

    return it != strings.end() ? it->second : key;
}

string Format(const string& key, std::initializer_list<string> args)
{
    string text = Get(key);
    int32 index = 0;

    for (const string& arg : args)
    {
        const string marker = "{" + std::to_string(index++) + "}";

        for (size_t at = text.find(marker); at != string::npos; at = text.find(marker, at + arg.size()))
            text.replace(at, marker.size(), arg);
    }

    return text;
}
}
