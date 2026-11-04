#pragma once

class Texture;

namespace Items
{
struct Item
{
    int32 id = 0;
    string name;
    int32 price = 0, priceTotal = 0, sell = 0;
    vector<int32> from, to;
    vector<string> categories;
    bool active = false, consumable = false;
    int32 maxStacks = 1;
    vector<pair<string, float>> stats;
    string text;

    bool Has(const char* category) const;

    bool IsBoots() const
    {
        return Has("Boots");
    }
};

enum class Category
{
    All = 0,
    Attack,
    Magic,
    Defense,
    Movement,
    Consumable,
    Recommended,
    Count
};

void SetChampionKey(const string& key);
vector<const Item*> Recommended();

bool Load();
bool IsLoaded();
const Item* Find(int32 id);
const vector<Item>& All();
// Items of a shop tab, in price order (Items.json's).
vector<const Item*> InCategory(Category category);
bool InCategory(const Item& item, Category category);
const char* CategoryKey(Category category);
shared_ptr<Texture> Icon(int32 id);

const char* HotKey(int32 slot);

string StatLabel(const string& key);
string StatValue(const string& key, float value);

int32 CostWith(const Item& item, const vector<int32>& owned);
}
