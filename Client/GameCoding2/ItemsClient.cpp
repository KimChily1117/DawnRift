#include "pch.h"
#include "ItemsClient.h"
#include "UiStrings.h"
#include "Texture.h"
#include <fstream>

namespace
{
const wstring ItemsFile = L"..\\Resources\\Data\\Items.json";
const wstring IconRoot = L"..\\Resources\\Textures\\UI\\Items\\";

vector<Items::Item>& Table()
{
    static vector<Items::Item> items;

    return items;
}

bool& Loaded()
{
    static bool loaded = false;

    return loaded;
}

string g_championKey = "Garen";

const char* const AttackCategories[] = {"Damage",           "AttackSpeed", "CriticalStrike", "LifeSteal",
                                        "ArmorPenetration", "OnHit",       "SpellVamp"};
const char* const MagicCategories[] = {"SpellDamage",       "Mana",         "ManaRegen",
                                       "CooldownReduction", "AbilityHaste", "MagicPenetration"};
const char* const DefenseCategories[] = {"Health", "Armor", "SpellBlock", "HealthRegen", "Tenacity", "Slow"};
const char* const MovementCategories[] = {"Boots", "NonbootsMovement"};
const char* const ConsumableCategories[] = {"Consumable", "Vision", "Trinket"};
}

namespace Items
{
bool Item::Has(const char* category) const
{
    return std::find(categories.begin(), categories.end(), category) != categories.end();
}

bool Load()
{
    if (Loaded())
        return !Table().empty();
    Loaded() = true;
    std::ifstream file(ItemsFile);
    json root = json::parse(file, nullptr, false);

    if (root.is_discarded() || !root.contains("items"))
    {
        DEBUG_LOG("[Items] missing or invalid Resources/Data/Items.json");

        return false;
    }

    static const int32 Excluded[] = {1040, 1101, 1102, 1103, 1105, 1106, 1107, 3865, 4403, 2421};

    for (const json& e : root["items"])
    {
        Item item;
        item.id = e.value("id", 0);

        if (std::find(std::begin(Excluded), std::end(Excluded), item.id) != std::end(Excluded))
            continue;
        item.name = e.value("name", "");
        item.price = e.value("price", 0);
        item.priceTotal = e.value("priceTotal", 0);
        item.sell = e.value("sell", 0);

        for (const json& f : e.value("from", json::array()))
            item.from.push_back(f.get<int32>());

        for (const json& t : e.value("to", json::array()))
            item.to.push_back(t.get<int32>());

        for (const json& c : e.value("categories", json::array()))
            item.categories.push_back(c.get<string>());
        item.active = e.value("active", false);
        item.consumable = e.value("consumable", false);
        item.maxStacks = max(1, e.value("maxStacks", 1));

        if (e.contains("stats"))
            for (auto it = e["stats"].begin(); it != e["stats"].end(); ++it)
                item.stats.emplace_back(it.key(), it.value().get<float>());
        item.text = e.value("text", "");
        Table().push_back(std::move(item));
    }

    // Recipes only name items the shop sells.
    for (Item& item : Table())
    {
        auto sold = [](int32 id)
        {
            return Find(id) == nullptr;
        };
        item.from.erase(std::remove_if(item.from.begin(), item.from.end(), sold), item.from.end());
        item.to.erase(std::remove_if(item.to.begin(), item.to.end(), sold), item.to.end());
    }

    return !Table().empty();
}

bool IsLoaded()
{
    return Load();
}

const Item* Find(int32 id)
{
    Load();

    for (const Item& item : Table())
        if (item.id == id)
            return &item;

    return nullptr;
}

const vector<Item>& All()
{
    Load();

    return Table();
}

bool InCategory(const Item& item, Category category)
{
    auto any = [&](const auto& list)
    {
        for (const char* c : list)
            if (item.Has(c))
                return true;

        return false;
    };

    switch (category)
    {
    case Category::All:
        return true;
    case Category::Attack:
        return any(AttackCategories);
    case Category::Magic:
        return any(MagicCategories);
    case Category::Defense:
        return any(DefenseCategories);
    case Category::Movement:
        return any(MovementCategories);
    case Category::Consumable:
        return any(ConsumableCategories);
    default:
        return false;
    }
}

vector<const Item*> InCategory(Category category)
{
    if (category == Category::Recommended)
        return Recommended();
    vector<const Item*> result;

    for (const Item& item : All())
        if (InCategory(item, category))
            result.push_back(&item);

    return result;
}

void SetChampionKey(const string& key)
{
    g_championKey = key;
}

vector<const Item*> Recommended()
{
    static json table;
    static bool loaded = false;

    if (!loaded)
    {
        loaded = true;
        std::ifstream file(L"..\\Resources\\Data\\RecommendedItems.json");
        table = json::parse(file, nullptr, false);
    }

    vector<const Item*> result;

    if (table.is_discarded() || !table.contains(g_championKey))
        return result;

    for (const char* group : {"starter", "boots", "core", "situational"})
        if (table[g_championKey].contains(group))
            for (const json& id : table[g_championKey][group])
                if (const Item* item = Find(id.get<int32>()))
                    result.push_back(item);

    return result;
}

const char* CategoryKey(Category category)
{
    switch (category)
    {
    case Category::Recommended:
        return "shop.recommended";
    case Category::Attack:
        return "shop.attack";
    case Category::Magic:
        return "shop.magic";
    case Category::Defense:
        return "shop.defense";
    case Category::Movement:
        return "shop.movement";
    case Category::Consumable:
        return "shop.consumable";
    default:
        return "shop.all";
    }
}

shared_ptr<Texture> Icon(int32 id)
{
    if (id <= 0)
        return nullptr;
    const wstring name = std::to_wstring(id);
    const wstring file = IconRoot + name + L".dds";

    if (!std::filesystem::exists(file))
        return nullptr;

    return RESOURCES->Load<Texture>(L"HudItem_" + name, file);
}

const char* HotKey(int32 slot)
{
    static const char* const Keys[6] = {"1", "2", "3", "5", "6", "7"};

    return slot >= 0 && slot < 6 ? Keys[slot] : "";
}

string StatLabel(const string& key)
{
    const string& label = UiStrings::Get("stat." + key);

    return label.empty() ? key : label;
}

string StatValue(const string& key, float value)
{
    static const char* const Percent[] = {"msPercent",       "attackSpeed",     "crit",      "critDamage",
                                          "armorPen",        "magicPenPercent", "lifeSteal", "omnivamp",
                                          "healShieldPower", "tenacity",        "hpRegen",   "manaRegen"};
    char buffer[32];

    if (std::find(std::begin(Percent), std::end(Percent), key) != std::end(Percent))
        snprintf(buffer, sizeof(buffer), "%d%%", static_cast<int32>(std::lround(value * 100.f)));
    else
        snprintf(buffer, sizeof(buffer), fabsf(value - roundf(value)) < 0.01f ? "%.0f" : "%.1f", value);

    return buffer;
}

namespace
{
int32 CostWalk(const Item& item, vector<int32>& owned, bool root)
{
    int32 cost = item.priceTotal;

    for (int32 componentId : item.from)
    {
        const Item* component = Find(componentId);

        if (!component)
            continue;
        auto held = std::find(owned.begin(), owned.end(), componentId);

        if (held != owned.end())
        {
            owned.erase(held);
            cost -= component->priceTotal;
        }
        else
            cost -= component->priceTotal - CostWalk(*component, owned, false);
    }

    return max(root ? item.price : 0, cost);
}
}

int32 CostWith(const Item& item, const vector<int32>& owned)
{
    vector<int32> left = owned;

    return CostWalk(item, left, true);
}
}
