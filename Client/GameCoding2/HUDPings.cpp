#include "pch.h"
#include "HUDController.h"
#include "Pings.h"
#include "HudText.h"
#include "UiStrings.h"
#include "PlayerController.h"
#include "Texture.h"

void HUDController::BuildPings(PlayerController& me)
{
    Pings::Update();
    const auto& desc = GAME->GetGameDesc();
    const float scale = desc.height / 1080.f;
    const float now = TIME->GetGameTime();
    auto text = HudText::Get();

    // Minimap markers: the icon pulsing over the spot.
    if (_minimapRect.z > _minimapRect.x)
        for (const Pings::Active& ping : Pings::Current())
        {
            const float age = now - ping.at;
            const float pulse = 1.f + 0.35f * std::fabs(std::sin(age * 6.f)) * max(0.f, 1.f - age / Pings::ShowSeconds);
            const float size = 11.f * scale * pulse;
            const Vec2 c = WorldToMinimap(ping.position);
            AddSolid(Vec4(c.x - size * 0.9f, c.y - size * 0.9f, c.x + size * 0.9f, c.y + size * 0.9f), 1060,
                     Vec4(1.f, 0.95f, 0.6f, 0.35f));

            if (auto icon = Pings::Icon(ping.type))
                _sprites.push_back({1061, icon, Vec4(c.x - size, c.y - size, c.x + size, c.y + size), Vec2(0.f, 0.f),
                                    Vec2(1.f, 1.f), Vec4(1.f)});
            else
                AddSolid(Vec4(c.x - size * 0.5f, c.y - size * 0.5f, c.x + size * 0.5f, c.y + size * 0.5f), 1061,
                         Vec4(1.f, 0.9f, 0.3f, 1.f));
        }

    // Lines under the top bar, newest last, fading out.
    float y = desc.height * 0.16f;

    for (const Pings::Active& ping : Pings::Current())
    {
        const float age = now - ping.at;
        const float alpha = age > Pings::ShowSeconds - 0.8f ? max(0.f, (Pings::ShowSeconds - age) / 0.8f) : 1.f;
        const string line = UiStrings::Format("ping.line", {ping.sourceName, Pings::Label(ping.type)});
        text->Draw(line, Vec2(desc.width * 0.5f, y), 18.f * scale, HudText::Color::Gold, HudText::Align::Center, alpha);
        y += 22.f * scale;
    }

    const Pings::Wheel& wheel = Pings::GetWheel();

    if (!wheel.open)
        return;
    const float radius = 92.f * scale, inner = 26.f * scale;
    AddSolid(Vec4(wheel.center.x - radius - 8.f * scale, wheel.center.y - radius - 8.f * scale,
                  wheel.center.x + radius + 8.f * scale, wheel.center.y + radius + 8.f * scale),
             1400, Vec4(0.02f, 0.05f, 0.08f, 0.55f));

    for (int32 sector = 0; sector < Pings::WheelSectors; ++sector)
    {
        const float angle = (sector / static_cast<float>(Pings::WheelSectors)) * XM_2PI;
        const Vec2 c(wheel.center.x + std::sin(angle) * radius * 0.68f,
                     wheel.center.y - std::cos(angle) * radius * 0.68f);
        const float size = (sector == wheel.hovered ? 30.f : 22.f) * scale;
        AddSolid(Vec4(c.x - size - 4.f * scale, c.y - size - 4.f * scale, c.x + size + 4.f * scale,
                      c.y + size + 4.f * scale),
                 1401, sector == wheel.hovered ? Vec4(0.78f, 0.67f, 0.43f, 0.9f) : Vec4(0.1f, 0.16f, 0.18f, 0.9f));

        if (auto icon = Pings::Icon(Pings::SectorType(sector)))
            _sprites.push_back({1402, icon, Vec4(c.x - size, c.y - size, c.x + size, c.y + size), Vec2(0.f, 0.f),
                                Vec2(1.f, 1.f), Vec4(1.f)});
    }

    AddSolid(Vec4(wheel.center.x - inner, wheel.center.y - inner, wheel.center.x + inner, wheel.center.y + inner), 1401,
             wheel.hovered < 0 ? Vec4(0.78f, 0.67f, 0.43f, 0.9f) : Vec4(0.1f, 0.16f, 0.18f, 0.9f));

    if (auto icon = Pings::Icon(Protocol::PING_GENERIC))
        _sprites.push_back({1402, icon,
                            Vec4(wheel.center.x - inner * 0.8f, wheel.center.y - inner * 0.8f,
                                 wheel.center.x + inner * 0.8f, wheel.center.y + inner * 0.8f),
                            Vec2(0.f, 0.f), Vec2(1.f, 1.f), Vec4(1.f)});
    const string label = Pings::Label(wheel.hovered < 0 ? Protocol::PING_GENERIC : Pings::SectorType(wheel.hovered));
    text->Draw(label, Vec2(wheel.center.x, wheel.center.y + radius + 22.f * scale), 18.f * scale, HudText::Color::Gold);
}
