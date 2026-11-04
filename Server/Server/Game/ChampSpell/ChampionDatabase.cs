using System.Linq;
using Google.Protobuf.Protocol;
using System;
using System.Collections.Generic;
using System.IO;
using System.Text.Json;

public sealed class ChampionData
{
    public const float UnitScale = 0.01f;

    public string Name { get; private set; }
    public Stat Hp { get; private set; }
    public Stat AttackDamage { get; private set; }
    public Stat Armor { get; private set; }
    public Stat MagicResist { get; private set; }
    public Stat HpRegen { get; private set; }
    public Stat Mana { get; private set; }
    public Stat ManaRegen { get; private set; }
    public float MoveSpeed { get; private set; }
    public float AttackRange { get; private set; }
    public float AttackSpeed { get; private set; }
    public float AttackWindupPercent { get; private set; }
    public bool Ranged { get; private set; }
    public float BasicAttackMissileSpeed { get; private set; }
    public IReadOnlyDictionary<string, SpellData> Spells { get; private set; }

    public float MoveSpeedUnits => MoveSpeed * UnitScale;
    public float AttackRangeUnits => AttackRange * UnitScale;

    public readonly struct Stat
    {
        public readonly float Base;
        public readonly float PerLevel;

        public Stat(float baseValue, float perLevel)
        {
            Base = baseValue;
            PerLevel = perLevel;
        }

        public float AtLevel(int level)
        {
            int n = Math.Max(1, level) - 1;

            return Base + PerLevel * n * (0.7025f + 0.0175f * n);
        }
    }

    internal SpellData Spell(SkillType slot)
    {
        string key = slot switch { SkillType.QSpell => "Q", SkillType.WSpell => "W", SkillType.ESpell => "E",
                                   SkillType.RSpell => "R",
                                   _ => null };

        return key != null && Spells.TryGetValue(key, out SpellData spell) ? spell : SpellData.Empty;
    }

    internal static ChampionData Parse(JsonElement root)
    {
        JsonElement stats = root.GetProperty("stats");
        var spells = new Dictionary<string, SpellData>();

        if (root.TryGetProperty("spells", out JsonElement spellsElement))
            foreach (JsonProperty spell in spellsElement.EnumerateObject())
                spells[spell.Name] = SpellData.Parse(spell.Value);

        return new ChampionData {
            Name = root.GetProperty("name").GetString(),
            Hp = ReadStat(stats, "hp"),
            AttackDamage = ReadStat(stats, "attackDamage"),
            Armor = ReadStat(stats, "armor"),
            MagicResist = ReadStat(stats, "magicResist"),
            HpRegen = ReadStat(stats, "hpRegen"),
            Mana = ReadStat(stats, "mana"),
            ManaRegen = stats.TryGetProperty("mana", out JsonElement mana)
                            ? new Stat(ReadFloat(mana, "regen", 0f), ReadFloat(mana, "regenPerLevel", 0f))
                            : new Stat(0f, 0f),
            MoveSpeed = ReadFloat(stats, "moveSpeed", 335f),
            AttackRange = ReadFloat(stats, "attackRange", 125f),
            AttackSpeed = ReadFloat(stats, "attackSpeed", 0.625f),
            AttackWindupPercent = ReadFloat(stats, "attackWindupPercent", 0.3f),
            Ranged = stats.TryGetProperty("ranged", out JsonElement ranged) && ranged.GetBoolean(),
            BasicAttackMissileSpeed = ReadFloat(stats, "basicAttackMissileSpeed", 0f),
            Spells = spells,
        };
    }

    private static Stat ReadStat(JsonElement stats, string name)
    {
        if (!stats.TryGetProperty(name, out JsonElement stat))
            return new Stat(0f, 0f);

        return new Stat(ReadFloat(stat, "base", 0f), ReadFloat(stat, "perLevel", 0f));
    }

    internal static float ReadFloat(JsonElement element, string name, float fallback)
    {
        return element.TryGetProperty(name, out JsonElement value) && value.ValueKind == JsonValueKind.Number
                   ? value.GetSingle()
                   : fallback;
    }
}

public sealed class SpellData
{
    public static readonly SpellData Empty = new SpellData();

    public float[] Cooldown { get; private set; } = Array.Empty<float>();
    public float[] ManaCost { get; private set; } = Array.Empty<float>();
    public float CastRange { get; private set; }
    public float CastRadius { get; private set; }
    public float MissileSpeed { get; private set; }
    private Dictionary<string, float[]> _values = new Dictionary<string, float[]>();

    public float CastRangeUnits => CastRange * ChampionData.UnitScale;
    public float CastRadiusUnits => CastRadius * ChampionData.UnitScale;
    public float MissileSpeedUnits => MissileSpeed * ChampionData.UnitScale;

    public float CooldownAt(int rank, float fallback) => At(Cooldown, rank, fallback);
    public float ManaCostAt(int rank) => At(ManaCost, rank, 0f);
    public float Value(string name, int rank, float fallback) => _values.TryGetValue(name, out float[] values)
                                                                     ? At(values, rank, fallback)
                                                                     : fallback;

    private static float At(float[] values, int rank, float fallback)
    {
        if (values == null || values.Length == 0)
            return fallback;

        return values[Math.Clamp(rank, 1, values.Length) - 1];
    }

    internal static SpellData Parse(JsonElement spell)
    {
        var data = new SpellData {
            Cooldown = ReadArray(spell, "cooldown"),
            ManaCost = ReadArray(spell, "mana"),
            CastRange = ChampionData.ReadFloat(spell, "castRange", 0f),
            CastRadius = ChampionData.ReadFloat(spell, "castRadius", 0f),
            MissileSpeed = ChampionData.ReadFloat(spell, "missileSpeed", 0f),
        };

        if (spell.TryGetProperty("values", out JsonElement values))
            foreach (JsonProperty value in values.EnumerateObject())
                data._values[value.Name] = ReadArray(values, value.Name);

        return data;
    }

    private static float[] ReadArray(JsonElement element, string name)
    {
        if (!element.TryGetProperty(name, out JsonElement array) || array.ValueKind != JsonValueKind.Array)
            return Array.Empty<float>();
        var result = new List<float>();

        foreach (JsonElement item in array.EnumerateArray())
            if (item.ValueKind == JsonValueKind.Number)
                result.Add(item.GetSingle());

        return result.ToArray();
    }
}

public static class ChampionDatabase
{
    private static readonly object Lock = new object();
    private static Dictionary<PLAYER_CHAMPION_TYPE, ChampionData> _champions;

    private static readonly(PLAYER_CHAMPION_TYPE Type, string File)[] Files =
        ChampionRegistry.All.Select(kit => (kit.Type, kit.Key + ".json"))
            .Concat(new[] {
                (PLAYER_CHAMPION_TYPE.PlayerTypeTibbers, "AnnieTibbers.json"),

                (PLAYER_CHAMPION_TYPE.PlayerTypeMinionMelee, "SRU_OrderMinionMelee.json"),
                (PLAYER_CHAMPION_TYPE.PlayerTypeMinionRanged, "SRU_OrderMinionRanged.json"),
                (PLAYER_CHAMPION_TYPE.PlayerTypeMinionSiege, "SRU_OrderMinionSiege.json"),
                (PLAYER_CHAMPION_TYPE.PlayerTypeTurret, "Turret.json"),
                (PLAYER_CHAMPION_TYPE.PlayerTypeSruBlue, "SRU_Blue.json"),
                (PLAYER_CHAMPION_TYPE.PlayerTypeSruRed, "SRU_Red.json"),
                (PLAYER_CHAMPION_TYPE.PlayerTypeSruGromp, "SRU_Gromp.json"),
                (PLAYER_CHAMPION_TYPE.PlayerTypeSruMurkwolf, "SRU_Murkwolf.json"),
                (PLAYER_CHAMPION_TYPE.PlayerTypeSruRazorbeak, "SRU_Razorbeak.json"),
                (PLAYER_CHAMPION_TYPE.PlayerTypeSruKrug, "SRU_Krug.json"),
                (PLAYER_CHAMPION_TYPE.PlayerTypeSruDragonFire, "SRU_Dragon_Fire.json"),
                (PLAYER_CHAMPION_TYPE.PlayerTypeSruDragonEarth, "SRU_Dragon_Earth.json"),
                (PLAYER_CHAMPION_TYPE.PlayerTypeSruDragonWater, "SRU_Dragon_Water.json"),
                (PLAYER_CHAMPION_TYPE.PlayerTypeSruDragonAir, "SRU_Dragon_Air.json"),
                (PLAYER_CHAMPION_TYPE.PlayerTypeSruBaron, "SRU_Baron.json"),
                (PLAYER_CHAMPION_TYPE.PlayerTypeSruDragonElder, "SRU_Dragon_Elder.json"),
                (PLAYER_CHAMPION_TYPE.PlayerTypeSruRiftherald, "SRU_RiftHerald.json"),
                (PLAYER_CHAMPION_TYPE.PlayerTypeMinionSuper, "SRU_OrderMinionSuper.json"),
                (PLAYER_CHAMPION_TYPE.PlayerTypeSruKrugMini, "SRU_KrugMini.json"),
                (PLAYER_CHAMPION_TYPE.PlayerTypeSruKrugMinimini, "SRU_KrugMiniMini.json"),
                (PLAYER_CHAMPION_TYPE.PlayerTypeSruMurkwolfMini, "SRU_MurkwolfMini.json"),
                (PLAYER_CHAMPION_TYPE.PlayerTypeSruRazorbeakMini, "SRU_RazorbeakMini.json"),
            })
            .ToArray();

    public static void Load(string championsFolder)
    {
        var champions = new Dictionary<PLAYER_CHAMPION_TYPE, ChampionData>();

        foreach (var (type, file) in Files)
        {
            string path = Path.Combine(championsFolder, file);
            using JsonDocument document = JsonDocument.Parse(File.ReadAllText(path));
            champions[type] = ChampionData.Parse(document.RootElement);
        }

        lock (Lock) _champions = champions;
        Console.WriteLine("[Champions] Loaded " + champions.Count + " champions from " + championsFolder);
    }

    public static ChampionData Get(PLAYER_CHAMPION_TYPE type)
    {
        Dictionary<PLAYER_CHAMPION_TYPE, ChampionData> champions;
        lock (Lock)
        {
            if (_champions == null)
                Load(Path.Combine(AppContext.BaseDirectory, "Content", "Champions"));
            champions = _champions;
        }

        return champions.TryGetValue(type, out ChampionData data) ? data : null;
    }
}
