using System;
using System.Collections.Generic;
using System.IO;
using System.Text.Json;

namespace Server.Game.Items
{
    public sealed class ItemStats
    {
        public float Hp, Mana, Ad, Ap, Armor, Mr, Ms, MsPercent, AttackSpeed, Crit, CritDamage, Haste, Lethality,
            ArmorPen, MagicPen, MagicPenPercent, LifeSteal, Omnivamp, HealShieldPower, Tenacity, HpRegen, ManaRegen;

        public void Add(ItemStats other, float times = 1f)
        {
            if (other == null)
                return;
            Hp += other.Hp * times;
            Mana += other.Mana * times;
            Ad += other.Ad * times;
            Ap += other.Ap * times;
            Armor += other.Armor * times;
            Mr += other.Mr * times;
            Ms += other.Ms * times;
            MsPercent += other.MsPercent * times;
            AttackSpeed += other.AttackSpeed * times;
            Crit += other.Crit * times;
            CritDamage += other.CritDamage * times;
            Haste += other.Haste * times;
            Lethality += other.Lethality * times;
            MagicPen += other.MagicPen * times;
            LifeSteal += other.LifeSteal * times;
            Omnivamp += other.Omnivamp * times;
            HealShieldPower += other.HealShieldPower * times;
            HpRegen += other.HpRegen * times;
            ManaRegen += other.ManaRegen * times;

            ArmorPen = 1f - (1f - ArmorPen) * (1f - other.ArmorPen * times);
            MagicPenPercent = 1f - (1f - MagicPenPercent) * (1f - other.MagicPenPercent * times);
            Tenacity = 1f - (1f - Tenacity) * (1f - other.Tenacity * times);
        }

        public static ItemStats Parse(JsonElement stats)
        {
            var result = new ItemStats();

            foreach (JsonProperty p in stats.EnumerateObject())
            {
                float v = p.Value.GetSingle();

                switch (p.Name)
                {
                case "hp":
                    result.Hp = v;
                    break;
                case "mana":
                    result.Mana = v;
                    break;
                case "ad":
                    result.Ad = v;
                    break;
                case "ap":
                    result.Ap = v;
                    break;
                case "armor":
                    result.Armor = v;
                    break;
                case "mr":
                    result.Mr = v;
                    break;
                case "ms":
                    result.Ms = v;
                    break;
                case "msPercent":
                    result.MsPercent = v;
                    break;
                case "attackSpeed":
                    result.AttackSpeed = v;
                    break;
                case "crit":
                    result.Crit = v;
                    break;
                case "critDamage":
                    result.CritDamage = v;
                    break;
                case "haste":
                    result.Haste = v;
                    break;
                case "lethality":
                    result.Lethality = v;
                    break;
                case "armorPen":
                    result.ArmorPen = v;
                    break;
                case "magicPen":
                    result.MagicPen = v;
                    break;
                case "magicPenPercent":
                    result.MagicPenPercent = v;
                    break;
                case "lifeSteal":
                    result.LifeSteal = v;
                    break;
                case "omnivamp":
                    result.Omnivamp = v;
                    break;
                case "healShieldPower":
                    result.HealShieldPower = v;
                    break;
                case "tenacity":
                    result.Tenacity = v;
                    break;
                case "hpRegen":
                    result.HpRegen = v;
                    break;
                case "manaRegen":
                    result.ManaRegen = v;
                    break;
                }
            }

            return result;
        }
    }

    public sealed class ItemData
    {
        public int Id;
        public string Name;
        public int Price, PriceTotal, Sell;
        public int[] From = Array.Empty<int>(), To = Array.Empty<int>();
        public string[] Categories = Array.Empty<string>();
        public bool Active, Consumable;
        public int MaxStacks = 1;
        public ItemStats Stats = new ItemStats();
        public string Text = "";

        public bool Has(string category) => Array.IndexOf(Categories, category) >= 0;
        public bool IsBoots => Has("Boots");
        public bool IsFinal => To.Length == 0 && !Consumable;

        public bool Unique => IsFinal && PriceTotal >= 2000;
    }

    public static class ItemDatabase
    {
        private static readonly object Lock = new object();
        private static Dictionary<int, ItemData> _items;

        public static readonly HashSet<int> Excluded =
            new HashSet<int> { 1040, 1101, 1102, 1103, 1105, 1106, 1107, 3865, 4403, 2421 };

        public static void Load(string path)
        {
            var items = new Dictionary<int, ItemData>();
            using JsonDocument document = JsonDocument.Parse(File.ReadAllText(path));

            foreach (JsonElement e in document.RootElement.GetProperty("items").EnumerateArray())
            {
                var item = new ItemData {
                    Id = e.GetProperty("id").GetInt32(),
                    Name = e.GetProperty("name").GetString(),
                    Price = e.GetProperty("price").GetInt32(),
                    PriceTotal = e.GetProperty("priceTotal").GetInt32(),
                    Sell = e.GetProperty("sell").GetInt32(),
                    From = Ints(e, "from"),
                    To = Ints(e, "to"),
                    Active = e.GetProperty("active").GetBoolean(),
                    Consumable = e.GetProperty("consumable").GetBoolean(),
                    MaxStacks = Math.Max(1, e.GetProperty("maxStacks").GetInt32()),
                    Stats = ItemStats.Parse(e.GetProperty("stats")),
                    Text = e.TryGetProperty("text", out JsonElement text) ? text.GetString() : "",
                };
                var categories = new List<string>();

                foreach (JsonElement c in e.GetProperty("categories").EnumerateArray())
                    categories.Add(c.GetString());
                item.Categories = categories.ToArray();

                if (Excluded.Contains(item.Id))
                    continue;
                items[item.Id] = item;
            }

            // Recipes only point at items the shop sells.
            foreach (ItemData item in items.Values)
            {
                item.From = Array.FindAll(item.From, id => items.ContainsKey(id));
                item.To = Array.FindAll(item.To, id => items.ContainsKey(id));
            }

            lock (Lock) _items = items;
            Console.WriteLine("[Items] Loaded " + items.Count + " items from " + path);
        }

        private static int[] Ints(JsonElement e, string name)
        {
            if (!e.TryGetProperty(name, out JsonElement array))
                return Array.Empty<int>();
            var list = new List<int>();

            foreach (JsonElement v in array.EnumerateArray())
                list.Add(v.GetInt32());

            return list.ToArray();
        }

        private static Dictionary<int, ItemData> Items
        {
            get {
                lock (Lock)
                {
                    if (_items == null)
                        Load(Path.Combine(AppContext.BaseDirectory, "Content", "Items.json"));

                    return _items;
                }
            }
        }

        public static ItemData Get(int id) => Items.TryGetValue(id, out ItemData item) ? item : null;
        public static IEnumerable<ItemData> All => Items.Values;
        public static int Count => Items.Count;
    }
}
