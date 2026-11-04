#include "pch.h"
#include "HUDController.h"
#include "ItemsClient.h"
#include "ChampionKits.h"
#include "UiStrings.h"
#include "TooltipText.h"
#include "HudText.h"
#include "UnitStatus.h"
#include "PlayerController.h"
#include "ClientPacketHandler.h"
#include "Texture.h"

namespace
{
constexpr int32 ShopLayer = 1300;
const Vec4 Gold(0.78f, 0.67f, 0.43f, 1.f);
const Vec4 Panel(0.02f, 0.05f, 0.08f, 0.97f);
const Vec4 PanelLight(0.06f, 0.11f, 0.14f, 1.f);
const Vec4 Hover(0.2f, 0.3f, 0.32f, 1.f);
const Vec4 Selected(0.47f, 0.36f, 0.18f, 0.9f);
const Vec4 Dim(0.5f, 0.5f, 0.5f, 1.f);
const Vec4 Shade(0.f, 0.f, 0.f, 0.62f);

bool Inside(const Vec2& p, const Vec4& r)
{
    return p.x >= r.x && p.x <= r.z && p.y >= r.y && p.y <= r.w;
}

void SendItemAction(Protocol::ITEM_ACTION action, int32 itemId, int32 slot)
{
    Protocol::C_ItemAction packet;
    packet.set_action(action);
    packet.set_itemid(itemId);
    packet.set_slot(slot);
    NETWORK->SendPacket(ClientPacketHandler::MakeSendBuffer(packet, C_ITEM_ACTION));
}

vector<int32> OwnedIds()
{
    vector<int32> owned;

    for (const auto& slot : UnitStatus::MyProgress().items)
        if (slot.itemId != 0)
            owned.push_back(slot.itemId);

    return owned;
}

string Number(float v)
{
    char buffer[32];
    snprintf(buffer, sizeof(buffer), fabsf(v - roundf(v)) < 0.05f ? "%.0f" : "%.1f", v);

    return buffer;
}
}

void HUDController::ToggleShop()
{
    _shopOpen = !_shopOpen;
    SOUND->PlayCue(_shopOpen ? "Play_sfx_hud_Menu_Open" : "Play_sfx_hud_Menu_Close");

    if (_shopOpen && _shopSelected == 0)
    {
        if (auto me = GAMEMANAGER->_myPlayer; me && me->_playerInfo)
            if (const ChampionKit* kit = FindKit(me->_playerInfo->champtype()))
                Items::SetChampionKey(kit->name);
        _shopCategory = static_cast<int32>(Items::Category::Recommended);
        const auto recommended = Items::Recommended();
        _shopSelected = recommended.empty() ? 1055 : recommended.front()->id;
    }
}

void HUDController::SetShopCategory(int32 category)
{
    _shopCategory = std::clamp(category, 0, static_cast<int32>(Items::Category::Count));
    _shopPage = 0;
}

size_t HUDController::GetShopListedCount() const
{
    return _shopListed;
}

void HUDController::BuildInventory(PlayerController& me)
{
    const auto& progress = UnitStatus::MyProgress();
    const float now = TIME->GetGameTime();
    const POINT mouse = INPUT->GetMousePos();
    const Vec2 cursor(static_cast<float>(mouse.x), static_cast<float>(mouse.y));
    _hoverItem = 0;
    _hoverSlot = -1;

    for (int32 i = 0; i < 6; ++i)
    {
        const string prefix = "InventoryItem" + std::to_string(i) + "_";
        const auto& slot = progress.items[i];
        const Element* icon = Get(prefix + "IconLoc");

        if (!icon)
            continue;
        const Vec4 rect = ScreenRect(*icon);
        const bool hovered = !_shopOpen && Inside(cursor, rect);
        Text(prefix + "HotKey", Items::HotKey(i), 1.f, static_cast<int32>(HudText::Color::White));

        if (slot.itemId == 0)
            continue;
        const Items::Item* item = Items::Find(slot.itemId);
        AddIcon(prefix + "IconLoc", Items::Icon(slot.itemId),
                slot.cooldownAt > now && slot.cooldownDuration > 0.f ? Vec4(0.75f, 0.75f, 0.75f, 1.f) : Vec4(1.f));
        // A used or new item flashes for a moment.
        if (now - _itemFlashAt[i] < 0.35f)
            AddSolid(rect, icon->layer + 1, Vec4(1.f, 0.95f, 0.7f, 0.6f * (1.f - (now - _itemFlashAt[i]) / 0.35f)));

        if (slot.cooldownAt > now && slot.cooldownDuration > 0.f)
        {
            AddSweep(rect, icon->layer + 1, (slot.cooldownAt - now) / slot.cooldownDuration, Shade);
            Text(prefix + "Cooldown", std::to_string(static_cast<int32>(std::ceil(slot.cooldownAt - now))), 1.2f);
        }

        if (slot.stacks > 1)
            Text(prefix + "Count", std::to_string(slot.stacks), 1.f, static_cast<int32>(HudText::Color::Gold));
        else if (slot.charges >= 0)
            Text(prefix + "Count", std::to_string(slot.charges), 1.f,
                 static_cast<int32>(slot.charges > 0 ? HudText::Color::Gold : HudText::Color::Gray));

        if (hovered)
        {
            _hoverItem = slot.itemId;
            _hoverSlot = i;
            _hoverRect = rect;

            if (item && item->active && INPUT->GetButtonDown(KEY_TYPE::LBUTTON))
                me.UseItem(i, mouse.x, mouse.y);
        }
    }

    if (const Element* hit = Get("Shop_HitArea"))
    {
        const Vec4 rect = ScreenRect(*hit);

        if (!_shopOpen && Inside(cursor, rect))
        {
            Add("Shop_ButtonHover");

            if (INPUT->GetButtonDown(KEY_TYPE::LBUTTON))
                ToggleShop();
        }
    }

    if (_hoverItem != 0 && !_shopOpen)
        if (const Items::Item* item = Items::Find(_hoverItem))
            BuildItemTooltip(*item, _hoverRect, _hoverSlot);
}

void HUDController::BuildItemTooltip(const Items::Item& item, const Vec4& anchor, int32 slot)
{
    const auto& desc = GAME->GetGameDesc();
    const float scale = desc.height / 1080.f;
    const float body = 16.f * scale, title = 20.f * scale, lineHeight = body * 1.35f;
    const float width = 400.f * scale, padding = 14.f * scale;
    auto text = HudText::Get();

    string facts;
    const int32 cost = Items::CostWith(item, OwnedIds());

    if (slot >= 0)
        facts = UiStrings::Format("item.sell", {std::to_string(item.sell)});
    else
        facts = cost != item.priceTotal
                    ? UiStrings::Format("item.cost_discounted", {std::to_string(cost), std::to_string(item.priceTotal)})
                    : UiStrings::Format("item.cost", {std::to_string(item.priceTotal)});
    vector<string> statLines;

    for (const auto& [key, value] : item.stats)
        statLines.push_back(Items::StatLabel(key) + " " + Items::StatValue(key, value));
    // The description repeats the stats first; skip those lines.
    string description;
    {
        size_t start = 0;
        int32 skipped = 0;

        while (start < item.text.size())
        {
            size_t end = item.text.find('\n', start);

            if (end == string::npos)
                end = item.text.size();
            const string line = item.text.substr(start, end - start);
            start = end + 1;

            if (skipped < static_cast<int32>(statLines.size()))
            {
                ++skipped;
                continue;
            }

            description += (description.empty() ? "" : "<br>") + line;
        }
    }

    const vector<TooltipText::Line> lines = TooltipText::Layout(description, body, width - padding * 2.f);
    const float height = padding * 2.f + title * 1.3f + lineHeight + statLines.size() * lineHeight +
                         (lines.empty() ? 0.f : body * 0.4f + lines.size() * lineHeight);
    float left = std::clamp((anchor.x + anchor.z) * 0.5f - width * 0.5f, 8.f, desc.width - width - 8.f);
    float top = anchor.y - 10.f * scale - height;

    if (top < 8.f)
        top = min(desc.height - height - 8.f, anchor.w + 10.f * scale);
    const float border = max(1.f, 1.5f * scale);
    AddSolid(Vec4(left - border, top - border, left + width + border, top + height + border), 1500,
             Vec4(0.47f, 0.36f, 0.18f, 1.f));
    AddSolid(Vec4(left, top, left + width, top + height), 1501, Vec4(0.004f, 0.04f, 0.07f, 0.97f));
    float y = top + padding + title * 0.6f;
    text->Draw(item.name, Vec2(left + padding, y), title, HudText::Color::Gold, HudText::Align::Left);
    text->Draw(facts, Vec2(left + width - padding, y), body * 0.92f, HudText::Color::Gold, HudText::Align::Right);
    y += title * 0.9f + lineHeight * 0.5f;

    for (const string& line : statLines)
    {
        text->Draw(line, Vec2(left + padding, y), body, HudText::Color::White, HudText::Align::Left);
        y += lineHeight;
    }

    if (!lines.empty())
        y += body * 0.4f;

    for (const TooltipText::Line& line : lines)
    {
        float x = left + padding;

        for (const TooltipText::Run& run : line)
        {
            text->Draw(run.text, Vec2(x, y), body,
                       run.color == HudText::Color::White ? HudText::Color::Gray : run.color, HudText::Align::Left);
            x += text->Measure(run.text, body);
        }

        y += lineHeight;
    }

    _tooltipTitle = item.name;
    _tooltipLines = statLines.size() + lines.size();
}

void HUDController::BuildShop(PlayerController& me)
{
    _shopListed = 0;

    if (!_shopOpen)
        return;
    s_mouseOverHud = true;
    Items::Load();
    const auto& desc = GAME->GetGameDesc();
    const float scale = desc.height / 1080.f;
    const float width = 1180.f * scale, height = 720.f * scale;
    const float left = (desc.width - width) * 0.5f, top = (desc.height - height) * 0.5f - 40.f * scale;
    const float titleHeight = 54.f * scale;
    const auto& progress = UnitStatus::MyProgress();
    const POINT mouse = INPUT->GetMousePos();
    const Vec2 cursor(static_cast<float>(mouse.x), static_cast<float>(mouse.y));
    const bool click = INPUT->GetButtonDown(KEY_TYPE::LBUTTON), rightClick = INPUT->GetButtonDown(KEY_TYPE::RBUTTON);
    auto text = HudText::Get();
    const float body = 17.f * scale;

    AddSolid(Vec4(left - 2.f, top - 2.f, left + width + 2.f, top + height + 2.f), ShopLayer, Gold);
    AddSolid(Vec4(left, top, left + width, top + height), ShopLayer + 1, Panel);
    AddSolid(Vec4(left, top + titleHeight, left + width, top + titleHeight + 2.f * scale), ShopLayer + 2, Gold);
    text->Draw(UiStrings::Get("shop.title"), Vec2(left + 24.f * scale, top + titleHeight * 0.5f), 26.f * scale,
               HudText::Color::Gold, HudText::Align::Left);
    text->Draw(UiStrings::Format("shop.gold", {std::to_string(progress.gold)}),
               Vec2(left + width * 0.5f, top + titleHeight * 0.5f), 22.f * scale, HudText::Color::Gold);

    if (!progress.inShop)
        text->Draw(UiStrings::Get("shop.far"), Vec2(left + width - 80.f * scale, top + titleHeight * 0.5f), body,
                   HudText::Color::Red, HudText::Align::Right);
    // Close (X).
    {
        const Vec4 button(left + width - 46.f * scale, top + 10.f * scale, left + width - 12.f * scale,
                          top + 44.f * scale);
        const bool over = Inside(cursor, button);
        AddSolid(button, ShopLayer + 2, over ? Hover : PanelLight);
        text->Draw("X", Vec2((button.x + button.z) * 0.5f, (button.y + button.w) * 0.5f), 20.f * scale,
                   HudText::Color::Gold);

        if (click && over)
        {
            ToggleShop();

            return;
        }
    }

    // Tabs.
    const float tabsLeft = left + 12.f * scale, tabsWidth = 170.f * scale, tabHeight = 44.f * scale;
    float y = top + titleHeight + 16.f * scale;
    static const int32 TabOrder[] = {
        static_cast<int32>(Items::Category::Recommended), static_cast<int32>(Items::Category::All),
        static_cast<int32>(Items::Category::Attack),      static_cast<int32>(Items::Category::Magic),
        static_cast<int32>(Items::Category::Defense),     static_cast<int32>(Items::Category::Movement),
        static_cast<int32>(Items::Category::Consumable),  static_cast<int32>(Items::Category::Count),
    };

    for (int32 tab : TabOrder)
    {
        const Vec4 rect(tabsLeft, y, tabsLeft + tabsWidth, y + tabHeight - 6.f * scale);
        const bool over = Inside(cursor, rect), current = tab == _shopCategory;
        AddSolid(rect, ShopLayer + 2, current ? Selected : over ? Hover : PanelLight);
        const string label = tab == static_cast<int32>(Items::Category::Count)
                                 ? UiStrings::Get("shop.stats")
                                 : UiStrings::Get(Items::CategoryKey(static_cast<Items::Category>(tab)));
        text->Draw(label, Vec2(tabsLeft + 14.f * scale, (rect.y + rect.w) * 0.5f), body,
                   current ? HudText::Color::Gold : HudText::Color::White, HudText::Align::Left);

        if (click && over)
            SetShopCategory(tab);
        y += tabHeight;
    }

    const float gridLeft = left + 200.f * scale, gridTop = top + titleHeight + 16.f * scale;
    const float gridRight = left + width - 340.f * scale, gridBottom = top + height - 16.f * scale;

    if (_shopCategory == static_cast<int32>(Items::Category::Count))
    {
        // The champion's stats with items (S_UnitStats).
        const auto& s = progress.stats;

        struct Row
        {
            const char* key;
            string value;
        };

        const Row rows[] = {
            {"stat.ad", Number(s.attackDamage)},
            {"stat.ap", Number(s.abilityPower)},
            {"stat.armor", Number(s.armor)},
            {"stat.mr", Number(s.magicResist)},
            {"stat.attackSpeed", "+" + Number(s.attackSpeed * 100.f) + "%"},
            {"stat.ms", Number(s.moveSpeed)},
            {"stat.crit", Number(s.critChance * 100.f) + "%"},
            {"stat.haste", Number(s.abilityHaste)},
            {"stat.lethality", Number(s.lethality)},
            {"stat.armorPen", Number(s.armorPenPercent * 100.f) + "%"},
            {"stat.magicPen", Number(s.magicPen)},
            {"stat.magicPenPercent", Number(s.magicPenPercent * 100.f) + "%"},
            {"stat.lifeSteal", Number(s.lifeSteal * 100.f) + "%"},
            {"stat.omnivamp", Number(s.omnivamp * 100.f) + "%"},
            {"stat.healShieldPower", Number(s.healShieldPower * 100.f) + "%"},
            {"stat.tenacity", Number(s.tenacity * 100.f) + "%"},
            {"stat.hpRegen", Number(s.hpRegen)},
            {"stat.manaRegen", Number(s.manaRegen)},
        };
        const float rowHeight = 30.f * scale, column = (gridRight - gridLeft) * 0.5f;

        for (size_t i = 0; i < std::size(rows); ++i)
        {
            const float x = gridLeft + (i % 2) * column, cy = gridTop + 20.f * scale + (i / 2) * rowHeight;
            text->Draw(UiStrings::Get(rows[i].key), Vec2(x + 10.f * scale, cy), body, HudText::Color::Gray,
                       HudText::Align::Left);
            text->Draw(rows[i].value, Vec2(x + column - 20.f * scale, cy), body, HudText::Color::White,
                       HudText::Align::Right);
        }
    }
    else
    {
        // The tab's items in a grid; the wheel and the arrows page it.
        const vector<const Items::Item*> listed = Items::InCategory(static_cast<Items::Category>(_shopCategory));
        _shopListed = listed.size();
        const float cell = 92.f * scale, iconSize = 60.f * scale;
        const int32 columns = max(1, static_cast<int32>((gridRight - gridLeft) / cell));
        const int32 rows = max(1, static_cast<int32>((gridBottom - gridTop) / cell));
        const int32 pages = max(1, (static_cast<int32>(listed.size()) + columns * rows - 1) / (columns * rows));

        if (Inside(cursor, Vec4(gridLeft, gridTop, gridRight, gridBottom)))
        {
            const float wheel = INPUT->GetWheel();

            if (wheel < 0.f)
                _shopPage = min(pages - 1, _shopPage + 1);

            if (wheel > 0.f)
                _shopPage = max(0, _shopPage - 1);
        }

        _shopPage = std::clamp(_shopPage, 0, pages - 1);
        const vector<int32> owned = OwnedIds();

        for (int32 index = _shopPage * columns * rows;
             index < static_cast<int32>(listed.size()) && index < (_shopPage + 1) * columns * rows; ++index)
        {
            const Items::Item& item = *listed[index];
            const int32 local = index - _shopPage * columns * rows;
            const float x = gridLeft + (local % columns) * cell, cy = gridTop + (local / columns) * cell;
            const Vec4 rect(x + 4.f * scale, cy + 4.f * scale, x + cell - 4.f * scale, cy + cell - 4.f * scale);
            const Vec4 iconRect(x + (cell - iconSize) * 0.5f, cy + 6.f * scale, x + (cell + iconSize) * 0.5f,
                                cy + 6.f * scale + iconSize);
            const bool over = Inside(cursor, rect), selected = item.id == _shopSelected;
            const int32 cost = Items::CostWith(item, owned);
            const bool affordable = progress.gold >= cost;
            AddSolid(rect, ShopLayer + 2, selected ? Selected : over ? Hover : PanelLight);
            _sprites.push_back({ShopLayer + 3, Items::Icon(item.id), iconRect, Vec2(0.f, 0.f), Vec2(1.f, 1.f),
                                affordable ? Vec4(1.f) : Dim});
            text->Draw(std::to_string(cost), Vec2(x + cell * 0.5f, cy + cell - 14.f * scale), 15.f * scale,
                       affordable ? HudText::Color::Gold : HudText::Color::Gray);

            if (over)
            {
                if (click)
                    _shopSelected = item.id;

                if (rightClick)
                    Buy(item.id);
                _hoverItem = item.id;
                _hoverRect = iconRect;
            }
        }

        if (pages > 1)
        {
            const string page = std::to_string(_shopPage + 1) + " / " + std::to_string(pages);
            text->Draw(page, Vec2((gridLeft + gridRight) * 0.5f, gridBottom - 6.f * scale), 14.f * scale,
                       HudText::Color::Gray);
        }
    }

    // The selected item.
    const float panelLeft = left + width - 326.f * scale, panelRight = left + width - 12.f * scale;
    AddSolid(Vec4(panelLeft, gridTop, panelRight, gridBottom), ShopLayer + 2, PanelLight);

    if (const Items::Item* item = Items::Find(_shopSelected))
    {
        const float pad = 14.f * scale, iconSize = 64.f * scale;
        float py = gridTop + pad;
        _sprites.push_back({ShopLayer + 3, Items::Icon(item->id),
                            Vec4(panelLeft + pad, py, panelLeft + pad + iconSize, py + iconSize), Vec2(0.f, 0.f),
                            Vec2(1.f, 1.f), Vec4(1.f)});
        text->Draw(item->name, Vec2(panelLeft + pad + iconSize + 10.f * scale, py + 16.f * scale), 20.f * scale,
                   HudText::Color::Gold, HudText::Align::Left);
        const int32 cost = Items::CostWith(*item, OwnedIds());
        text->Draw(
            cost != item->priceTotal
                ? UiStrings::Format("item.cost_discounted", {std::to_string(cost), std::to_string(item->priceTotal)})
                : UiStrings::Format("item.cost", {std::to_string(item->priceTotal)}),
            Vec2(panelLeft + pad + iconSize + 10.f * scale, py + 42.f * scale), body * 0.9f, HudText::Color::Gold,
            HudText::Align::Left);
        py += iconSize + pad;

        for (const auto& [key, value] : item->stats)
        {
            text->Draw(Items::StatLabel(key) + " " + Items::StatValue(key, value), Vec2(panelLeft + pad, py),
                       body * 0.95f, HudText::Color::White, HudText::Align::Left);
            py += body * 1.3f;
        }

        // Description without the repeated stat lines, wrapped.
        {
            string description;
            size_t start = 0;
            int32 skipped = 0;

            while (start < item->text.size())
            {
                size_t end = item->text.find('\n', start);

                if (end == string::npos)
                    end = item->text.size();
                const string line = item->text.substr(start, end - start);
                start = end + 1;

                if (skipped < static_cast<int32>(item->stats.size()))
                {
                    ++skipped;
                    continue;
                }

                description += (description.empty() ? "" : "<br>") + line;
            }

            py += 6.f * scale;
            const float maxLines = (gridBottom - 150.f * scale - py) / (body * 1.25f);
            size_t shown = 0;

            for (const TooltipText::Line& line :
                 TooltipText::Layout(description, body * 0.9f, panelRight - panelLeft - pad * 2.f))
            {
                if (static_cast<float>(shown++) >= maxLines)
                    break;
                float x = panelLeft + pad;

                for (const TooltipText::Run& run : line)
                {
                    text->Draw(run.text, Vec2(x, py), body * 0.9f,
                               run.color == HudText::Color::White ? HudText::Color::Gray : run.color,
                               HudText::Align::Left);
                    x += text->Measure(run.text, body * 0.9f);
                }

                py += body * 1.25f;
            }
        }

        auto iconRow = [&](const vector<int32>& ids, const char* key, float rowY)
        {
            if (ids.empty())
                return;
            text->Draw(UiStrings::Get(key), Vec2(panelLeft + pad, rowY), body * 0.85f, HudText::Color::Gray,
                       HudText::Align::Left);
            const float thumb = 40.f * scale;
            float x = panelLeft + pad;

            for (int32 id : ids)
            {
                if (x + thumb > panelRight - pad)
                    break;
                const Vec4 r(x, rowY + 12.f * scale, x + thumb, rowY + 12.f * scale + thumb);
                const bool over = Inside(cursor, r);

                if (over)
                    AddSolid(Vec4(r.x - 2.f, r.y - 2.f, r.z + 2.f, r.w + 2.f), ShopLayer + 3, Gold);
                _sprites.push_back({ShopLayer + 4, Items::Icon(id), r, Vec2(0.f, 0.f), Vec2(1.f, 1.f), Vec4(1.f)});

                if (over && click)
                    _shopSelected = id;

                if (over && rightClick)
                    Buy(id);

                if (over)
                {
                    _hoverItem = id;
                    _hoverRect = r;
                }

                x += thumb + 6.f * scale;
            }
        };
        const float recipeTop = gridBottom - 150.f * scale;
        iconRow(item->from, "shop.recipe", recipeTop);
        iconRow(item->to, "shop.builds_into", recipeTop + 64.f * scale);
        // Buttons: buy, sell (when held), undo.
        const float by = gridBottom - 26.f * scale, bw = 92.f * scale, bh = 34.f * scale;

        struct Button
        {
            const char* key;
            bool enabled;
            int32 action;
        };

        int32 heldSlot = -1;

        for (int32 i = 0; i < 6; ++i)
            if (progress.items[i].itemId == item->id)
            {
                heldSlot = i;
                break;
            }
        const Button buttons[3] = {
            {"shop.buy", progress.inShop && progress.gold >= cost, 0},
            {"shop.sell", progress.inShop && heldSlot >= 0, 1},
            {"shop.undo", progress.inShop && progress.canUndo, 2},
        };

        for (int32 i = 0; i < 3; ++i)
        {
            const float bx = panelLeft + pad + i * (bw + 8.f * scale);
            const Vec4 r(bx, by - bh * 0.5f, bx + bw, by + bh * 0.5f);
            const bool over = Inside(cursor, r);
            AddSolid(r, ShopLayer + 3,
                     !buttons[i].enabled ? Vec4(0.08f, 0.1f, 0.11f, 1.f)
                     : over              ? Hover
                                         : Vec4(0.1f, 0.16f, 0.18f, 1.f));
            text->Draw(UiStrings::Get(buttons[i].key), Vec2((r.x + r.z) * 0.5f, by), body,
                       buttons[i].enabled ? HudText::Color::Gold : HudText::Color::Gray);

            if (click && over && buttons[i].enabled)
            {
                if (buttons[i].action == 0)
                    Buy(item->id);
                else if (buttons[i].action == 1)
                    Sell(heldSlot);
                else
                    SendItemAction(Protocol::ITEM_ACTION_UNDO, 0, 0);
            }
        }
    }

    for (int32 i = 0; i < 6; ++i)
    {
        const Element* icon = Get("InventoryItem" + std::to_string(i) + "_IconLoc");

        if (!icon || progress.items[i].itemId == 0)
            continue;
        const Vec4 rect = ScreenRect(*icon);

        if (Inside(cursor, rect))
        {
            _hoverItem = progress.items[i].itemId;
            _hoverRect = rect;
            _hoverSlot = i;

            if (rightClick)
                Sell(i);

            if (click)
                _shopSelected = progress.items[i].itemId;
        }
    }

    if (_hoverItem != 0)
        if (const Items::Item* hovered = Items::Find(_hoverItem))
            BuildItemTooltip(*hovered, _hoverRect, _hoverSlot);
}

void HUDController::Buy(int32 itemId)
{
    SendItemAction(Protocol::ITEM_ACTION_BUY, itemId, 0);
    SOUND->PlayCue("Play_sfx_hud_Generic_Button_Click");
}

void HUDController::Sell(int32 slot)
{
    if (slot < 0 || slot >= 6)
        return;
    SendItemAction(Protocol::ITEM_ACTION_SELL, 0, slot);
    SOUND->PlayCue("Play_sfx_hud_Generic_Button_Click");
}

void HUDController::FlashItemSlot(int32 slot)
{
    if (slot >= 0 && slot < 6)
        _itemFlashAt[slot] = TIME->GetGameTime();
}
