#include "pch.h"
#include "HUDController.h"
#include "Chat.h"
#include "HudText.h"
#include "UiStrings.h"
#include "PlayerController.h"

void HUDController::BuildChat(PlayerController& me)
{
    Chat::Update();
    const auto& desc = GAME->GetGameDesc();
    const float scale = desc.height / 1080.f;
    const float now = TIME->GetGameTime();
    auto text = HudText::Get();
    const float size = 17.f * scale, lineHeight = size * 1.35f;
    const float left = 18.f * scale, width = 520.f * scale;
    const bool open = Chat::IsOpen();
    // Above the portrait frame.
    float bottom = desc.height - 230.f * scale;
    const int32 myTeam = me._playerInfo ? me._playerInfo->teamid() : 0;

    if (open)
    {
        const float boxTop = bottom - lineHeight * 1.4f;
        AddSolid(Vec4(left - 6.f * scale, boxTop, left + width, bottom), 1150, Vec4(0.f, 0.02f, 0.05f, 0.75f));
        AddSolid(Vec4(left - 6.f * scale, boxTop, left - 3.f * scale, bottom), 1151, Vec4(0.78f, 0.67f, 0.43f, 1.f));
        const string prefix = UiStrings::Get(Chat::Channel() == Protocol::CHAT_ALL ? "chat.all" : "chat.team") + " ";
        const string composition = Chat::Composition();
        const string line = prefix + Chat::Input() + composition + (std::fmod(now, 1.f) < 0.5f ? "|" : "");
        text->Draw(line, Vec2(left, (boxTop + bottom) * 0.5f), size, HudText::Color::White, HudText::Align::Left);

        if (!composition.empty())
        {
            const float start = left + text->Measure(prefix + Chat::Input(), size),
                        end = start + text->Measure(composition, size);
            AddSolid(Vec4(start, bottom - 3.f * scale, end, bottom - 1.5f * scale), 1152, Vec4(1.f, 1.f, 1.f, 0.9f));
        }

        bottom = boxTop - 4.f * scale;
    }

    const auto& lines = Chat::Lines();
    const size_t shown = open ? min<size_t>(lines.size(), 12) : min<size_t>(lines.size(), 6);
    float y = bottom - lineHeight * 0.5f;
    size_t drawn = 0;

    for (size_t i = lines.size(); i-- > 0 && drawn < shown;)
    {
        const Chat::Line& line = lines[i];
        const float age = now - line.at;

        if (!open && age > Chat::ShowSeconds)
            break;
        const float alpha = open ? 1.f : age > Chat::ShowSeconds - 1.f ? max(0.f, Chat::ShowSeconds - age) : 1.f;
        string prefix;
        HudText::Color color = HudText::Color::White;

        if (line.channel == Protocol::CHAT_SYSTEM)
        {
            color = HudText::Color::Gold;
        }
        else
        {
            prefix = (line.channel == Protocol::CHAT_ALL ? UiStrings::Get("chat.all") : UiStrings::Get("chat.team")) +
                     " " + line.sender + ": ";
            color = line.channel == Protocol::CHAT_ALL
                        ? (line.team == myTeam ? HudText::Color::Mana : HudText::Color::Red)
                        : HudText::Color::Mana;
        }

        if (open)
            AddSolid(Vec4(left - 6.f * scale, y - lineHeight * 0.5f, left + width, y + lineHeight * 0.5f), 1150,
                     Vec4(0.f, 0.02f, 0.05f, 0.55f));
        text->Draw(prefix, Vec2(left, y), size, color, HudText::Align::Left, alpha);
        text->Draw(line.text, Vec2(left + text->Measure(prefix, size), y), size,
                   line.channel == Protocol::CHAT_SYSTEM ? HudText::Color::Gold : HudText::Color::White,
                   HudText::Align::Left, alpha);
        y -= lineHeight;
        ++drawn;
    }
}
