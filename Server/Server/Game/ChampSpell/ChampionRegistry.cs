using Google.Protobuf.Protocol;
using Server.Game.Objects;
using Server.Game.Room;
using System;
using System.Collections.Generic;
using System.Linq;

public sealed class ChampionKit
{
    public PLAYER_CHAMPION_TYPE Type;
    public string Key;
    public Func<IChampionSkillHandler> Handler;

    public Func<int, double?> CastLockSeconds = skillId => 0.25;
    // Skills it keeps walking through (Garen E).
    public Func<int, bool> MovesWhileCasting = skillId => false;

    public Action<GameRoom, Player, GameObject> OnBasicAttackHit;

    public Func<Player, GameObject, float> BasicAttackDamage;
    public Func<Player, int> BasicAttackVariant;

    public Func<Player, bool> CanBasicAttack;

    public Action<GameRoom, Player, GameObject, float> FireBasicAttack;
    // The champion got the last hit on a unit (Ahri's passive).
    public Action<GameRoom, Player, GameObject> OnKill;

    public const double TakedownSeconds = 3.0;
    public Action<GameRoom, Player, Player> OnTakedown;

    public Func<Player, int, int, float?> Cooldown;

    public Action<GameRoom, Player, float> Update;
}

public static class ChampionRegistry
{
    private static readonly Dictionary<PLAYER_CHAMPION_TYPE, ChampionKit> Kits =
        new[] {
            new ChampionKit {
                Type = PLAYER_CHAMPION_TYPE.PlayerTypeGaren,
                Key = "Garen",
                Handler = () => new GarenSkillHandler(),

                CastLockSeconds = skillId => skillId == 2 ? (double?)null
                                                          : (skillId == 0   ? 48
                                                             : skillId == 1 ? 24
                                                             : skillId == 3 ? 72
                                                                            : 37) /
                                                                24.0,
                MovesWhileCasting = skillId => skillId == 3,
            },
            new ChampionKit {
                Type = PLAYER_CHAMPION_TYPE.PlayerTypeAnnie,
                Key = "Annie",
                Handler = () => new AnnieSkillHandler(),
                CastLockSeconds = skillId => (skillId == 0   ? 37
                                              : skillId == 3 ? 14
                                                             : 25) /
                                             24.0,
            },
            new ChampionKit {
                Type = PLAYER_CHAMPION_TYPE.PlayerTypeAshe,
                Key = "Ashe",
                Handler = () => new AsheSkillHandler(),
                CastLockSeconds = AsheSkillHandler.CastLockSeconds,
                OnBasicAttackHit = AsheSkillHandler.OnBasicAttackHit,
                BasicAttackDamage = AsheSkillHandler.BasicAttackDamage,
                BasicAttackVariant = player => player.Status.AsheQActive ? 1 : 0,
                Cooldown = AsheSkillHandler.Cooldown,
                Update = AsheSkillHandler.Update,
            },
            new ChampionKit {
                Type = PLAYER_CHAMPION_TYPE.PlayerTypeSona,
                Key = "Sona",
                Handler = () => new SonaSkillHandler(),
                CastLockSeconds = SonaSkillHandler.CastLockSeconds,
                OnBasicAttackHit = SonaSkillHandler.OnBasicAttackHit,
                BasicAttackVariant = SonaSkillHandler.BasicAttackVariant,
                Update = SonaSkillHandler.Update,
            },
            new ChampionKit {
                Type = PLAYER_CHAMPION_TYPE.PlayerTypeGraves,
                Key = "Graves",
                Handler = () => new GravesSkillHandler(),
                CastLockSeconds = GravesSkillHandler.CastLockSeconds,
                CanBasicAttack = player => !player.Status.GravesReloading && player.Status.GravesShells > 0,
                FireBasicAttack = GravesSkillHandler.FireShotgun,
                Update = GravesSkillHandler.Update,
            },
            new ChampionKit {
                Type = PLAYER_CHAMPION_TYPE.PlayerTypeKatarina,
                Key = "Katarina",
                Handler = () => new KatarinaSkillHandler(),
                CastLockSeconds = KatarinaSkillHandler.CastLockSeconds,
                CanBasicAttack = player => !player.Status.KatarinaChanneling,
                OnTakedown = KatarinaSkillHandler.OnTakedown,
                Update = KatarinaSkillHandler.Update,
            },
            new ChampionKit {
                Type = PLAYER_CHAMPION_TYPE.PlayerTypeAhri,
                Key = "Ahri",
                Handler = () => new AhriSkillHandler(),
                CastLockSeconds = AhriSkillHandler.CastLockSeconds,
                OnKill = AhriSkillHandler.OnKill,
                Cooldown = AhriSkillHandler.Cooldown,
                Update = AhriSkillHandler.Update,
            },
        }
            .ToDictionary(kit => kit.Type);

    public static ChampionKit Find(PLAYER_CHAMPION_TYPE type) => Kits.TryGetValue(type, out ChampionKit kit) ? kit
                                                                                                             : null;
    public static IEnumerable<ChampionKit> All => Kits.Values;

    public static int DefaultSkinFor(int team)
    {
        string[] picks =
            (Environment.GetEnvironmentVariable("DAWN_SKIN") ?? "").Split(',', StringSplitOptions.RemoveEmptyEntries);

        return picks.Length >= team && int.TryParse(picks[team - 1].Trim(), out int skin) ? Math.Max(0, skin) : 0;
    }

    public static PLAYER_CHAMPION_TYPE DefaultFor(int team)
    {
        string[] picks = (Environment.GetEnvironmentVariable("DAWN_CHAMPION") ?? "")
                             .Split(',', StringSplitOptions.RemoveEmptyEntries);
        string pick = picks.Length >= team ? picks[team - 1].Trim() : "";
        ChampionKit kit =
            Kits.Values.FirstOrDefault(k => string.Equals(k.Key, pick, StringComparison.OrdinalIgnoreCase));

        if (kit != null)
            return kit.Type;

        return team == 1 ? PLAYER_CHAMPION_TYPE.PlayerTypeGaren : PLAYER_CHAMPION_TYPE.PlayerTypeAnnie;
    }
}
