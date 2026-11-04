using Google.Protobuf.Protocol;

public readonly struct ChampionCombatStats
{
    public readonly float AttackRange;
    public readonly float AttackSpeed;
    public readonly float WindupRatio;
    public readonly float Damage;
    public readonly float ProjectileSpeed;

    private ChampionCombatStats(float attackRange, float attackSpeed, float windupRatio, float damage,
                                float projectileSpeed)
    {
        AttackRange = attackRange;
        AttackSpeed = attackSpeed;
        WindupRatio = windupRatio;
        Damage = damage;
        ProjectileSpeed = projectileSpeed;
    }

    public bool IsRanged => ProjectileSpeed > 0.0f;
    public double AttackCycleSeconds => 1.0 / AttackSpeed;
    public double WindupSeconds => AttackCycleSeconds * WindupRatio;

    public static ChampionCombatStats For(PLAYER_CHAMPION_TYPE champType)
    {
        ChampionData data =
            ChampionDatabase.Get(champType) ?? ChampionDatabase.Get(PLAYER_CHAMPION_TYPE.PlayerTypeGaren);

        return new ChampionCombatStats(data.AttackRangeUnits, data.AttackSpeed, data.AttackWindupPercent,
                                       data.AttackDamage.AtLevel(1),
                                       data.Ranged ? data.BasicAttackMissileSpeed * ChampionData.UnitScale : 0.0f);
    }
}
