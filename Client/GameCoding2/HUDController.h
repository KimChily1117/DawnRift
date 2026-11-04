// HUD 표시.
#pragma once
#include "MonoBehaviour.h"
#include "VertexData.h"

class PlayerController;

namespace Items
{
struct Item;
}

class Mesh;
class Material;
class Texture;

class HUDController : public MonoBehaviour
{
public:
    void Awake() override;
    void Update() override;
    void Render();

    void TriggerSkillCoolDown(int skillId) {}

    void ShowRejection(Protocol::SKILL_REJECT_REASON reason);

    void ShowGameOver(bool won)
    {
        _gameOver = won ? 1 : -1;
    }

    void ShowAnnouncement(const string& text, bool ally, int32 sourceType = 0, int32 targetType = 0);

    const string& GetAnnouncement() const
    {
        return _announcement.text;
    }

    void ShowLevelUp();

    void ToggleOptions();

    bool IsOptionsOpen() const
    {
        return _optionsOpen;
    }

    void ToggleShop();

    bool IsShopOpen() const
    {
        return _shopOpen;
    }

    void SetShopCategory(int32 category);

    void SelectShopItem(int32 itemId)
    {
        _shopSelected = itemId;
    }

    int32 GetShopSelected() const
    {
        return _shopSelected;
    }

    size_t GetShopListedCount() const;
    void Buy(int32 itemId);
    void Sell(int32 slot);
    void FlashItemSlot(int32 slot);

    int32 GetHoveredItem() const
    {
        return _hoverItem;
    }

    static void LoadAudioSettings();
    // DAWN_MUTE=1: silence for remote test runs (never saved).
    static bool MutedByEnvironment();
    static void ApplyMuteOverride();
    float GetLevelUpAge() const;

    size_t GetQueuedAnnouncements() const
    {
        return _announcementQueue.size();
    }

    // The newest banner, shown or still queued.
    const string& GetLatestAnnouncement() const
    {
        return _announcementQueue.empty() ? _announcement.text : _announcementQueue.back().text;
    }

    static bool IsMouseOverHud()
    {
        return s_mouseOverHud;
    }

    // Tests: show the Tab scoreboard without the key.
    void SetScoreboardOverride(bool shown)
    {
        _scoreboardOverride = shown;
    }

    bool IsScoreboardShown() const
    {
        return _scoreboardShown;
    }

    bool MinimapToWorld(const Vec2& pixel, Vec3& world) const;
    Vec2 WorldToMinimap(const Vec3& world) const;

    const string& GetTooltipTitle() const
    {
        return _tooltipTitle;
    }

    size_t GetTooltipLineCount() const
    {
        return _tooltipLines;
    }

    const string& GetTooltipDebug() const
    {
        return _tooltipDebug;
    }

    void SetTooltipOverride(const string& element)
    {
        _tooltipOverride = element;
    }

    int32 GetGameOver() const
    {
        return _gameOver;
    }

    bool GetElementRect(const string& name, Vec4& rect) const;

    const string& GetLastHpText() const
    {
        return _lastHpText;
    }

    size_t GetLastSpriteCount() const
    {
        return _lastSpriteCount;
    }

    static shared_ptr<HUDController> Find();

private:
    struct Element
    {
        Vec2 pos, size, ref = Vec2(1600.f, 1200.f), anchor;
        int32 layer = 0;
        shared_ptr<Texture> texture;
        Vec2 uv0, uv1;
        Vec4 color = Vec4(1.f);
        bool text = false;
    };

    struct Sprite
    {
        int32 layer = 0;
        shared_ptr<Texture> texture;
        Vec4 rect;
        Vec2 uv0, uv1;
        Vec4 color;
        bool slice = false;
    };

    void LoadLayout(const wstring& file);
    const Element* Get(const string& name) const;
    Vec4 ScreenRect(const Element& element) const;
    void Add(const string& name, const Vec4& color = Vec4(1.f));
    void AddCropped(const string& name, float fraction, const Vec4& color = Vec4(1.f));
    void AddIcon(const string& name, const shared_ptr<Texture>& icon, const Vec4& color = Vec4(1.f));
    void AddSweep(const Vec4& rect, int32 layer, float remaining, const Vec4& color);
    void AddSolid(const Vec4& rect, int32 layer, const Vec4& color);
    void Text(const string& name, const string& value, float scale = 1.f, int32 color = 0, int32 align = 1);
    void BuildAbility(PlayerController& me, int32 slot);

    void BuildSummoner(PlayerController& me, int32 slot);

    void BuildTooltip(PlayerController& me);

    void BuildTopBar(PlayerController& me);

    void BuildScoreboard(PlayerController& me);

    void BuildMinimap(PlayerController& me);

    void LoadChampionIcons(Protocol::PLAYER_CHAMPION_TYPE champion);

    unordered_map<string, Element> _elements;
    vector<Sprite> _sprites;
    size_t _lastSpriteCount = 0;
    Protocol::PLAYER_CHAMPION_TYPE _iconsFor = Protocol::PLAYER_TYPE_NONE;
    shared_ptr<Texture> _portrait, _passive, _spells[4];
    shared_ptr<Texture> _summonerIcons[2];

    shared_ptr<Mesh> _mesh;
    shared_ptr<Material> _material;
    vector<VertexTextureNormalTangentData> _vertices;

    static constexpr float MessageSeconds = 1.5f;
    string _message;
    float _messageEndsAt = 0.f;
    string _lastHpText;
    int32 _gameOver = 0;
    static constexpr float LevelUpSeconds = 1.6f;
    float _levelUpAt = -100.f;
    float _xpShown = 0.f;
    void BuildSkillPoints(PlayerController& me);
    void BuildOptions();

    void BuildInventory(PlayerController& me);
    void BuildItemTooltip(const Items::Item& item, const Vec4& anchor, int32 slot);
    void BuildShop(PlayerController& me);

    void BuildPings(PlayerController& me);
    // HUDChat.cpp: the chat log and the input line.
    void BuildChat(PlayerController& me);

    bool Project(const Vec3& world, Vec2& screen) const;
    void BuildUnitOverlays(PlayerController& me);
    void BuildSurrender(PlayerController& me);

    void BuildRoomOverlay(PlayerController& me);
    void BuildDeathRecap(PlayerController& me);
    void BuildMatchSummary(PlayerController& me);
    float OptionsExtraRows() const;
    void BuildOptionsExtras(float left, float y, float width, float rowHeight, float scale, const Vec2& cursor,
                            bool down);
    int32 _rebinding = -1;
    bool _shopOpen = false;
    int32 _shopCategory = 0, _shopSelected = 0, _shopPage = 0;
    size_t _shopListed = 0;
    int32 _hoverItem = 0, _hoverSlot = -1;
    Vec4 _hoverRect;
    float _itemFlashAt[6] = {-100.f, -100.f, -100.f, -100.f, -100.f, -100.f};
    static void SaveAudioSettings();
    bool _optionsOpen = false;
    int32 _optionsDrag = -1;
    static constexpr float AnnouncementSeconds = 3.f;

    struct Announcement
    {
        string text;
        bool ally = true;
        int32 sourceType = 0, targetType = 0;
    };

    Announcement _announcement;
    std::deque<Announcement> _announcementQueue;
    float _announcementEndsAt = 0.f;
    string _tooltipTitle, _tooltipOverride, _tooltipDebug;
    size_t _tooltipLines = 0;
    bool _scoreboardOverride = false, _scoreboardShown = false;
    shared_ptr<Texture> _minimap;
    Vec4 _minimapRect;
    static inline bool s_mouseOverHud = false;
};
