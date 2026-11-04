#include "pch.h"
#include "TooltipText.h"
#include <fstream>

namespace
{
const wstring TooltipsFile = L"..\\Resources\\Data\\UI\\Tooltips.ko.json";

using Entries = unordered_map<string, unordered_map<string, TooltipText::Entry>>;

const Entries& All()
{
    static Entries entries;
    static bool loaded = false;

    if (loaded)
        return entries;
    loaded = true;
    std::ifstream file(TooltipsFile);
    json data = json::parse(file, nullptr, false);

    if (data.is_discarded())
    {
        DEBUG_LOG("[Tooltip] missing Resources/Data/UI/Tooltips.ko.json");

        return entries;
    }

    for (const auto& [champion, slots] : data.items())
        for (const auto& [slot, value] : slots.items())
        {
            if (!value.is_object())
                continue;
            TooltipText::Entry entry;
            entry.name = value.value("name", "");
            entry.ranks = value.value("ranks", vector<string>{});
            entry.cooldown = value.value("cooldown", vector<float>{});
            entry.cost = value.value("cost", vector<float>{});
            entry.range = value.value("range", 0.f);
            entries[champion][slot] = std::move(entry);
        }
    return entries;
}

HudText::Color ColorOf(const string& tag)
{
    if (tag == "magicDamage" || tag == "magicPen")
        return HudText::Color::Magic;

    if (tag == "physicalDamage" || tag == "scaleAD")
        return HudText::Color::Physical;

    if (tag == "shield" || tag == "healing" || tag == "scaleHealth")
        return HudText::Color::Green;

    if (tag == "rules")
        return HudText::Color::Gray;

    if (tag == "trueDamage")
        return HudText::Color::White;

    return HudText::Color::Gold;
}
}

namespace TooltipText
{
const Entry* Find(const string& champion, const string& slot)
{
    const auto& entries = All();
    auto c = entries.find(champion);

    if (c == entries.end())
        return nullptr;
    auto s = c->second.find(slot);

    return s == c->second.end() ? nullptr : &s->second;
}

vector<Line> Layout(const string& text, float size, float maxWidth)
{
    auto hud = HudText::Get();
    vector<Line> lines(1);
    float width = 0.f;
    vector<HudText::Color> colors = {HudText::Color::White};
    auto addWord = [&](const string& word)
    {
        if (word.empty())
            return;
        const float w = hud->Measure(word, size);
        const bool leadingSpace = word[0] == ' ';

        if (width > 0.f && width + w > maxWidth)
        {
            lines.emplace_back();
            width = 0.f;

            if (leadingSpace)
            {
                const string rest = word.substr(1);
                lines.back().push_back({rest, colors.back()});
                width = hud->Measure(rest, size);

                return;
            }
        }

        Line& line = lines.back();

        if (!line.empty() && line.back().color == colors.back())
            line.back().text += word;
        else
            line.push_back({word, colors.back()});
        width += w;
    };

    string word;

    for (size_t i = 0; i < text.size();)
    {
        if (text[i] == '<')
        {
            const size_t close = text.find('>', i);

            if (close == string::npos)
                break;
            addWord(word);
            word.clear();
            string tag = text.substr(i + 1, close - i - 1);
            i = close + 1;

            if (tag == "br" || tag == "br/" || tag == "br /")
            {
                lines.emplace_back();
                width = 0.f;
            }
            else if (!tag.empty() && tag[0] == '/')
            {
                if (colors.size() > 1)
                    colors.pop_back();
            }
            else
                colors.push_back(ColorOf(tag.substr(0, tag.find(' '))));
            continue;
        }

        if (text[i] == ' ' && !word.empty())
        {
            addWord(word);
            word.clear();
        }

        word += text[i++];
    }

    addWord(word);

    return lines;
}
}
