// HUD 표시.
#include "pch.h"
#include "HUDController.h"
#include "Announcements.h"
#include "ChampionKits.h"
#include "UiStrings.h"
#include "TooltipText.h"
#include "MapUnits.h"
#include "CameraController.h"
#include "FogOfWar.h"
#include "Camera.h"
#include "SummonerSpellsClient.h"
#include "Material.h"
#include "Texture.h"
#include "PlayerController.h"
#include "HudText.h"
#include "UnitStatus.h"
#include "ItemsClient.h"
#include "Pings.h"
#include "GameFlow.h"
#include "Keybinds.h"
#include <fstream>

namespace
{
const wstring LayoutRoot = L"..\\Resources\\Data\\UI\\";
const wstring TextureRoot = L"..\\Resources\\Textures\\UI\\Hud\\";
constexpr size_t MaxSprites = 4096;
constexpr int32 SweepSegments = 24;
const char* const Hotkeys[4] = {"Q", "W", "E", "R"};
const Vec4 CooldownShade(0.f, 0.f, 0.f, 0.62f);
const Vec4 DeadPortrait(0.35f, 0.35f, 0.35f, 1.f);

Vec2 ReadVec2(const json& value, const Vec2& fallback)
{
    return value.is_array() && value.size() >= 2 ? Vec2(value[0].get<float>(), value[1].get<float>()) : fallback;
}

string LastPart(const string& name)
{
    const size_t slash = name.rfind('/');

    return slash == string::npos ? name : name.substr(slash + 1);
}
}

shared_ptr<HUDController> HUDController::Find()
{
    auto hud = UI->GetHUD();

    return hud ? hud->GetScript<HUDController>() : nullptr;
}

void HUDController::Awake()
{
    LoadLayout(L"JadePlayerFrame.json");
    LoadLayout(L"JadePlayerInventory.json");
    LoadLayout(L"JadeMinimap.json");
    _minimap = RESOURCES->Load<Texture>(L"HudMinimap", TextureRoot + L"Minimap.dds");

    _vertices.resize(MaxSprites * 4);
    vector<uint32> indices;
    indices.reserve(MaxSprites * 6);

    for (uint32 index = 0; index < _vertices.size(); index += 4)
        indices.insert(indices.end(), {index, index + 1, index + 2, index + 2, index + 1, index + 3});
    _mesh = make_shared<Mesh>();
    _mesh->CreateDynamic(_vertices, indices);
    _material = make_shared<Material>();
    _material->SetShader(make_shared<Shader>(L"Hud.fx"));
}

void HUDController::LoadLayout(const wstring& file)
{
    std::ifstream stream(LayoutRoot + file);
    json root = json::parse(stream, nullptr, false);

    if (root.is_discarded() || !root.contains("elements"))
    {
        DEBUG_LOG("[HUD] missing layout " << file.c_str() << "");

        return;
    }

    for (const json& source : root["elements"])
    {
        if (!source.contains("pos") || !source["pos"].is_array())
            continue;
        Element element;
        element.pos = ReadVec2(source["pos"], Vec2::Zero);
        element.size = ReadVec2(source.value("size", json()), Vec2::Zero);
        element.ref = ReadVec2(source.value("ref", json()), Vec2(1600.f, 1200.f));
        element.anchor = ReadVec2(source.value("anchor", json()), Vec2::Zero);
        element.layer = source.value("layer", 0);
        element.text = source.value("text", false);

        if (source.contains("color") && source["color"].is_array() && source["color"].size() == 4)
            element.color = Vec4(source["color"][0].get<float>(), source["color"][1].get<float>(),
                                 source["color"][2].get<float>(), source["color"][3].get<float>()) /
                            255.f;

        if (source.contains("texture") && source.contains("uv") && source["uv"].size() == 4)
        {
            const string texture = source["texture"].get<string>();
            const wstring key = L"Hud_" + Utils::ToWString(texture);
            element.texture = RESOURCES->Get<Texture>(key);

            if (!element.texture)
                element.texture = RESOURCES->Load<Texture>(key, TextureRoot + Utils::ToWString(texture));
            const Vec2 size = ReadVec2(source.value("textureSize", json()), Vec2(1.f));
            const json& uv = source["uv"];
            element.uv0 = Vec2(uv[0].get<float>() / max(1.f, size.x), uv[1].get<float>() / max(1.f, size.y));
            element.uv1 = Vec2(uv[2].get<float>() / max(1.f, size.x), uv[3].get<float>() / max(1.f, size.y));
        }

        _elements[LastPart(source["name"].get<string>())] = element;
    }
}

void HUDController::LoadChampionIcons(Protocol::PLAYER_CHAMPION_TYPE champion)
{
    _iconsFor = champion;
    const wstring name = KitIcon(champion);
    auto load = [&](const wstring& key)
    {
        const wstring file = TextureRoot + L"Champions\\" + name + L"_" + key + L".dds";

        return RESOURCES->Load<Texture>(L"HudIcon_" + name + L"_" + key, file);
    };
    _portrait = load(L"circle");
    _passive = load(L"passive");
    const wchar_t* spells[4] = {L"q", L"w", L"e", L"r"};

    for (int32 i = 0; i < 4; ++i)
        _spells[i] = load(spells[i]);
}

const HUDController::Element* HUDController::Get(const string& name) const
{
    auto it = _elements.find(name);

    return it == _elements.end() ? nullptr : &it->second;
}

Vec4 HUDController::ScreenRect(const Element& element) const
{
    const auto& desc = GAME->GetGameDesc();
    const float scale = desc.height / element.ref.y;
    const float x = element.anchor.x * desc.width + (element.pos.x - element.anchor.x * element.ref.x) * scale;
    const float y = element.anchor.y * desc.height + (element.pos.y - element.anchor.y * element.ref.y) * scale;

    return Vec4(x, y, x + element.size.x * scale, y + element.size.y * scale);
}

bool HUDController::GetElementRect(const string& name, Vec4& rect) const
{
    const Element* element = Get(name);

    if (!element)
        return false;
    rect = ScreenRect(*element);

    return true;
}

void HUDController::Add(const string& name, const Vec4& color)
{
    const Element* element = Get(name);

    if (!element || (!element->texture && element->color.w <= 0.f))
        return;
    _sprites.push_back(
        {element->layer, element->texture, ScreenRect(*element), element->uv0, element->uv1, element->color * color});
}

void HUDController::AddCropped(const string& name, float fraction, const Vec4& color)
{
    const Element* element = Get(name);
    fraction = std::clamp(fraction, 0.f, 1.f);

    if (!element || !element->texture || fraction <= 0.f)
        return;
    Vec4 rect = ScreenRect(*element);
    rect.z = rect.x + (rect.z - rect.x) * fraction;
    const Vec2 uv1(element->uv0.x + (element->uv1.x - element->uv0.x) * fraction, element->uv1.y);
    _sprites.push_back({element->layer, element->texture, rect, element->uv0, uv1, element->color * color});
}

void HUDController::AddIcon(const string& name, const shared_ptr<Texture>& icon, const Vec4& color)
{
    const Element* element = Get(name);

    if (!element || !icon)
        return;
    _sprites.push_back({element->layer, icon, ScreenRect(*element), Vec2(0.f, 0.f), Vec2(1.f, 1.f), color});
}

void HUDController::AddSolid(const Vec4& rect, int32 layer, const Vec4& color)
{
    _sprites.push_back({layer, nullptr, rect, Vec2(-1.f), Vec2(-1.f), color});
}

void HUDController::AddSweep(const Vec4& rect, int32 layer, float remaining, const Vec4& color)
{
    remaining = std::clamp(remaining, 0.f, 1.f);

    if (remaining <= 0.f)
        return;
    const Vec2 center((rect.x + rect.z) * 0.5f, (rect.y + rect.w) * 0.5f);
    const float radius = max(rect.z - rect.x, rect.w - rect.y);
    const float start = XM_2PI * (1.f - remaining);
    const int32 segments = max(1, (int32)std::ceil(SweepSegments * remaining));
    auto point = [&](float angle)
    {
        Vec2 p(center.x + sinf(angle) * radius, center.y - cosf(angle) * radius);
        p.x = std::clamp(p.x, rect.x, rect.z);
        p.y = std::clamp(p.y, rect.y, rect.w);

        return p;
    };

    for (int32 i = 0; i < segments; ++i)
    {
        const float a0 = start + (XM_2PI - start) * i / segments;
        const float a1 = start + (XM_2PI - start) * (i + 1) / segments;
        const float mid = (a0 + a1) * 0.5f;
        Sprite slice{layer, nullptr, Vec4(0.f), Vec2(0.f), Vec2(0.f), color, true};
        const Vec2 p0 = point(a0), pm = point(mid), p1 = point(a1);
        slice.rect = Vec4(center.x, center.y, p0.x, p0.y);
        slice.uv0 = pm;
        slice.uv1 = p1;
        _sprites.push_back(slice);
    }
}

void HUDController::Text(const string& name, const string& value, float scale, int32 color, int32 align)
{
    const Element* element = Get(name);

    if (!element || value.empty())
        return;
    const Vec4 rect = ScreenRect(*element);
    const float height = (rect.w - rect.y) * 1.25f * scale;
    const HudText::Align alignment = align == 0   ? HudText::Align::Left
                                     : align == 2 ? HudText::Align::Right
                                                  : HudText::Align::Center;
    const float x = align == 0 ? rect.x : align == 2 ? rect.z : (rect.x + rect.z) * 0.5f;
    HudText::Get()->Draw(value, Vec2(x, (rect.y + rect.w) * 0.5f), max(8.f, height), static_cast<HudText::Color>(color),
                         alignment);
}

void HUDController::ShowRejection(Protocol::SKILL_REJECT_REASON reason)
{
    switch (reason)
    {
    case Protocol::SKILL_REJECT_MANA:
        _message = UiStrings::Get("reject.mana");
        break;
    case Protocol::SKILL_REJECT_COOLDOWN:
        _message = UiStrings::Get("reject.cooldown");
        break;
    case Protocol::SKILL_REJECT_STUNNED:
        _message = UiStrings::Get("reject.stunned");
        break;
    case Protocol::SKILL_REJECT_SILENCED:
        _message = UiStrings::Get("reject.silenced");
        break;
    case Protocol::SKILL_REJECT_DEAD:
        _message = UiStrings::Get("reject.dead");
        break;
    case Protocol::SKILL_REJECT_NOT_LEARNED:
        _message = UiStrings::Get("reject.not_learned");
        break;
    case Protocol::SKILL_REJECT_GOLD:
        _message = UiStrings::Get("reject.gold");
        break;
    case Protocol::SKILL_REJECT_SHOP_RANGE:
        _message = UiStrings::Get("reject.shop_range");
        break;
    case Protocol::SKILL_REJECT_INVENTORY:
        _message = UiStrings::Get("reject.inventory");
        break;
    default:
        return;
    }

    _messageEndsAt = TIME->GetGameTime() + MessageSeconds;
}

namespace
{
const wstring AudioSettingsFile = L"..\\Saved\\AudioSettings.json";

// Options rows: the master volume, then each channel.
struct VolumeRow
{
    const char* key;
    const char* label;
    int32 group;
};

const VolumeRow VolumeRows[] = {
    {"master", "options.master", -1},
    {"music", "options.music", static_cast<int32>(SoundGroup::Music)},
    {"announcer", "options.announcer", static_cast<int32>(SoundGroup::Announcer)},
    {"voice", "options.voice", static_cast<int32>(SoundGroup::Voice)},
    {"sfx", "options.sfx", static_cast<int32>(SoundGroup::Sfx)},
    {"ui", "options.ui", static_cast<int32>(SoundGroup::Ui)},
    {"ambience", "options.ambience", static_cast<int32>(SoundGroup::Ambience)},
};

float RowVolume(const VolumeRow& row)
{
    return row.group < 0 ? SOUND->GetMasterVolume() : SOUND->GetGroupVolume(static_cast<SoundGroup>(row.group));
}

void SetRowVolume(const VolumeRow& row, float volume)
{
    if (row.group < 0)
        SOUND->SetMasterVolume(volume);
    else
        SOUND->SetGroupVolume(static_cast<SoundGroup>(row.group), volume);
}
}

void HUDController::LoadAudioSettings()
{
    SOUND->SetGroupVolume(SoundGroup::Music, 0.f);
    std::ifstream file(AudioSettingsFile);
    json j = json::parse(file, nullptr, false);

    if (j.is_discarded())
    {
        ApplyMuteOverride();

        return;
    }

    for (const VolumeRow& row : VolumeRows)
        if (j.contains(row.key) && j[row.key].is_number())
            SetRowVolume(row, j[row.key].get<float>());
    ApplyMuteOverride();
}

bool HUDController::MutedByEnvironment()
{
    char* value = nullptr;
    size_t length = 0;
    const bool muted = _dupenv_s(&value, &length, "DAWN_MUTE") == 0 && value && string(value) == "1";
    free(value);

    return muted && SOUND->GetAudioCapturePath().empty();
}

void HUDController::ApplyMuteOverride()
{
    if (MutedByEnvironment())
        SOUND->SetMasterVolume(0.f);
}

void HUDController::SaveAudioSettings()
{
    if (MutedByEnvironment())
        return;
    json j;

    for (const VolumeRow& row : VolumeRows)
        j[row.key] = std::round(RowVolume(row) * 100.f) / 100.f;
    std::error_code error;
    std::filesystem::create_directories(std::filesystem::path(AudioSettingsFile).parent_path(), error);
    std::ofstream file(AudioSettingsFile);
    file << j.dump(1);
}

void HUDController::ToggleOptions()
{
    _optionsOpen = !_optionsOpen;
    _optionsDrag = -1;
    SOUND->PlayCue(_optionsOpen ? "Play_sfx_hud_Menu_Open" : "Play_sfx_hud_Menu_Close");

    if (!_optionsOpen)
        SaveAudioSettings();
}

void HUDController::BuildOptions()
{
    if (!_optionsOpen)
        return;
    s_mouseOverHud = true;
    const auto& desc = GAME->GetGameDesc();
    const float scale = desc.height / 1080.f;
    const float width = 560.f * scale, rowHeight = 46.f * scale;
    const float height = rowHeight * (static_cast<float>(std::size(VolumeRows)) + 2.6f + OptionsExtraRows());
    const float left = (desc.width - width) * 0.5f, top = (desc.height - height) * 0.5f;
    constexpr int32 Layer = 1400;
    AddSolid(Vec4(0.f, 0.f, static_cast<float>(desc.width), static_cast<float>(desc.height)), Layer,
             Vec4(0.f, 0.f, 0.f, 0.45f));
    AddSolid(Vec4(left, top, left + width, top + height), Layer + 1, Vec4(0.03f, 0.06f, 0.08f, 0.96f));
    AddSolid(Vec4(left, top, left + width, top + 2.f * scale), Layer + 2, Vec4(0.78f, 0.67f, 0.43f, 1.f));
    auto text = HudText::Get();
    text->Draw(UiStrings::Get("options.title"), Vec2(left + width * 0.5f, top + rowHeight * 0.6f), 26.f * scale,
               HudText::Color::Gold);

    const POINT mouse = INPUT->GetMousePos();
    const Vec2 cursor(static_cast<float>(mouse.x), static_cast<float>(mouse.y));
    const bool down = INPUT->GetButtonDown(KEY_TYPE::LBUTTON), held = INPUT->GetButton(KEY_TYPE::LBUTTON);

    if (!held && _optionsDrag >= 0)
    {
        _optionsDrag = -1;
        SaveAudioSettings();
    }

    const float barLeft = left + width * 0.42f, barRight = left + width * 0.86f;

    for (int32 i = 0; i < static_cast<int32>(std::size(VolumeRows)); ++i)
    {
        const VolumeRow& row = VolumeRows[i];
        const float cy = top + rowHeight * (1.4f + i);
        text->Draw(UiStrings::Get(row.label), Vec2(left + 24.f * scale, cy), 19.f * scale, HudText::Color::White,
                   HudText::Align::Left);
        const Vec4 bar(barLeft, cy - 4.f * scale, barRight, cy + 4.f * scale);
        const Vec4 hit(barLeft - 8.f * scale, cy - 14.f * scale, barRight + 8.f * scale, cy + 14.f * scale);
        const bool inside = cursor.x >= hit.x && cursor.x <= hit.z && cursor.y >= hit.y && cursor.y <= hit.w;

        if (down && inside)
            _optionsDrag = i;

        if (_optionsDrag == i)
            SetRowVolume(row, std::clamp((cursor.x - barLeft) / (barRight - barLeft), 0.f, 1.f));
        const float volume = RowVolume(row);
        const float x = barLeft + (barRight - barLeft) * volume;
        AddSolid(bar, Layer + 2, Vec4(0.16f, 0.2f, 0.22f, 1.f));
        AddSolid(Vec4(barLeft, bar.y, x, bar.w), Layer + 3, Vec4(0.78f, 0.67f, 0.43f, 1.f));
        AddSolid(Vec4(x - 6.f * scale, cy - 11.f * scale, x + 6.f * scale, cy + 11.f * scale), Layer + 4,
                 _optionsDrag == i || inside ? Vec4(1.f, 0.93f, 0.7f, 1.f) : Vec4(0.86f, 0.78f, 0.55f, 1.f));
        text->Draw(std::to_string(static_cast<int32>(std::round(volume * 100.f))),
                   Vec2(left + width - 24.f * scale, cy), 18.f * scale, HudText::Color::Gray, HudText::Align::Right);
    }

    BuildOptionsExtras(left, top + rowHeight * (1.4f + static_cast<float>(std::size(VolumeRows))), width, rowHeight,
                       scale, cursor, down);

    // Close.
    const float cy = top + height - rowHeight * 0.75f;
    const Vec4 button(left + width * 0.5f - 70.f * scale, cy - 17.f * scale, left + width * 0.5f + 70.f * scale,
                      cy + 17.f * scale);
    const bool overButton =
        cursor.x >= button.x && cursor.x <= button.z && cursor.y >= button.y && cursor.y <= button.w;
    AddSolid(button, Layer + 2, overButton ? Vec4(0.2f, 0.3f, 0.32f, 1.f) : Vec4(0.1f, 0.16f, 0.18f, 1.f));
    text->Draw(UiStrings::Get("options.close"), Vec2(left + width * 0.5f, cy), 19.f * scale, HudText::Color::Gold);

    if (down && overButton)
        ToggleOptions();
}

void HUDController::ShowLevelUp()
{
    _levelUpAt = TIME->GetGameTime();
    _xpShown = 0.f;
}

float HUDController::GetLevelUpAge() const
{
    return TIME->GetGameTime() - _levelUpAt;
}

void HUDController::BuildSkillPoints(PlayerController& me)
{
    const auto& progress = UnitStatus::MyProgress();

    if (progress.skillPoints <= 0 || !me._playerInfo)
        return;
    const int32 level = max(1, progress.level);
    const float now = TIME->GetGameTime();
    const float pulse = 0.75f + 0.25f * std::sin(now * 6.f);
    bool any = false;

    for (int32 slot = 0; slot < 4; ++slot)
    {
        const int32 cap = slot == 3 ? (level >= 6) + (level >= 11) + (level >= 16) : min(5, (level + 1) / 2);

        if (progress.ranks[slot] >= cap)
            continue;
        any = true;
        const string prefix = "LevelUp" + std::to_string(slot) + "_Button";
        const Element* region = Get(prefix + "Region");
        const POINT mouse = INPUT->GetMousePos();
        const Vec4 rect = region ? ScreenRect(*region) : Vec4(0.f);
        const bool hover = region && mouse.x >= rect.x && mouse.x <= rect.z && mouse.y >= rect.y && mouse.y <= rect.w;
        Add(prefix + (hover ? "Hover" : "Default"), Vec4(1.f, 1.f, 1.f, hover ? 1.f : pulse));

        if (hover && INPUT->GetButtonDown(KEY_TYPE::LBUTTON))
            me.LevelUpSkill(slot + 1);
    }

    if (any)
        Text("LevelUp_Text", Announcements::Text("level_up"), 1.f, static_cast<int32>(HudText::Color::Gold));
}

void HUDController::ShowAnnouncement(const string& text, bool ally, int32 sourceType, int32 targetType)
{
    Announcement next{text, ally, sourceType, targetType};

    if (TIME->GetGameTime() < _announcementEndsAt)
    {
        if (_announcementQueue.size() < 4)
            _announcementQueue.push_back(next);

        return;
    }

    _announcement = next;
    _announcementEndsAt = TIME->GetGameTime() + AnnouncementSeconds;
}

void HUDController::BuildSummoner(PlayerController& me, int32 slot)
{
    static const char* const Hotkeys[2] = {"D", "F"};
    const string prefix = "Summoner" + std::to_string(slot) + "_";
    const auto& info = *me._playerInfo;
    const auto spell = info.summonerspells_size() > slot
                           ? static_cast<Protocol::SUMMONER_SPELL>(info.summonerspells(slot))
                           : Protocol::SUMMONER_NONE;
    Add(prefix + "TimerBorderCooldownBG");

    if (spell == Protocol::SUMMONER_NONE)
    {
        Add(prefix + "BorderUnavailable");

        return;
    }

    const wstring file = SummonerSpellsClient::IconFile(spell);
    _summonerIcons[slot] = RESOURCES->Load<Texture>(L"HudSummoner_" + file, file);
    AddIcon(prefix + "IconLoc", _summonerIcons[slot]);

    const auto& state = UnitStatus::MyProgress().summoners[slot];
    const float now = TIME->GetGameTime();
    float remaining = state.spell == spell ? max(0.f, state.readyAt - now) : 0.f;
    float duration = state.duration;
    // Smite without charges waits for the next one.
    if (state.spell == spell && state.rechargeAt > 0.f && state.charges <= 0 && state.rechargeAt - now > remaining)
    {
        remaining = state.rechargeAt - now;
        duration = 90.f;
    }

    if (remaining > 0.f)
    {
        if (const Element* icon = Get(prefix + "IconLoc"))
            AddSweep(ScreenRect(*icon), icon->layer, duration > 0.f ? min(1.f, remaining / duration) : 1.f,
                     CooldownShade);
        char buffer[16];

        if (remaining >= 1.f)
            snprintf(buffer, sizeof(buffer), "%d", (int32)std::ceil(remaining));
        else
            snprintf(buffer, sizeof(buffer), "%.1f", remaining);
        Text(prefix + "Cooldown", buffer, 1.6f);
    }

    const bool ready = remaining <= 0.f && info.hp() > 0;
    Add(ready ? prefix + "BorderAvailable" : prefix + "BorderUnavailable");

    if (spell == Protocol::SUMMONER_SMITE && state.spell == spell)
        Text(prefix + "Ammo", std::to_string(state.charges), 1.f, static_cast<int32>(HudText::Color::Gold));
    Text(prefix + "HotKey", Hotkeys[slot], 1.f, static_cast<int32>(HudText::Color::White));
}

namespace
{
string Clock(float seconds)
{
    const int32 total = max(0, static_cast<int32>(seconds));
    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%02d:%02d", total / 60, total % 60);

    return buffer;
}

vector<shared_ptr<BasePlayerController>> Champions()
{
    vector<shared_ptr<BasePlayerController>> out;

    if (auto me = GAMEMANAGER->_myPlayer)
        out.push_back(me);

    for (const auto& [id, object] : CUR_SCENE->_players)
        if (object)
            if (auto c = object->GetScript<BasePlayerController>();
                c && c != GAMEMANAGER->_myPlayer && c->_playerInfo && !IsMapUnit(c->_playerInfo->champtype()) &&
                c->_playerInfo->champtype() != Protocol::PLAYER_TYPE_TIBBERS)
                out.push_back(c);

    return out;
}
}

void HUDController::BuildTopBar(PlayerController& me)
{
    const auto& desc = GAME->GetGameDesc();
    const float scale = desc.height / 1080.f;
    const auto& info = *me._playerInfo;
    const UnitStatus::State* status = UnitStatus::Find(info.objectid());
    auto text = HudText::Get();

    const float width = 300.f * scale, height = 32.f * scale, size = 18.f * scale;
    const float right = desc.width - 12.f * scale, top = 8.f * scale;
    AddSolid(Vec4(right - width, top, right, top + height), 900, Vec4(0.004f, 0.04f, 0.07f, 0.82f));
    const float y = top + height * 0.5f;
    char kda[32];
    snprintf(kda, sizeof(kda), "%d / %d / %d", status ? status->kills : 0, status ? status->deaths : 0,
             status ? status->assists : 0);
    text->Draw(kda, Vec2(right - width + 14.f * scale, y), size, HudText::Color::White, HudText::Align::Left);
    text->Draw(UiStrings::Get("score.cs") + " " + std::to_string(status ? status->cs : 0),
               Vec2(right - width * 0.36f, y), size, HudText::Color::White, HudText::Align::Center);
    text->Draw(Clock(UnitStatus::GameTime()), Vec2(right - 12.f * scale, y), size, HudText::Color::Gold,
               HudText::Align::Right);

    // Top center: each team's kills.
    int32 kills[3] = {};

    for (const auto& champion : Champions())
        if (const UnitStatus::State* s = UnitStatus::Find(champion->_playerInfo->objectid());
            s && champion->_playerInfo->teamid() >= 1 && champion->_playerInfo->teamid() <= 2)
            kills[champion->_playerInfo->teamid()] += s->kills;
    const float cx = desc.width * 0.5f;
    AddSolid(Vec4(cx - 70.f * scale, top, cx + 70.f * scale, top + height), 900, Vec4(0.004f, 0.04f, 0.07f, 0.82f));
    text->Draw(std::to_string(kills[1]), Vec2(cx - 22.f * scale, y), size * 1.15f, HudText::Color::Mana,
               HudText::Align::Right);
    text->Draw(":", Vec2(cx, y), size, HudText::Color::Gray);
    text->Draw(std::to_string(kills[2]), Vec2(cx + 22.f * scale, y), size * 1.15f, HudText::Color::Red,
               HudText::Align::Left);
}

namespace
{
constexpr float MinimapWorld = 147.f;
}

bool HUDController::MinimapToWorld(const Vec2& pixel, Vec3& world) const
{
    const Vec4& r = _minimapRect;

    if (r.z <= r.x || pixel.x < r.x || pixel.x > r.z || pixel.y < r.y || pixel.y > r.w)
        return false;
    world =
        Vec3((pixel.x - r.x) / (r.z - r.x) * MinimapWorld, 2.f, (1.f - (pixel.y - r.y) / (r.w - r.y)) * MinimapWorld);

    return true;
}

Vec2 HUDController::WorldToMinimap(const Vec3& world) const
{
    const Vec4& r = _minimapRect;

    return Vec2(r.x + world.x / MinimapWorld * (r.z - r.x), r.y + (1.f - world.z / MinimapWorld) * (r.w - r.y));
}

void HUDController::BuildMinimap(PlayerController& me)
{
    const Element* content = Get("MinimapContent");

    if (!content || !_minimap)
        return;
    _minimapRect = ScreenRect(*content);
    const Vec4 r = _minimapRect;
    const float w = r.z - r.x, h = r.w - r.y;
    const int32 base = content->layer + 20;
    Add("MinimapFrameBoarder");
    Add("MinimapFrame");
    _sprites.push_back({base, _minimap, r, Vec2(0.f, 0.f), Vec2(1.f, 1.f), Vec4(1.f)});

    if (auto fog = FogOfWar::Get(); fog->IsLoaded())
    {
        constexpr int32 Cells = 36;

        for (int32 z = 0; z < Cells; ++z)
            for (int32 x = 0; x < Cells; ++x)
            {
                const float wx = (x + 0.5f) / Cells * MinimapWorld, wz = (1.f - (z + 0.5f) / Cells) * MinimapWorld;
                const float dark = (1.f - fog->SightAt(wx, wz)) * 0.55f;

                auto edgeX = [&](int32 i)
                {
                    return floorf(r.x + i * w / Cells);
                };
                auto edgeY = [&](int32 i)
                {
                    return floorf(r.y + i * h / Cells);
                };

                if (dark > 0.02f)
                    AddSolid(Vec4(edgeX(x), edgeY(z), edgeX(x + 1), edgeY(z + 1)), base + 1,
                             Vec4(0.f, 0.02f, 0.05f, dark));
            }
    }

    const int32 myTeam = me._playerInfo->teamid();
    auto teamColor = [&](int32 team)
    {
        return team == myTeam ? Vec4(0.22f, 0.62f, 1.f, 1.f)
               : team == 0    ? Vec4(0.95f, 0.75f, 0.25f, 1.f)
                              : Vec4(1.f, 0.25f, 0.22f, 1.f);
    };
    auto dot = [&](const Vec3& p, float size, const Vec4& color, int32 layer)
    {
        const Vec2 c = WorldToMinimap(p);
        AddSolid(Vec4(c.x - size, c.y - size, c.x + size, c.y + size), layer, color);
    };
    const float scale = h / 285.f;

    for (const auto& [id, object] : CUR_SCENE->_players)
    {
        auto unit = object ? object->GetScript<BasePlayerController>() : nullptr;

        if (!unit || !unit->_playerInfo || unit->_playerInfo->hp() <= 0 || FogOfWar::Get()->IsHidden(id))
            continue;
        const auto& info = *unit->_playerInfo;
        const MapUnitInfo* kind = FindMapUnit(info.champtype());
        const Vec3 p = object->GetTransform()->GetPosition();

        if (!kind)
            continue;

        if (kind->bar == MapUnitBar::Structure)
        {
            dot(p, 4.5f * scale, Vec4(0.f, 0.f, 0.f, 0.9f), base + 2);

            dot(p, 3.5f * scale, teamColor(info.teamid()), base + 3);
        }
        else if (kind->bar == MapUnitBar::Minion)
            dot(p, (info.champtype() == Protocol::PLAYER_TYPE_WARD ? 2.5f : 1.8f) * scale,
                info.champtype() == Protocol::PLAYER_TYPE_WARD ? Vec4(1.f, 0.9f, 0.3f, 1.f) : teamColor(info.teamid()),
                base + 2);
        else
        {
            const bool epic = kind->radius >= 1.4f;

            dot(p, (epic ? 5.f : 3.f) * scale, Vec4(0.f, 0.f, 0.f, 0.9f), base + 2);

            dot(p, (epic ? 4.f : 2.2f) * scale, epic ? Vec4(0.75f, 0.35f, 1.f, 1.f) : teamColor(0), base + 3);
        }
    }

    for (const auto& [id, object] : CUR_SCENE->_players)
    {
        auto unit = object ? object->GetScript<BasePlayerController>() : nullptr;

        if (!unit || !unit->_playerInfo || IsMapUnit(unit->_playerInfo->champtype()) || unit->_playerInfo->hp() <= 0 ||
            unit->_playerInfo->champtype() == Protocol::PLAYER_TYPE_TIBBERS || FogOfWar::Get()->IsHidden(id))
            continue;
        const auto& info = *unit->_playerInfo;
        const Vec2 c = WorldToMinimap(object->GetTransform()->GetPosition());
        const float size = 11.f * scale, ring = 1.6f * scale;
        const bool mine = unit == GAMEMANAGER->_myPlayer;
        AddSolid(Vec4(c.x - size - ring, c.y - size - ring, c.x + size + ring, c.y + size + ring), base + 4,
                 mine ? Vec4(1.f, 0.85f, 0.35f, 1.f) : teamColor(info.teamid()));
        const wstring name = KitIcon(info.champtype());
        _sprites.push_back({base + 5,
                            RESOURCES->Load<Texture>(L"HudIcon_" + name + L"_circle",
                                                     TextureRoot + L"Champions\\" + name + L"_circle.dds"),
                            Vec4(c.x - size, c.y - size, c.x + size, c.y + size), Vec2(0.f, 0.f), Vec2(1.f, 1.f),
                            Vec4(1.f)});
    }

    // Me on top again (the lists are unordered).
    if (auto camera = CameraController::Find())
    {
        // The camera's view: the ground under the screen's corners.
        auto cameraObject = CUR_SCENE->GetMainCamera();
        const Matrix inverse =
            (cameraObject->GetCamera()->GetViewMatrix() * cameraObject->GetCamera()->GetProjectionMatrix()).Invert();
        const Vec3 eye = cameraObject->GetTransform()->GetPosition();
        Vec2 corners[4];
        const Vec2 ndc[4] = {Vec2(-1.f, 1.f), Vec2(1.f, 1.f), Vec2(1.f, -1.f), Vec2(-1.f, -1.f)};

        for (int32 i = 0; i < 4; ++i)
        {
            const Vec3 farPoint = Vec3::Transform(Vec3(ndc[i].x, ndc[i].y, 1.f), inverse);
            const Vec3 dir = farPoint - eye;
            const float t = dir.y < -0.0001f ? (2.f - eye.y) / dir.y : 1000.f;
            corners[i] = WorldToMinimap(eye + dir * t);
        }

        const float left = max(r.x, min(corners[0].x, corners[3].x)), right = min(r.z, max(corners[1].x, corners[2].x));
        const float top = max(r.y, min(corners[0].y, corners[1].y)), bottom = min(r.w, max(corners[2].y, corners[3].y));
        const float t = max(1.f, 1.2f * scale);
        const Vec4 white(1.f, 1.f, 1.f, 0.85f);

        if (right > left && bottom > top)
        {
            AddSolid(Vec4(left, top, right, top + t), base + 6, white);
            AddSolid(Vec4(left, bottom - t, right, bottom), base + 6, white);
            AddSolid(Vec4(left, top, left + t, bottom), base + 6, white);
            AddSolid(Vec4(right - t, top, right, bottom), base + 6, white);
        }

        Add(camera->IsLocked() ? "CameraLockOn_ButtonDefault" : "CameraLockOff_ButtonDefault");
    }

    const POINT mouse = INPUT->GetMousePos();
    const Vec2 cursor(static_cast<float>(mouse.x), static_cast<float>(mouse.y));
    bool over = false;

    for (const char* region : {"MinimapRegion", "HUDCenterFrame", "InventoryFrame", "PortraitFrame_Default"})
        if (const Element* e = Get(region))
        {
            const Vec4 a = ScreenRect(*e);
            over |= cursor.x >= a.x && cursor.x <= a.z && cursor.y >= a.y && cursor.y <= a.w;
        }
    s_mouseOverHud = over;

    if (ImGui::GetIO().WantCaptureMouse)
        return;

    if (const Element* lock = Get("CameraLockButton_HitRegion"))
    {
        const Vec4 a = ScreenRect(*lock);

        if (INPUT->GetButtonDown(KEY_TYPE::LBUTTON) && cursor.x >= a.x && cursor.x <= a.z && cursor.y >= a.y &&
            cursor.y <= a.w)
            if (auto camera = CameraController::Find())
                camera->ToggleLocked();
    }

    Vec3 world;

    if (MinimapToWorld(cursor, world))
    {
        const bool pingKey = Keybinds::Held(Keybinds::Action::Ping) || INPUT->GetButton(KEY_TYPE::ALT),
                   retreatKey = Keybinds::Held(Keybinds::Action::PingRetreat);

        if ((pingKey || retreatKey) && INPUT->GetButtonDown(KEY_TYPE::LBUTTON))
            Pings::Send(retreatKey ? Protocol::PING_RETREAT : Protocol::PING_GENERIC, world, 0);
        else if (!pingKey && !retreatKey && INPUT->GetButton(KEY_TYPE::LBUTTON))
            if (auto camera = CameraController::Find())
                camera->Peek(world);

        if (INPUT->GetButtonDown(KEY_TYPE::RBUTTON))
            me.IssueMoveCommand(world, false);
    }
}

void HUDController::BuildScoreboard(PlayerController& me)
{
    _scoreboardShown = _scoreboardOverride || INPUT->GetButton(KEY_TYPE::TAB);

    if (!_scoreboardShown)
        return;
    const auto& desc = GAME->GetGameDesc();
    const float scale = desc.height / 1080.f;
    const float width = 980.f * scale, rowHeight = 46.f * scale, size = 17.f * scale;
    const auto champions = Champions();
    int32 rows = 1;

    for (int32 team = 1; team <= 2; ++team)
        rows = max(rows,
                   static_cast<int32>(std::count_if(champions.begin(), champions.end(),
                                                    [&](const auto& c) { return c->_playerInfo->teamid() == team; })));
    const float height = 70.f * scale + rows * rowHeight * 2.f + 60.f * scale;
    const float left = (desc.width - width) * 0.5f, top = desc.height * 0.16f;
    AddSolid(Vec4(left - 2.f, top - 2.f, left + width + 2.f, top + height + 2.f), 1200, Vec4(0.47f, 0.36f, 0.18f, 1.f));
    AddSolid(Vec4(left, top, left + width, top + height), 1201, Vec4(0.004f, 0.04f, 0.07f, 0.95f));
    auto text = HudText::Get();
    text->Draw(UiStrings::Get("score.title"), Vec2(left + width * 0.5f, top + 26.f * scale), 24.f * scale,
               HudText::Color::Gold);
    text->Draw(Clock(UnitStatus::GameTime()), Vec2(left + width - 20.f * scale, top + 26.f * scale), size,
               HudText::Color::Gray, HudText::Align::Right);

    // Columns: portrait, name / champion, level, K/D/A, CS, D F.
    const float colName = left + 74.f * scale, colLevel = left + width * 0.52f, colKda = left + width * 0.64f,
                colCs = left + width * 0.77f, colSpells = left + width * 0.86f;
    float y = top + 64.f * scale;

    for (const char* key : {"score.level", "score.kda", "score.cs", "score.spells"})
    {
        const float x = string(key) == "score.level" ? colLevel
                        : string(key) == "score.kda" ? colKda
                        : string(key) == "score.cs"  ? colCs
                                                     : colSpells + 22.f * scale;
        text->Draw(UiStrings::Get(key), Vec2(x, y), size * 0.85f, HudText::Color::Gray);
    }

    y += 18.f * scale;

    for (int32 team = 1; team <= 2; ++team)
    {
        text->Draw(UiStrings::Get(team == 1 ? "score.blue" : "score.red"), Vec2(left + 20.f * scale, y + 12.f * scale),
                   size, team == 1 ? HudText::Color::Mana : HudText::Color::Red, HudText::Align::Left);
        y += 26.f * scale;

        for (const auto& champion : champions)
        {
            const auto& info = *champion->_playerInfo;

            if (info.teamid() != team)
                continue;
            const UnitStatus::State* s = UnitStatus::Find(info.objectid());
            const float cy = y + rowHeight * 0.5f;

            if (champion == GAMEMANAGER->_myPlayer)
                AddSolid(Vec4(left + 8.f * scale, y + 2.f, left + width - 8.f * scale, y + rowHeight - 2.f), 1202,
                         Vec4(0.47f, 0.36f, 0.18f, 0.18f));
            const wstring name = KitIcon(info.champtype());
            const float icon = rowHeight - 8.f * scale;
            auto portrait = RESOURCES->Load<Texture>(L"HudIcon_" + name + L"_circle",
                                                     TextureRoot + L"Champions\\" + name + L"_circle.dds");
            _sprites.push_back(
                {1203, portrait,
                 Vec4(left + 20.f * scale, cy - icon * 0.5f, left + 20.f * scale + icon, cy + icon * 0.5f),
                 Vec2(0.f, 0.f), Vec2(1.f, 1.f), Vec4(1.f)});
            text->Draw(info.name(), Vec2(colName, cy), size, HudText::Color::White, HudText::Align::Left);
            text->Draw(std::to_string(
                           UnitStatus::Find(info.objectid()) ? max(1, UnitStatus::Find(info.objectid())->level) : 1),
                       Vec2(colLevel, cy), size);
            char kda[32];
            snprintf(kda, sizeof(kda), "%d / %d / %d", s ? s->kills : 0, s ? s->deaths : 0, s ? s->assists : 0);
            text->Draw(kda, Vec2(colKda, cy), size);
            text->Draw(std::to_string(s ? s->cs : 0), Vec2(colCs, cy), size);

            for (int32 slot = 0; slot < 2 && slot < info.summonerspells_size(); ++slot)
            {
                const wstring file =
                    SummonerSpellsClient::IconFile(static_cast<Protocol::SUMMONER_SPELL>(info.summonerspells(slot)));

                if (file.empty())
                    continue;
                const float spellSize = icon * 0.75f, x = colSpells + slot * (spellSize + 6.f * scale);
                _sprites.push_back({1203, RESOURCES->Load<Texture>(L"HudSummoner_" + file, file),
                                    Vec4(x, cy - spellSize * 0.5f, x + spellSize, cy + spellSize * 0.5f),
                                    Vec2(0.f, 0.f), Vec2(1.f, 1.f), Vec4(1.f)});
            }

            if (s)
            {
                const float itemSize = icon * 0.55f;

                for (int32 slot = 0; slot < 6; ++slot)
                    if (s->items[slot] != 0)
                        if (auto itemIcon = Items::Icon(s->items[slot]))
                        {
                            const float x = colKda + width * 0.07f + slot * (itemSize + 3.f * scale);
                            _sprites.push_back({1203, itemIcon,
                                                Vec4(x, cy - itemSize * 0.5f, x + itemSize, cy + itemSize * 0.5f),
                                                Vec2(0.f, 0.f), Vec2(1.f, 1.f), Vec4(1.f)});
                        }
            }

            y += rowHeight;
        }

        y += 8.f * scale;
    }
}

void HUDController::BuildTooltip(PlayerController& me)
{
    _tooltipTitle.clear();
    _tooltipLines = 0;
    const auto& info = *me._playerInfo;
    const ChampionKit* kit = FindKit(info.champtype());
    const string champion = kit ? kit->name : "Garen";
    static const char* const AbilitySlots[4] = {"Q", "W", "E", "R"};
    static const char* const SpellNames[] = {"", "Flash", "Smite", "Ignite", "Heal", "Ghost"};

    // The icon under the mouse: passive, Q-R, D-F.
    const POINT mouse = INPUT->GetMousePos();
    string element, champ = champion, slot;
    int32 rank = 1;
    auto hovered = [&](const string& name)
    {
        if (!_tooltipOverride.empty())
            return _tooltipOverride == name;
        const Element* e = Get(name);

        if (!e)
            return false;
        const Vec4 r = ScreenRect(*e);

        return mouse.x >= r.x && mouse.x <= r.z && mouse.y >= r.y && mouse.y <= r.w;
    };

    if (hovered("Passive_IconLoc"))
        element = "Passive_IconLoc", slot = "P";

    for (int32 i = 0; i < 4 && element.empty(); ++i)
        if (hovered("Ability" + std::to_string(i) + "_IconLoc"))
        {
            element = "Ability" + std::to_string(i) + "_IconLoc";
            slot = AbilitySlots[i];
            rank = UnitStatus::MyProgress().ranks[i];
        }
    for (int32 i = 0; i < 2 && element.empty(); ++i)
        if (hovered("Summoner" + std::to_string(i) + "_IconLoc") && info.summonerspells_size() > i)
        {
            const int32 spell = info.summonerspells(i);

            if (spell > 0 && spell < 6)
            {
                element = "Summoner" + std::to_string(i) + "_IconLoc";
                champ = "Summoners";
                slot = SpellNames[spell];
            }
        }
    const TooltipText::Entry* entry = element.empty() ? nullptr : TooltipText::Find(champ, slot);
    const Element* icon = element.empty() ? nullptr : Get(element);
    _tooltipDebug = "override=" + _tooltipOverride + " element=" + element + " champ=" + champ + " slot=" + slot +
                    " entry=" + (entry ? "y" : "n") + " icon=" + (icon ? "y" : "n");

    if (!entry || !icon || entry->ranks.empty())
        return;

    const auto& desc = GAME->GetGameDesc();
    const float scale = desc.height / 1080.f;
    const float body = 17.f * scale, title = 21.f * scale, lineHeight = body * 1.35f;
    const float width = 440.f * scale, padding = 14.f * scale;
    const int32 index = std::clamp(rank, 1, static_cast<int32>(entry->ranks.size())) - 1;

    string facts;
    auto add = [&](const string& text)
    {
        facts += (facts.empty() ? "" : "   ") + text;
    };
    auto number = [](float v)
    {
        char buffer[16];
        snprintf(buffer, sizeof(buffer), fabsf(v - roundf(v)) < 0.01f ? "%.0f" : "%.2g", v);

        return string(buffer);
    };

    if (slot == "P")
        add(UiStrings::Get("tooltip.passive"));
    else if (champ != "Summoners")
        add(UiStrings::Format("tooltip.rank", {std::to_string(index + 1)}));

    if (!entry->cooldown.empty() && entry->cooldown[min<size_t>(index, entry->cooldown.size() - 1)] > 0.f)
        add(UiStrings::Format("tooltip.cooldown",
                              {number(entry->cooldown[min<size_t>(index, entry->cooldown.size() - 1)])}));

    if (!entry->cost.empty() && entry->cost[min<size_t>(index, entry->cost.size() - 1)] > 0.f)
        add(UiStrings::Format("tooltip.cost", {number(entry->cost[min<size_t>(index, entry->cost.size() - 1)])}));

    if (entry->range > 0.f && entry->range < 5000.f)
        add(UiStrings::Format("tooltip.range", {number(entry->range)}));

    const vector<TooltipText::Line> lines = TooltipText::Layout(entry->ranks[index], body, width - padding * 2.f);
    const float height =
        padding * 2.f + title * 1.3f + (facts.empty() ? 0.f : lineHeight) + body * 0.4f + lines.size() * lineHeight;
    const Vec4 iconRect = ScreenRect(*icon);
    float left = std::clamp((iconRect.x + iconRect.z) * 0.5f - width * 0.5f, 8.f, desc.width - width - 8.f);
    float top = max(8.f, iconRect.y - 10.f * scale - height);
    const float border = max(1.f, 1.5f * scale);
    AddSolid(Vec4(left - border, top - border, left + width + border, top + height + border), 1100,
             Vec4(0.47f, 0.36f, 0.18f, 1.f));
    AddSolid(Vec4(left, top, left + width, top + height), 1101, Vec4(0.004f, 0.04f, 0.07f, 0.96f));

    auto text = HudText::Get();
    float y = top + padding + title * 0.6f;
    text->Draw(entry->name, Vec2(left + padding, y), title, HudText::Color::Gold, HudText::Align::Left);
    y += title * 0.9f;

    if (!facts.empty())
    {
        y += lineHeight * 0.5f;
        text->Draw(facts, Vec2(left + padding, y), body * 0.92f, HudText::Color::Gray, HudText::Align::Left);
        y += lineHeight * 0.5f;
    }

    y += body * 0.4f + lineHeight * 0.5f;

    for (const TooltipText::Line& line : lines)
    {
        float x = left + padding;

        for (const TooltipText::Run& run : line)
        {
            text->Draw(run.text, Vec2(x, y), body, run.color, HudText::Align::Left);
            x += text->Measure(run.text, body);
        }

        y += lineHeight;
    }

    _tooltipTitle = entry->name;
    _tooltipLines = lines.size();
}

void HUDController::BuildAbility(PlayerController& me, int32 slot)
{
    const string prefix = "Ability" + std::to_string(slot) + "_";
    const int32 skillId = slot + 1;
    const auto& info = *me._playerInfo;
    const float remaining = me.GetCooldownRemaining(skillId);
    const float duration = me.GetCooldownDuration(skillId);
    const float cost = me.GetManaCost(skillId);
    const bool outOfMana = cost > static_cast<float>(info.mp());
    const bool learned = UnitStatus::MyProgress().ranks[slot] > 0;
    const bool ready = learned && remaining <= 0.f && !outOfMana && info.hp() > 0;

    Add(prefix + "TimerBorderCooldownBG");
    AddIcon(prefix + "IconLoc", _spells[slot], learned ? Vec4(1.f) : Vec4(0.3f, 0.3f, 0.3f, 1.f));

    if (const UnitStatus::State* state = UnitStatus::Find(info.objectid()))
    {
        const float now = TIME->GetGameTime();
        auto timerBar = [&](float fraction)
        {
            Add(prefix + "TimerBar_Backdrop");
            AddCropped(prefix + "TimerBar_BarTexture", std::clamp(fraction, 0.f, 1.f));
        };

        if (info.champtype() == Protocol::PLAYER_TYPE_ASHE && slot == 0)
        {
            const bool active = state->asheQEndsAt > now;

            if (active)
                timerBar((state->asheQEndsAt - now) / 6.f);
            else
            {
                if (learned && state->asheFocus < 4)
                    Add(prefix + "OverlayUnavailable");

                if (state->asheFocus > 0)
                    Text(prefix + "Ammo", std::to_string(state->asheFocus), 1.f,
                         static_cast<int32>(HudText::Color::Gold));
            }
        }

        if (info.champtype() == Protocol::PLAYER_TYPE_GRAVES && slot == 2 && state->gravesStacks > 0)
            Text(prefix + "Ammo", std::to_string(state->gravesStacks), 1.f, static_cast<int32>(HudText::Color::Gold));

        if (info.champtype() == Protocol::PLAYER_TYPE_KATARINA && slot == 3 && state->katarinaREndsAt > now)
            timerBar((state->katarinaREndsAt - now) / 2.5f);

        if (info.champtype() == Protocol::PLAYER_TYPE_AHRI && slot == 3 && state->ahriDashes > 0)
        {
            Text(prefix + "Ammo", std::to_string(state->ahriDashes), 1.f, static_cast<int32>(HudText::Color::Gold));

            if (state->ahriDashWindowEndsAt > now)
                timerBar((state->ahriDashWindowEndsAt - now) / 15.f);
        }
    }

    if (remaining > 0.f)
    {
        if (const Element* icon = Get(prefix + "IconLoc"))
            AddSweep(ScreenRect(*icon), icon->layer, duration > 0.f ? remaining / duration : 1.f, CooldownShade);
        char buffer[16];

        if (remaining >= 1.f)
            snprintf(buffer, sizeof(buffer), "%d", (int32)std::ceil(remaining));
        else
            snprintf(buffer, sizeof(buffer), "%.1f", remaining);
        Text(prefix + "Cooldown", buffer, 1.6f);
    }
    else if (outOfMana)
    {
        Add(prefix + "IconOom");
    }

    Add(ready ? prefix + "BorderAvailable" : prefix + "BorderUnavailable");

    if (cost > 0.f)
    {
        Add(prefix + "CostBg");
        Text(prefix + "Cost", std::to_string((int32)cost), 0.9f, static_cast<int32>(HudText::Color::Mana));
    }

    Text(prefix + "HotKey", Hotkeys[slot], 1.f, static_cast<int32>(HudText::Color::White));

    const Element* row = Get(prefix + "PipRect");
    const Element* full = Get(prefix + "FullPip0");
    const Element* empty = Get(prefix + "EmptyPip0");

    if (row && full && empty)
    {
        const int32 maxRank = slot == 3 ? 3 : 5;
        const int32 rank = std::clamp(UnitStatus::MyProgress().ranks[slot], 0, maxRank);
        const Vec4 area = ScreenRect(*row);
        const Vec4 pip = ScreenRect(*full);
        const float width = pip.z - pip.x, height = pip.w - pip.y, gap = width * 0.25f;
        const float total = maxRank * width + (maxRank - 1) * gap;
        float x = (area.x + area.z - total) * 0.5f;
        const float y = (area.y + area.w - height) * 0.5f;

        for (int32 i = 0; i < maxRank; ++i, x += width + gap)
        {
            const Element& source = i < rank ? *full : *empty;
            _sprites.push_back({source.layer, source.texture, Vec4(x, y, x + width, y + height), source.uv0, source.uv1,
                                source.color});
        }
    }
}

void HUDController::Update()
{
    _sprites.clear();
    auto me = GAMEMANAGER->_myPlayer;

    if (!me || !me->_playerInfo || _elements.empty())
        return;
    const auto& info = *me->_playerInfo;

    if (_iconsFor != info.champtype())
        LoadChampionIcons(info.champtype());
    const UnitStatus::State* status = UnitStatus::Find(info.objectid());
    const UnitStatus::Progress& progress = UnitStatus::MyProgress();
    const float now = TIME->GetGameTime();
    const bool dead = status && status->respawnAt > now;

    // Frames and backers.
    Add("HUDCenterFrame_Backer");
    Add("HUDCenterFrame");
    Add("PortraitFrame_Default");
    Add("InventoryFrame");
    Add("InventoryFrame_Bookend_Right");

    for (int32 i = 0; i <= 5; ++i)
        Add("InventoryItem" + std::to_string(i) + "_Backdrop");
    Add("Shop_ButtonDefault");

    AddIcon("PlayerIcon_Base", _portrait, dead ? DeadPortrait : Vec4(1.f));
    Add("PlayerExp_Backdrop");

    if (progress.xpToNext > 0)
    {
        const float target = static_cast<float>(progress.xp) / progress.xpToNext;
        const float before = _xpShown;
        _xpShown = target < _xpShown ? target : _xpShown + (target - _xpShown) * min(1.f, TIME->GetDeltaTime() * 4.f);
        const bool filling = target - before > 0.002f;
        AddCropped("PlayerExp_BarTexture_Old", _xpShown, filling ? Vec4(1.4f, 1.3f, 1.6f, 1.f) : Vec4(1.f));
    }

    const int32 level = status ? status->level : max(1, info.level());

    const float levelAge = now - _levelUpAt;
    const float swell = levelAge < LevelUpSeconds
                            ? 1.f + 0.6f * std::sin(std::clamp(levelAge / LevelUpSeconds, 0.f, 1.f) * 3.14159f)
                            : 1.f;
    Text("PlayerLevel", std::to_string(level), 1.1f * swell,
         levelAge < LevelUpSeconds ? static_cast<int32>(HudText::Color::Gold) : 0);

    if (dead)
    {
        Text("PlayerRespawnTimer", std::to_string((int32)std::ceil(status->respawnAt - now)), 2.2f,
             static_cast<int32>(HudText::Color::Red));

        const auto& desc = GAME->GetGameDesc();
        const float uiScale = max(0.75f, desc.height / 1080.f);
        HudText::Get()->Draw(
            UiStrings::Format("death.respawn", {std::to_string((int32)std::ceil(status->respawnAt - now))}),
            Vec2(desc.width * 0.5f, desc.height * 0.16f), 36.f * uiScale, HudText::Color::White);
        HudText::Get()->Draw(UiStrings::Get("death.hint"),
                             Vec2(desc.width * 0.5f, desc.height * 0.16f + 40.f * uiScale), 17.f * uiScale,
                             HudText::Color::White, HudText::Align::Center, 0.8f);
    }

    Add("PlayerHP_Backdrop");
    Add("PlayerHP_StaticHealthEmpty");
    const float health = info.maxhp() > 0 ? static_cast<float>(info.hp()) / info.maxhp() : 0.f;
    AddCropped("PlayerHP_StaticHealth", health);
    Add("PlayerHP_BookendLeft");
    Add("PlayerHP_BookendRight");
    _lastHpText = std::to_string(info.hp()) + " / " + std::to_string(info.maxhp());
    Text("PlayerHP_ValueText", _lastHpText, 1.15f);
    Add("PlayerPar_Backdrop");
    Add("PlayerPar_StaticManaEmpty");

    if (info.maxmp() > 0)
    {
        AddCropped("PlayerPar_StaticMana", static_cast<float>(info.mp()) / info.maxmp());
        Text("PlayerPar_ValueText", std::to_string(info.mp()) + " / " + std::to_string(info.maxmp()), 1.15f);
    }

    Add("PlayerPar_BookendLeft");
    Add("PlayerPar_BookendRight");

    // Passive, spells and the empty summoner spell slots.
    Add("Passive_TimerBorderCooldownBG");
    AddIcon("Passive_IconLoc", _passive);

    if (status && info.champtype() == Protocol::PLAYER_TYPE_GRAVES)
    {
        // Shells left, or the reload's sweep.
        if (status->gravesShells == 0 && status->gravesReloadEndsAt > now)
        {
            if (const Element* icon = Get("Passive_IconLoc"))
                AddSweep(ScreenRect(*icon), icon->layer, (status->gravesReloadEndsAt - now) / 2.f, CooldownShade);
        }
        else
            Text("Passive_Ammo", std::to_string(status->gravesShells), 1.f, static_cast<int32>(HudText::Color::Gold));
    }

    if (status && info.champtype() == Protocol::PLAYER_TYPE_SONA && status->sonaChord > 0)
        Text("Passive_Ammo", std::to_string(status->sonaChord), 1.f,
             static_cast<int32>(status->sonaChord >= 3 ? HudText::Color::Mana : HudText::Color::Gold));
    Add("Passive_BorderAvailable");

    for (int32 slot = 0; slot < 4; ++slot)
        BuildAbility(*me, slot);
    BuildSkillPoints(*me);

    for (int32 slot = 0; slot < 2; ++slot)
        BuildSummoner(*me, slot);

    {
        static shared_ptr<Texture> wardIcon =
            RESOURCES->Load<Texture>(L"HudItem_3340", TextureRoot + L"Items\\3340.dds");

        const bool eye = status && status->heraldEyeEndsAt > now;
        static shared_ptr<Texture> eyeIcon =
            RESOURCES->Load<Texture>(L"HudItem_3513", L"..\\Resources\\Textures\\UI\\Items\\3513.dds");
        AddIcon("InventoryItem6_IconLoc", eye && eyeIcon ? eyeIcon : wardIcon);

        if (eye)
            Text("InventoryItem6_Cooldown",
                 std::to_string(static_cast<int32>(std::ceil(status->heraldEyeEndsAt - now))), 0.9f,
                 static_cast<int32>(HudText::Color::Gold));
        const auto& p = UnitStatus::MyProgress();

        if (p.trinketCharges <= 0 && p.trinketRechargeAt > now)
            if (const Element* icon = Get("InventoryItem6_IconLoc"))
            {
                AddSweep(ScreenRect(*icon), icon->layer,
                         p.trinketRechargeDuration > 0.f ? (p.trinketRechargeAt - now) / p.trinketRechargeDuration
                                                         : 1.f,
                         CooldownShade);
                Text("InventoryItem6_Cooldown",
                     std::to_string(static_cast<int32>(std::ceil(p.trinketRechargeAt - now))), 1.2f);
            }
        Text("InventoryItem6_Count", std::to_string(p.trinketCharges), 1.f, static_cast<int32>(HudText::Color::Gold));
        Text("InventoryItem6_HotKey", "4", 1.f, static_cast<int32>(HudText::Color::White));
    }

    Text("Shop_GoldText", std::to_string(progress.gold), 1.1f, static_cast<int32>(HudText::Color::Gold));
    BuildInventory(*me);

    if (status)
    {
        static const char* const Elements[4] = {"buff.infernal", "buff.mountain", "buff.ocean", "buff.cloud"};
        string line;

        for (int32 i = 0; i < 4; ++i)
            if (status->dragons[i] > 0)
                line += string(line.empty() ? "" : "   ") +
                        UiStrings::Format(Elements[i], {std::to_string(status->dragons[i])});
        const auto& desc = GAME->GetGameDesc();
        const Element* frame = Get("HUDCenterFrame");
        const Vec4 rect =
            frame ? ScreenRect(*frame) : Vec4(0.f, desc.height * 0.85f, static_cast<float>(desc.width), desc.height);
        const float size = desc.height * 0.022f;
        float y = rect.y - size * 0.9f;
        auto timedLine = [&](float endsAt, const char* key, HudText::Color color)
        {
            const int32 left = static_cast<int32>(std::ceil(endsAt - now));
            char clock[16];
            sprintf_s(clock, "%d:%02d", left / 60, left % 60);
            HudText::Get()->Draw(UiStrings::Format(key, {clock}), Vec2((rect.x + rect.z) * 0.5f, y), size, color);
            y -= size * 1.3f;
        };

        if (status->baronEndsAt > now)
            timedLine(status->baronEndsAt, "buff.baron", HudText::Color::Mana);

        if (status->elderEndsAt > now)
            timedLine(status->elderEndsAt, "buff.elder", HudText::Color::Gold);

        if (status->dragonSoul >= 0 && status->dragonSoul < 4)
        {
            static const char* const Souls[4] = {"buff.soul_infernal", "buff.soul_mountain", "buff.soul_ocean",
                                                 "buff.soul_cloud"};
            HudText::Get()->Draw(UiStrings::Get(Souls[status->dragonSoul]), Vec2((rect.x + rect.z) * 0.5f, y), size,
                                 HudText::Color::Gold);
            y -= size * 1.3f;
        }

        if (!line.empty())
            HudText::Get()->Draw(line, Vec2((rect.x + rect.z) * 0.5f, y), size, HudText::Color::Gold);
    }

    BuildUnitOverlays(*me);

    if (!_shopOpen)
        BuildTooltip(*me);
    BuildTopBar(*me);
    BuildMinimap(*me);
    BuildScoreboard(*me);
    BuildPings(*me);
    BuildChat(*me);
    BuildSurrender(*me);
    BuildRoomOverlay(*me);
    BuildDeathRecap(*me);
    BuildShop(*me);
    BuildOptions();
    BuildMatchSummary(*me);

    if (now - _levelUpAt < LevelUpSeconds)
        if (auto camera = CUR_SCENE ? CUR_SCENE->GetMainCamera() : nullptr)
        {
            const auto& desc = GAME->GetGameDesc();
            const float t = (now - _levelUpAt) / LevelUpSeconds;
            const Vec3 head = me->GetTransform()->GetPosition() + Vec3(0.f, 2.6f, 0.f);
            const Vec3 clip = Vec3::Transform(head, camera->GetCamera()->GetViewMatrix() *
                                                        camera->GetCamera()->GetProjectionMatrix());

            if (clip.z > 0.f && clip.z < 1.f)
            {
                const Vec2 screen((clip.x * 0.5f + 0.5f) * desc.width,
                                  (0.5f - clip.y * 0.5f) * desc.height - t * desc.height * 0.05f);
                HudText::Get()->Draw(Announcements::Text("level_up"), screen, desc.height * 0.03f, HudText::Color::Mana,
                                     HudText::Align::Center, std::clamp((1.f - t) * 3.f, 0.f, 1.f));
            }
        }

    if (now >= _announcementEndsAt && !_announcementQueue.empty())
    {
        _announcement = _announcementQueue.front();
        _announcementQueue.pop_front();
        _announcementEndsAt = now + AnnouncementSeconds;
    }

    if (now < _announcementEndsAt)
    {
        const auto& desc = GAME->GetGameDesc();
        const float alpha = std::clamp((_announcementEndsAt - now) / 0.5f, 0.f, 1.f);
        const float height = desc.height * 0.07f, center = desc.height * 0.2f;
        AddSolid(Vec4(desc.width * 0.22f, center - height * 0.5f, desc.width * 0.78f, center + height * 0.5f), 999,
                 Vec4(0.02f, 0.03f, 0.05f, 0.7f * alpha));
        HudText::Get()->Draw(_announcement.text, Vec2(desc.width * 0.5f, center), height * 0.42f,
                             _announcement.ally ? HudText::Color::Gold : HudText::Color::Red, HudText::Align::Center,
                             alpha);

        const float icon = height * 0.86f;

        for (int32 side = 0; side < 2; ++side)
        {
            const int32 type = side == 0 ? _announcement.sourceType : _announcement.targetType;

            if (type == 0)
                continue;
            const wstring name = KitIcon(type);
            auto portrait = RESOURCES->Load<Texture>(L"HudIcon_" + name + L"_circle",
                                                     TextureRoot + L"Champions\\" + name + L"_circle.dds");
            const float x = side == 0 ? desc.width * 0.22f + height * 0.1f : desc.width * 0.78f - height * 0.1f - icon;
            _sprites.push_back({1000, portrait, Vec4(x, center - icon * 0.5f, x + icon, center + icon * 0.5f),
                                Vec2(0.f, 0.f), Vec2(1.f, 1.f), Vec4(1.f, 1.f, 1.f, alpha)});
        }
    }

    if (now < _messageEndsAt)
    {
        const float alpha = std::clamp((_messageEndsAt - now) / 0.3f, 0.f, 1.f);

        if (const Element* tooltip = Get("StatusMessageToolTip"))
        {
            const Vec4 rect = ScreenRect(*tooltip);
            HudText::Get()->Draw(_message, Vec2((rect.x + rect.z) * 0.5f, (rect.y + rect.w) * 0.5f),
                                 (rect.w - rect.y) * 0.55f, HudText::Color::Red, HudText::Align::Center, alpha);
        }
    }

    if (_gameOver != 0 && !GameFlow::MatchSummary().shown)
    {
        const auto& desc = GAME->GetGameDesc();
        const float height = desc.height * 0.16f, center = desc.height * 0.38f;
        AddSolid(Vec4(0.f, center - height * 0.5f, static_cast<float>(desc.width), center + height * 0.5f), 1000,
                 Vec4(0.02f, 0.03f, 0.05f, 0.75f));
        HudText::Get()->Draw(UiStrings::Get(_gameOver > 0 ? "gameover.victory" : "gameover.defeat"),
                             Vec2(desc.width * 0.5f, center), height * 0.7f,
                             _gameOver > 0 ? HudText::Color::Gold : HudText::Color::Red);
    }

    std::stable_sort(_sprites.begin(), _sprites.end(),
                     [](const Sprite& a, const Sprite& b) { return a.layer < b.layer; });
}

void HUDController::Render()
{
    _lastSpriteCount = _sprites.size();

    if (_sprites.empty() || !_mesh)
        return;
    const auto& desc = GAME->GetGameDesc();
    auto ndc = [&](float x, float y)
    {
        return Vec3(x / desc.width * 2.f - 1.f, 1.f - y / desc.height * 2.f, 0.f);
    };
    auto setVertex = [](VertexTextureNormalTangentData& vertex, const Vec3& position, const Vec2& uv, const Vec4& color)
    {
        vertex.position = position;
        vertex.uv = uv;
        vertex.normal = Vec3(color.x, color.y, color.z);
        vertex.tangent = Vec3(color.w, 0.f, 0.f);
    };

    // Consecutive sprites with the same atlas share one draw.
    struct Run
    {
        shared_ptr<Texture> texture;
        uint32 first = 0, count = 0;
    };

    vector<Run> runs;
    shared_ptr<Texture> anyTexture;
    uint32 used = 0;

    for (const Sprite& sprite : _sprites)
    {
        if (used >= MaxSprites)
            break;
        VertexTextureNormalTangentData* quad = &_vertices[used * 4];

        if (sprite.slice)
        {
            const Vec2 noTexture(-1.f);
            setVertex(quad[0], ndc(sprite.rect.x, sprite.rect.y), noTexture, sprite.color);
            setVertex(quad[1], ndc(sprite.rect.z, sprite.rect.w), noTexture, sprite.color);
            setVertex(quad[2], ndc(sprite.uv1.x, sprite.uv1.y), noTexture, sprite.color);
            setVertex(quad[3], ndc(sprite.uv0.x, sprite.uv0.y), noTexture, sprite.color);
        }
        else
        {
            const Vec2 uv0 = sprite.texture ? sprite.uv0 : Vec2(-1.f), uv1 = sprite.texture ? sprite.uv1 : Vec2(-1.f);
            setVertex(quad[0], ndc(sprite.rect.x, sprite.rect.y), uv0, sprite.color);
            setVertex(quad[1], ndc(sprite.rect.z, sprite.rect.y), Vec2(uv1.x, uv0.y), sprite.color);
            setVertex(quad[2], ndc(sprite.rect.x, sprite.rect.w), Vec2(uv0.x, uv1.y), sprite.color);
            setVertex(quad[3], ndc(sprite.rect.z, sprite.rect.w), uv1, sprite.color);
        }

        if (sprite.texture && !anyTexture)
            anyTexture = sprite.texture;
        // Untextured sprites join whichever run they follow.
        if (runs.empty() || (sprite.texture && runs.back().texture && runs.back().texture != sprite.texture))
            runs.push_back({sprite.texture, used, 0});

        if (!runs.back().texture)
            runs.back().texture = sprite.texture;
        runs.back().count++;
        used++;
    }

    if (!anyTexture)
        return;

    _mesh->UpdateDynamicVertices(_vertices, used * 4);
    auto shader = _material->GetShader();
    _mesh->GetVertexBuffer()->PushData();
    _mesh->GetIndexBuffer()->PushData();
    DC->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    for (const Run& run : runs)
    {
        const shared_ptr<Texture>& texture = run.texture ? run.texture : anyTexture;
        _material->SetDiffuseMap(texture);
        _material->SetNormalMap(texture);
        _material->SetSpecularMap(texture);
        _material->Update();
        shader->DrawIndexed(0, 0, run.count * 6, run.first * 6, 0);
    }

    DC->OMSetBlendState(nullptr, nullptr, 0xffffffff);
    DC->OMSetDepthStencilState(nullptr, 0);
    DC->RSSetState(nullptr);
}
