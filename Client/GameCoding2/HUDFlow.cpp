#include "pch.h"
#include "HUDController.h"
#include "GameFlow.h"
#include "Keybinds.h"
#include "HudText.h"
#include "UiStrings.h"
#include "UnitStatus.h"
#include "ChampionKits.h"
#include "ItemsClient.h"
#include "PlayerController.h"
#include "BasePlayerController.h"
#include "Camera.h"
#include "Texture.h"
#include "MapUnits.h"

namespace
{
const Vec4 Gold(0.78f, 0.67f, 0.43f, 1.f);
const Vec4 Panel(0.02f, 0.05f, 0.08f, 0.96f);
const Vec4 PanelLight(0.1f, 0.16f, 0.18f, 1.f);
const Vec4 Hover(0.2f, 0.3f, 0.32f, 1.f);

bool Inside(const Vec2& p, const Vec4& r)
{
    return p.x >= r.x && p.x <= r.z && p.y >= r.y && p.y <= r.w;
}

string Clock(float seconds)
{
    const int32 s = max(0, static_cast<int32>(seconds));
    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%d:%02d", s / 60, s % 60);

    return buffer;
}
}

bool HUDController::Project(const Vec3& world, Vec2& screen) const
{
    auto cameraObject = CUR_SCENE ? CUR_SCENE->GetMainCamera() : nullptr;

    if (!cameraObject || !cameraObject->GetCamera())
        return false;
    auto camera = cameraObject->GetCamera();
    const Vec4 clip =
        Vec4::Transform(Vec4(world.x, world.y, world.z, 1.f), camera->GetViewMatrix() * camera->GetProjectionMatrix());

    if (clip.w <= 0.0001f)
        return false;
    const auto& desc = GAME->GetGameDesc();
    screen = Vec2((clip.x / clip.w * 0.5f + 0.5f) * desc.width, (0.5f - clip.y / clip.w * 0.5f) * desc.height);

    return true;
}

void HUDController::BuildUnitOverlays(PlayerController& me)
{
    const auto& desc = GAME->GetGameDesc();
    const float scale = desc.height / 1080.f;
    const float now = TIME->GetGameTime();
    auto text = HudText::Get();
    vector<shared_ptr<BasePlayerController>> units;
    units.push_back(GAMEMANAGER->_myPlayer);

    for (const auto& [id, object] : CUR_SCENE->_players)
        if (object && (!me._playerInfo || id != me._playerInfo->objectid()))
            if (auto controller = object->GetScript<BasePlayerController>(); controller && controller->_playerInfo)
                units.push_back(controller);

    for (const auto& unit : units)
    {
        if (!unit || !unit->_playerInfo || unit->_playerInfo->hp() <= 0)
            continue;
        const UnitStatus::State* state = UnitStatus::Find(unit->_playerInfo->objectid());

        if (!state)
            continue;
        const Vec3 feet = unit->GetTransform()->GetPosition();

        if (state->recallEndsAt > now && state->recallDuration > 0.f)
        {
            Vec2 screen;

            if (!Project(feet + Vec3(0.f, 3.6f, 0.f), screen))
                continue;
            const float width = 110.f * scale, height = 9.f * scale;
            const float fraction = 1.f - (state->recallEndsAt - now) / state->recallDuration;
            AddSolid(Vec4(screen.x - width * 0.5f - 1.f, screen.y - 1.f, screen.x + width * 0.5f + 1.f,
                          screen.y + height + 1.f),
                     900, Vec4(0.f, 0.f, 0.f, 0.8f));
            AddSolid(Vec4(screen.x - width * 0.5f, screen.y,
                          screen.x - width * 0.5f + width * std::clamp(fraction, 0.f, 1.f), screen.y + height),
                     901, Vec4(0.55f, 0.75f, 1.f, 1.f));
            text->Draw(UiStrings::Get("recall.channel"), Vec2(screen.x, screen.y - 9.f * scale), 13.f * scale,
                       HudText::Color::Mana);
        }

        if (state->turretPlates > 0)
        {
            Vec2 screen;

            if (!Project(feet + Vec3(0.f, 5.2f, 0.f), screen))
                continue;
            const float pip = 7.f * scale, gap = 3.f * scale;
            const float left = screen.x - (state->turretPlates * pip + (state->turretPlates - 1) * gap) * 0.5f;

            for (int32 i = 0; i < state->turretPlates; ++i)
                AddSolid(Vec4(left + i * (pip + gap), screen.y, left + i * (pip + gap) + pip, screen.y + pip), 900,
                         Gold);
        }
    }
}

void HUDController::BuildSurrender(PlayerController& me)
{
    const auto& desc = GAME->GetGameDesc();
    const float scale = desc.height / 1080.f;
    const float now = TIME->GetGameTime();
    auto text = HudText::Get();

    if (!GameFlow::VersionWarning().empty())
    {
        AddSolid(Vec4(0.f, desc.height * 0.09f, static_cast<float>(desc.width), desc.height * 0.125f), 1190,
                 Vec4(0.4f, 0.05f, 0.05f, 0.8f));
        text->Draw(GameFlow::VersionWarning(), Vec2(desc.width * 0.5f, desc.height * 0.1075f), 19.f * scale,
                   HudText::Color::White);
    }

    const GameFlow::SurrenderState& vote = GameFlow::Surrender();

    if (!vote.message.empty() && now < vote.messageUntil)
        text->Draw(vote.message, Vec2(desc.width * 0.5f, desc.height * 0.22f), 22.f * scale, HudText::Color::Gold);

    if (!vote.active || now > vote.endsAt)
        return;
    const float width = 300.f * scale, height = 120.f * scale;
    const float left = desc.width - width - 20.f * scale, top = desc.height * 0.3f;
    AddSolid(Vec4(left - 2.f, top - 2.f, left + width + 2.f, top + height + 2.f), 1120, Gold);
    AddSolid(Vec4(left, top, left + width, top + height), 1121, Panel);
    text->Draw(UiStrings::Get("surrender.title"), Vec2(left + width * 0.5f, top + 20.f * scale), 20.f * scale,
               HudText::Color::Gold);
    text->Draw(UiStrings::Format("surrender.count",
                                 {std::to_string(vote.yes), std::to_string(vote.needed), std::to_string(vote.no)}),
               Vec2(left + width * 0.5f, top + 48.f * scale), 17.f * scale, HudText::Color::White);
    text->Draw(Clock(vote.endsAt - now), Vec2(left + width - 14.f * scale, top + 20.f * scale), 16.f * scale,
               HudText::Color::Gray, HudText::Align::Right);

    if (vote.voted)
    {
        text->Draw(UiStrings::Get("surrender.voted"), Vec2(left + width * 0.5f, top + 88.f * scale), 16.f * scale,
                   HudText::Color::Gray);

        return;
    }

    const POINT mouse = INPUT->GetMousePos();
    const Vec2 cursor(static_cast<float>(mouse.x), static_cast<float>(mouse.y));
    const bool click = INPUT->GetButtonDown(KEY_TYPE::LBUTTON);

    for (int32 i = 0; i < 2; ++i)
    {
        const Vec4 button(left + 20.f * scale + i * (width * 0.5f - 10.f * scale), top + 72.f * scale,
                          left + width * 0.5f - 10.f * scale + i * (width * 0.5f - 10.f * scale), top + 104.f * scale);
        const bool over = Inside(cursor, button);

        if (over)
            s_mouseOverHud = true;
        AddSolid(button, 1122, over ? Hover : PanelLight);
        text->Draw(UiStrings::Get(i == 0 ? "surrender.yes" : "surrender.no"),
                   Vec2((button.x + button.z) * 0.5f, (button.y + button.w) * 0.5f), 17.f * scale,
                   HudText::Color::Gold);

        if (click && over)
            GameFlow::SendSurrender(i == 0);
    }
}

void HUDController::BuildDeathRecap(PlayerController& me)
{
    const GameFlow::Recap& recap = GameFlow::DeathRecap();
    const UnitStatus::State* mine = me._playerInfo ? UnitStatus::Find(me._playerInfo->objectid()) : nullptr;
    const float now = TIME->GetGameTime();

    if (!recap.shown || !mine || mine->respawnAt <= now)
        return;
    const auto& desc = GAME->GetGameDesc();
    const float scale = desc.height / 1080.f;
    auto text = HudText::Get();
    const float width = 360.f * scale, rowHeight = 26.f * scale;
    const float height = 70.f * scale + recap.entries.size() * rowHeight;
    const float left = 20.f * scale, top = desc.height * 0.3f;
    AddSolid(Vec4(left - 2.f, top - 2.f, left + width + 2.f, top + height + 2.f), 1120, Vec4(0.6f, 0.2f, 0.2f, 1.f));
    AddSolid(Vec4(left, top, left + width, top + height), 1121, Panel);
    text->Draw(UiStrings::Get("recap.title"), Vec2(left + 14.f * scale, top + 20.f * scale), 20.f * scale,
               HudText::Color::Red, HudText::Align::Left);
    text->Draw(UiStrings::Format("recap.total",
                                 {std::to_string(recap.total), std::to_string(static_cast<int32>(recap.seconds))}),
               Vec2(left + width - 14.f * scale, top + 20.f * scale), 15.f * scale, HudText::Color::Gray,
               HudText::Align::Right);
    float y = top + 56.f * scale;
    static const char* const SkillNames[] = {"P", "Q", "W", "E", "R"};

    for (const GameFlow::RecapEntry& entry : recap.entries)
    {
        const wstring icon = KitIcon(entry.sourceType);

        if (!icon.empty() && FindKit(entry.sourceType))
            _sprites.push_back(
                {1122,
                 RESOURCES->Load<Texture>(L"HudIcon_" + icon + L"_circle",
                                          L"..\\Resources\\Textures\\UI\\Hud\\Champions\\" + icon + L"_circle.dds"),
                 Vec4(left + 12.f * scale, y - 10.f * scale, left + 32.f * scale, y + 10.f * scale), Vec2(0.f, 0.f),
                 Vec2(1.f, 1.f), Vec4(1.f)});
        string what = entry.skillId >= 1 && entry.skillId <= 4 ? SkillNames[entry.skillId]
                      : entry.skillId == 0                     ? UiStrings::Get("recap.attack")
                      : entry.skillId == -2                    ? UiStrings::Get("recap.summoner")
                                                               : UiStrings::Get("recap.passive");
        const string who = entry.name.empty() ? UiStrings::Get("recap.unknown") : entry.name;
        text->Draw(who + "  " + what, Vec2(left + 40.f * scale, y), 16.f * scale, HudText::Color::White,
                   HudText::Align::Left);
        const float share = recap.total > 0 ? entry.damage / static_cast<float>(recap.total) : 0.f;
        AddSolid(Vec4(left + width - 120.f * scale, y + 6.f * scale,
                      left + width - 120.f * scale + 100.f * scale * share, y + 9.f * scale),
                 1122,
                 entry.damageType == 1   ? Vec4(0.5f, 0.6f, 1.f, 1.f)
                 : entry.damageType == 2 ? Vec4(1.f, 1.f, 1.f, 1.f)
                                         : Vec4(1.f, 0.6f, 0.3f, 1.f));
        text->Draw(std::to_string(entry.damage), Vec2(left + width - 14.f * scale, y), 16.f * scale,
                   entry.damageType == 1   ? HudText::Color::Magic
                   : entry.damageType == 2 ? HudText::Color::White
                                           : HudText::Color::Physical,
                   HudText::Align::Right);
        y += rowHeight;
    }
}

void HUDController::BuildMatchSummary(PlayerController& me)
{
    const GameFlow::Summary& summary = GameFlow::MatchSummary();

    if (!summary.shown)
        return;
    s_mouseOverHud = true;
    const auto& desc = GAME->GetGameDesc();
    const float scale = desc.height / 1080.f;
    auto text = HudText::Get();
    const int32 myTeam = me._playerInfo ? me._playerInfo->teamid() : 0;
    const float width = 1100.f * scale, rowHeight = 44.f * scale;
    const float height = 150.f * scale + (summary.entries.size() + 2) * rowHeight + 70.f * scale;
    const float left = (desc.width - width) * 0.5f, top = (desc.height - height) * 0.5f;
    AddSolid(Vec4(left - 2.f, top - 2.f, left + width + 2.f, top + height + 2.f), 1300, Gold);
    AddSolid(Vec4(left, top, left + width, top + height), 1301, Panel);
    const bool won = summary.winningTeam == myTeam;
    text->Draw(UiStrings::Get(won ? "gameover.victory" : "gameover.defeat"),
               Vec2(left + width * 0.5f, top + 46.f * scale), 44.f * scale,
               won ? HudText::Color::Gold : HudText::Color::Red);
    text->Draw(
        UiStrings::Format(summary.surrendered ? "summary.surrendered" : "summary.time", {Clock(summary.gameTime)}),
        Vec2(left + width * 0.5f, top + 92.f * scale), 17.f * scale, HudText::Color::Gray);
    const float colName = left + 60.f * scale, colLevel = left + width * 0.34f, colKda = left + width * 0.42f,
                colCs = left + width * 0.53f, colGold = left + width * 0.6f, colDamage = left + width * 0.69f,
                colTaken = left + width * 0.78f, colItems = left + width * 0.84f;
    float y = top + 130.f * scale;

    for (const auto& [key, x] : {pair{"score.level", colLevel}, pair{"score.kda", colKda}, pair{"score.cs", colCs},
                                 pair{"summary.gold", colGold}, pair{"summary.damage", colDamage},
                                 pair{"summary.taken", colTaken}, pair{"summary.items", colItems + 60.f * scale}})
        text->Draw(UiStrings::Get(key), Vec2(x, y), 15.f * scale, HudText::Color::Gray);
    y += rowHeight * 0.6f;

    for (int32 team = 1; team <= 2; ++team)
    {
        text->Draw(UiStrings::Get(team == 1 ? "score.blue" : "score.red"),
                   Vec2(left + 20.f * scale, y + rowHeight * 0.5f), 17.f * scale,
                   team == 1 ? HudText::Color::Mana : HudText::Color::Red, HudText::Align::Left);
        y += rowHeight * 0.7f;

        for (const GameFlow::SummaryEntry& e : summary.entries)
        {
            if (e.team != team)
                continue;
            const float cy = y + rowHeight * 0.5f;

            if (me._playerInfo && e.objectId == me._playerInfo->objectid())
                AddSolid(Vec4(left + 8.f * scale, y + 2.f, left + width - 8.f * scale, y + rowHeight - 2.f), 1302,
                         Vec4(0.47f, 0.36f, 0.18f, 0.18f));
            const wstring icon = KitIcon(e.champion);

            if (FindKit(e.champion))
                _sprites.push_back(
                    {1303,
                     RESOURCES->Load<Texture>(L"HudIcon_" + icon + L"_circle",
                                              L"..\\Resources\\Textures\\UI\\Hud\\Champions\\" + icon + L"_circle.dds"),
                     Vec4(left + 20.f * scale, cy - 16.f * scale, left + 52.f * scale, cy + 16.f * scale),
                     Vec2(0.f, 0.f), Vec2(1.f, 1.f), Vec4(1.f)});
            text->Draw(e.name, Vec2(colName, cy), 16.f * scale, HudText::Color::White, HudText::Align::Left);
            text->Draw(std::to_string(e.level), Vec2(colLevel, cy), 16.f * scale);
            char kda[32];
            snprintf(kda, sizeof(kda), "%d / %d / %d", e.kills, e.deaths, e.assists);
            text->Draw(kda, Vec2(colKda, cy), 16.f * scale);
            text->Draw(std::to_string(e.cs), Vec2(colCs, cy), 16.f * scale);
            text->Draw(std::to_string(e.gold), Vec2(colGold, cy), 16.f * scale, HudText::Color::Gold);
            text->Draw(std::to_string(e.damageToChampions), Vec2(colDamage, cy), 16.f * scale,
                       HudText::Color::Physical);
            text->Draw(std::to_string(e.damageTaken), Vec2(colTaken, cy), 16.f * scale, HudText::Color::Gray);

            for (size_t slot = 0; slot < e.items.size() && slot < 6; ++slot)
                if (e.items[slot] != 0)
                    if (auto itemIcon = Items::Icon(e.items[slot]))
                    {
                        const float x = colItems + slot * 24.f * scale;
                        _sprites.push_back({1303, itemIcon,
                                            Vec4(x, cy - 11.f * scale, x + 22.f * scale, cy + 11.f * scale),
                                            Vec2(0.f, 0.f), Vec2(1.f, 1.f), Vec4(1.f)});
                    }
            y += rowHeight;
        }
    }

    const POINT mouse = INPUT->GetMousePos();
    const Vec2 cursor(static_cast<float>(mouse.x), static_cast<float>(mouse.y));
    const Vec4 button(left + width * 0.5f - 90.f * scale, top + height - 50.f * scale,
                      left + width * 0.5f + 90.f * scale, top + height - 16.f * scale);
    const bool over = Inside(cursor, button);
    AddSolid(button, 1302, over ? Hover : PanelLight);
    text->Draw(UiStrings::Get("summary.exit"), Vec2((button.x + button.z) * 0.5f, (button.y + button.w) * 0.5f),
               18.f * scale, HudText::Color::Gold);

    if (!summary.savedTo.empty())
        text->Draw(UiStrings::Get("summary.saved"), Vec2(left + 20.f * scale, top + height - 33.f * scale),
                   13.f * scale, HudText::Color::Gray, HudText::Align::Left);

    if (over && INPUT->GetButtonDown(KEY_TYPE::LBUTTON))
        ::PostQuitMessage(0);
}

void HUDController::BuildRoomOverlay(PlayerController& me)
{
    const GameFlow::RoomState& room = GameFlow::Room();

    if (!room.paused && room.closedAt <= 0.0)
        return;
    const auto& desc = GAME->GetGameDesc();
    const float scale = desc.height / 1080.f;
    auto text = HudText::Get();

    const float width = 620.f * scale, height = 150.f * scale;
    const float left = (desc.width - width) * 0.5f, top = desc.height * 0.36f;
    AddSolid(Vec4(left - 2.f, top - 2.f, left + width + 2.f, top + height + 2.f), 1181, Gold);
    AddSolid(Vec4(left, top, left + width, top + height), 1182, Panel);

    if (room.closedAt > 0.0)
    {
        text->Draw(room.closedReason.empty() ? UiStrings::Get("room.closed") : room.closedReason,
                   Vec2(desc.width * 0.5f, top + 50.f * scale), 24.f * scale, HudText::Color::Gold);
        text->Draw(UiStrings::Get("room.closing"), Vec2(desc.width * 0.5f, top + 100.f * scale), 18.f * scale,
                   HudText::Color::Gray);

        if (GameFlow::RealSeconds() - room.closedAt > 4.0)
            ::PostMessage(desc.hWnd, WM_CLOSE, 0, 0);

        return;
    }

    string names;

    for (const string& name : room.waitingFor)
        names += (names.empty() ? "" : ", ") + name;
    text->Draw(UiStrings::Get("room.paused"), Vec2(desc.width * 0.5f, top + 34.f * scale), 26.f * scale,
               HudText::Color::Gold);
    text->Draw(UiStrings::Format("room.waiting", {names}), Vec2(desc.width * 0.5f, top + 76.f * scale), 19.f * scale,
               HudText::Color::White);
    text->Draw(UiStrings::Format("room.seconds",
                                 {std::to_string(static_cast<int32>(std::ceil(GameFlow::PauseSecondsLeft())))}),
               Vec2(desc.width * 0.5f, top + 116.f * scale), 30.f * scale, HudText::Color::Red);
}

float HUDController::OptionsExtraRows() const
{
    return 1.f + static_cast<float>(Keybinds::Action::Count) * 0.72f + (GameFlow::PracticeEnabled() ? 2.7f : 0.f) +
           1.f + (GameFlow::IAmHost() ? 1.f : 0.f);
}

void HUDController::BuildOptionsExtras(float left, float y, float width, float rowHeight, float scale,
                                       const Vec2& cursor, bool down)
{
    auto text = HudText::Get();
    constexpr int32 Layer = 1402;
    auto button = [&](const Vec4& rect, const string& label, bool highlight = false)
    {
        const bool over = Inside(cursor, rect);
        AddSolid(rect, Layer, highlight ? Vec4(0.47f, 0.36f, 0.18f, 0.9f) : over ? Hover : PanelLight);
        text->Draw(label, Vec2((rect.x + rect.z) * 0.5f, (rect.y + rect.w) * 0.5f), 16.f * scale, HudText::Color::Gold);

        return down && over;
    };
    // Quick cast.
    text->Draw(UiStrings::Get("options.quickcast"), Vec2(left + 24.f * scale, y), 19.f * scale, HudText::Color::White,
               HudText::Align::Left);

    if (button(Vec4(left + width * 0.62f, y - 14.f * scale, left + width * 0.86f, y + 14.f * scale),
               UiStrings::Get(Keybinds::QuickCast() ? "options.on" : "options.off"), Keybinds::QuickCast()))
    {
        Keybinds::SetQuickCast(!Keybinds::QuickCast());
        Keybinds::Save();
    }

    y += rowHeight;

    const float keyRow = rowHeight * 0.72f;

    for (int32 i = 0; i < static_cast<int32>(Keybinds::Action::Count); ++i)
    {
        const auto action = static_cast<Keybinds::Action>(i);
        text->Draw(UiStrings::Get(Keybinds::LabelKey(action)), Vec2(left + 24.f * scale, y), 17.f * scale,
                   HudText::Color::Gray, HudText::Align::Left);
        const bool waiting = _rebinding == i;

        if (button(Vec4(left + width * 0.62f, y - 11.f * scale, left + width * 0.86f, y + 11.f * scale),
                   waiting ? UiStrings::Get("options.press_key") : Keybinds::KeyName(Keybinds::Key(action)), waiting))
            _rebinding = waiting ? -1 : i;
        y += keyRow;
    }

    if (_rebinding >= 0)
    {
        const int32 code = INPUT->FirstKeyDown();

        if (code == VK_ESCAPE)
            _rebinding = -1;
        else if (code > 0 && code != VK_LBUTTON && code != VK_RBUTTON && code != VK_MBUTTON)
        {
            Keybinds::SetKey(static_cast<Keybinds::Action>(_rebinding), static_cast<KEY_TYPE>(code));
            Keybinds::Save();
            _rebinding = -1;
        }
    }

    y += rowHeight * 0.2f;
    // The practice tool.
    if (GameFlow::PracticeEnabled())
    {
        text->Draw(UiStrings::Get("options.practice"), Vec2(left + 24.f * scale, y), 19.f * scale, HudText::Color::Gold,
                   HudText::Align::Left);
        y += rowHeight * 0.75f;

        struct Tool
        {
            const char* key;
            Protocol::PRACTICE_ACTION action;
        };

        const Tool tools[] = {
            {"practice.cooldowns", Protocol::PRACTICE_RESET_COOLDOWNS},
            {"practice.gold", Protocol::PRACTICE_ADD_GOLD},
            {"practice.level", Protocol::PRACTICE_LEVEL_UP},
            {"practice.max_level", Protocol::PRACTICE_MAX_LEVEL},
            {"practice.heal", Protocol::PRACTICE_HEAL},
            {"practice.dummy", Protocol::PRACTICE_SPAWN_DUMMY},
            {"practice.clear", Protocol::PRACTICE_CLEAR_DUMMIES},
        };

        const float bw = (width - 48.f * scale - 3.f * 8.f * scale) / 4.f;

        for (int32 i = 0; i < static_cast<int32>(std::size(tools)); ++i)
        {
            const float bx = left + 24.f * scale + (i % 4) * (bw + 8.f * scale), by = y + (i / 4) * rowHeight * 0.8f;

            if (button(Vec4(bx, by - 15.f * scale, bx + bw, by + 15.f * scale), UiStrings::Get(tools[i].key)))
            {
                Vec3 at = Vec3::Zero;

                if (tools[i].action == Protocol::PRACTICE_SPAWN_DUMMY)
                    if (auto champion = GAMEMANAGER->_myPlayer)
                        at = champion->GetTransform()->GetPosition() + Vec3(3.f, 0.f, 0.f);
                GameFlow::SendPractice(tools[i].action, at);
            }
        }

        y += rowHeight * 1.9f;
    }

    // Surrender.
    if (button(Vec4(left + width * 0.5f - 90.f * scale, y - 15.f * scale, left + width * 0.5f + 90.f * scale,
                    y + 15.f * scale),
               UiStrings::Get("surrender.button")))
    {
        GameFlow::SendSurrender(true);
        ToggleOptions();
    }

    if (GameFlow::IAmHost())
    {
        y += rowHeight;
        static double armedUntil = 0.0;
        const bool armed = GameFlow::RealSeconds() < armedUntil;

        if (button(Vec4(left + width * 0.5f - 130.f * scale, y - 15.f * scale, left + width * 0.5f + 130.f * scale,
                        y + 15.f * scale),
                   UiStrings::Get(armed ? "options.force_end_confirm" : "options.force_end"), armed))
        {
            if (armed)
            {
                armedUntil = 0.0;
                GameFlow::SendForceEnd();
                ToggleOptions();
            }
            else
                armedUntil = GameFlow::RealSeconds() + 3.0;
        }
    }
}
