using System;
using System.Collections.Generic;
using System.Diagnostics;

public sealed class StatusEffects
{
    private struct SpeedBuff
    {
        public float Amount;
        public long StartsAt, EndsAt;
        public bool Decays;
    }

    private readonly List<SpeedBuff> _speed = new List<SpeedBuff>();

    private readonly List<SpeedBuff> _slows = new List<SpeedBuff>();
    private readonly List<SpeedBuff> _attackSpeed = new List<SpeedBuff>();
    private long _charmEndsAt;
    private long _shieldEndsAt, _reflectEndsAt, _damageReductionEndsAt, _tenacityEndsAt;
    private long _stunEndsAt, _silenceEndsAt, _armorShredEndsAt;
    private float _damageReduction, _tenacity, _armorShred;

    public float Shield { get; private set; }
    public ulong ShieldSourceId { get; private set; }
    public int ShieldSkillId { get; private set; }

    public float ReflectDamage { get; private set; }
    public long LastDamagedAt { get; set; }

    // Annie passive: spell casts toward the next stun.
    public int PyromaniaStacks { get; set; }
    public bool StunReady { get; set; }

    // Garen E: Judgment hits per target until the armor shred.
    public readonly Dictionary<ulong, int> JudgmentHits = new Dictionary<ulong, int>();

    public static long Now => Server.Game.GameClock.Now;
    public static long After(double seconds) => Now + (long)(seconds * Stopwatch.Frequency);

    public bool IsStunned => Now < _stunEndsAt || InStasis;

    public long StasisEndsAt { get; set; }
    public bool InStasis => Now < StasisEndsAt;

    public void Cleanse()
    {
        _stunEndsAt = _silenceEndsAt = _charmEndsAt = 0;
        _slows.Clear();
        NearsightEndsAt = 0;
    }

    public bool IsCharmed => Now < _charmEndsAt;
    public ulong CharmSourceId { get; private set; }
    public float CharmSlow { get; private set; }
    // Neither orders nor casts: stunned or charmed.
    public bool IsDisabled => IsStunned || IsCharmed;

    public long DashEndsAt { get; set; }
    public float DashSpeed { get; set; }
    public bool IsDashing => Now < DashEndsAt;

    public int AsheFocus { get; set; }
    public bool AsheCritPending { get; set; }
    public long AsheFocusExpiresAt { get; set; }
    public long AsheQEndsAt { get; set; }
    public bool AsheQActive => Now < AsheQEndsAt;

    public int AhriRDashes { get; set; }
    public long AhriRWindowEndsAt { get; set; }
    public long AhriRNextDashAt { get; set; }
    public int AhriEssence { get; set; }

    public float BonusArmor => Now < _bonusResistEndsAt ? _bonusArmor : 0f;
    public float BonusMagicResist => Now < _bonusResistEndsAt ? _bonusMagicResist : 0f;
    private float _bonusArmor, _bonusMagicResist;
    private long _bonusResistEndsAt;

    public void SetBonusResists(float armor, float magicResist, double seconds)
    {
        _bonusArmor = armor;
        _bonusMagicResist = magicResist;
        _bonusResistEndsAt = After(seconds);
    }

    public float OutgoingDamageMultiplier => Now < _weakenedEndsAt ? 1f - _weakened : 1f;
    private float _weakened;
    private long _weakenedEndsAt;

    public void Weaken(float fraction, double seconds)
    {
        _weakened = fraction;
        _weakenedEndsAt = After(seconds);
    }

    public float OnHitBonus { get; private set; }
    public ulong OnHitBonusSourceId { get; private set; }
    private long _onHitBonusEndsAt;

    public void SetOnHitBonus(float amount, double seconds, ulong sourceId)
    {
        OnHitBonus = amount;
        OnHitBonusSourceId = sourceId;
        _onHitBonusEndsAt = After(seconds);
    }

    public bool TakeOnHitBonus(out float amount, out ulong sourceId)
    {
        amount = OnHitBonus;
        sourceId = OnHitBonusSourceId;
        bool ready = amount > 0f && Now < _onHitBonusEndsAt;
        OnHitBonus = 0f;

        return ready;
    }

    public long NearsightEndsAt { get; set; }
    public bool IsNearsighted => Now < NearsightEndsAt;

    public int SonaSong { get; set; }
    public long SonaAuraEndsAt { get; set; }
    public int SonaChord { get; set; }

    public int GravesShells { get; set; } = 2;
    public long GravesReloadEndsAt { get; set; }
    public bool GravesReloading => Now < GravesReloadEndsAt;
    public int GravesStacks { get; set; }

    public List<KatarinaDagger> KatarinaDaggers { get; } = new List<KatarinaDagger>();
    public long KatarinaREndsAt { get; set; }
    public bool KatarinaChanneling => Now < KatarinaREndsAt;

    private long _grievousEndsAt;
    private float _grievous;

    public void SetGrievousWounds(float fraction, double seconds)
    {
        _grievous = Now < _grievousEndsAt ? Math.Max(_grievous, fraction) : fraction;
        _grievousEndsAt = After(seconds);
    }

    public bool IsSilenced => Now < _silenceEndsAt;
    public float DamageReduction => Now < _damageReductionEndsAt ? _damageReduction : 0f;
    public float ArmorShred => Now < _armorShredEndsAt ? _armorShred : 0f;
    public float Tenacity => Now < _tenacityEndsAt ? _tenacity : 0f;

    public ulong IgniteSourceId { get; private set; }
    public float IgniteDamagePerTick { get; private set; }
    public int IgniteTicksLeft { get; set; }
    public long IgniteNextTickAt { get; set; }
    private long _igniteEndsAt;
    private float _grievousWounds;

    public long RecentHealUntil { get; set; }

    public float HealingMultiplier =>
        1f - Math.Max(Now < _igniteEndsAt ? _grievousWounds : 0f, Now < _grievousEndsAt ? _grievous : 0f);

    public void SetIgnite(ulong sourceId, float damagePerTick, int ticks, double tickSeconds, float grievousWounds)
    {
        IgniteSourceId = sourceId;
        IgniteDamagePerTick = damagePerTick;
        IgniteTicksLeft = ticks;
        IgniteNextTickAt = After(tickSeconds);
        _igniteEndsAt = After(ticks * tickSeconds);
        _grievousWounds = grievousWounds;
    }

    public void ClearIgnite()
    {
        IgniteSourceId = 0;
        IgniteTicksLeft = 0;
        _igniteEndsAt = 0;
    }

    // Baron's buff (EpicBuffs); death ends it (Clear).
    public long HandOfBaronEndsAt { get; set; }
    public bool HasHandOfBaron => Now < HandOfBaronEndsAt;

    public long ElderEndsAt { get; set; }
    public bool HasElder => Now < ElderEndsAt;
    public long ElderBurnUntil { get; set; }
    public long SoulReadyAt { get; set; }

    public float MoveSpeedMultiplier
    {
        get {
            long now = Now;
            float bonus = 0f;

            foreach (SpeedBuff buff in _speed)
            {
                if (now >= buff.EndsAt)
                    continue;
                float remaining =
                    buff.Decays ? (float)(buff.EndsAt - now) / Math.Max(1, buff.EndsAt - buff.StartsAt) : 1f;
                bonus += buff.Amount * remaining;
            }

            float slow = 0f;

            foreach (SpeedBuff buff in _slows)
                if (now < buff.EndsAt)
                    slow = Math.Max(slow, buff.Decays? buff.Amount*(float)(buff.EndsAt - now) /
                                              Math.Max(1, buff.EndsAt - buff.StartsAt): buff.Amount);

            if (IsCharmed)
                slow = Math.Max(slow, CharmSlow);
            slow *= 1f - SlowResist;

            return (1f + bonus) * (1f - Math.Min(0.99f, slow));
        }
    }

    public float AttackSpeedBonus
    {
        get {
            long now = Now;
            float bonus = 0f;

            foreach (SpeedBuff buff in _attackSpeed)
                if (now < buff.EndsAt)
                    bonus += buff.Amount;

            return bonus;
        }
    }

    public void AddSlow(float fraction, double seconds, bool decays = false)
    {
        _slows.RemoveAll(buff => Now >= buff.EndsAt);
        _slows.Add(new SpeedBuff { Amount = fraction, StartsAt = Now, EndsAt = After(seconds), Decays = decays });
    }

    public void AddAttackSpeed(float fraction, double seconds)
    {
        _attackSpeed.RemoveAll(buff => Now >= buff.EndsAt);
        _attackSpeed.Add(new SpeedBuff { Amount = fraction, StartsAt = Now, EndsAt = After(seconds) });
    }

    public bool IsSlowed => _slows.Exists(buff => Now < buff.EndsAt);
    public float SlowResist { get; set; }

    public void Charm(double seconds, ulong sourceId, float slow)
    {
        _charmEndsAt = Math.Max(_charmEndsAt, After(seconds));
        CharmSourceId = sourceId;
        CharmSlow = slow;
    }

    public void SetShield(float amount, double seconds, ulong sourceId, int skillId, float reflectDamage)
    {
        Shield = amount;
        ShieldSourceId = sourceId;
        ShieldSkillId = skillId;
        _shieldEndsAt = After(seconds);
        ReflectDamage = reflectDamage;
        _reflectEndsAt = reflectDamage > 0f ? _shieldEndsAt : 0;
    }

    public bool HasReflect => ReflectDamage > 0f && Now < _reflectEndsAt;

    // Returns the damage left after the shield.
    public float Absorb(float damage)
    {
        if (Shield <= 0f || Now >= _shieldEndsAt)
            return damage;
        float absorbed = Math.Min(Shield, damage);
        Shield -= absorbed;

        return damage - absorbed;
    }

    public void SetDamageReduction(float fraction, double seconds)
    {
        _damageReduction = fraction;
        _damageReductionEndsAt = After(seconds);
    }

    public void SetTenacity(float fraction, double seconds)
    {
        _tenacity = fraction;
        _tenacityEndsAt = After(seconds);
    }

    public void SetArmorShred(float fraction, double seconds)
    {
        _armorShred = fraction;
        _armorShredEndsAt = After(seconds);
    }

    public void AddMoveSpeed(float amount, double seconds, bool decays)
    {
        _speed.RemoveAll(buff => Now >= buff.EndsAt);
        _speed.Add(new SpeedBuff { Amount = amount, StartsAt = Now, EndsAt = After(seconds), Decays = decays });
    }

    // CC durations; a longer one already running is kept.
    public void Stun(double seconds) => _stunEndsAt = Math.Max(_stunEndsAt, After(seconds));
    public void Silence(double seconds) => _silenceEndsAt = Math.Max(_silenceEndsAt, After(seconds));

    public bool ExpireShield()
    {
        if (Shield <= 0f || Now < _shieldEndsAt)
            return false;
        Shield = 0f;
        ReflectDamage = 0f;

        return true;
    }

    public void Clear()
    {
        StasisEndsAt = 0;
        _speed.Clear();
        _slows.Clear();
        _attackSpeed.Clear();
        _charmEndsAt = 0;
        DashEndsAt = 0;
        AsheFocus = 0;
        AsheQEndsAt = 0;
        AhriRDashes = 0;
        AhriRWindowEndsAt = 0;
        _bonusResistEndsAt = _weakenedEndsAt = _onHitBonusEndsAt = 0;
        OnHitBonus = 0f;
        NearsightEndsAt = 0;
        SonaSong = SonaChord = 0;
        SonaAuraEndsAt = 0;
        GravesShells = 2;
        GravesReloadEndsAt = 0;
        GravesStacks = 0;
        KatarinaDaggers.Clear();
        KatarinaREndsAt = _grievousEndsAt = 0;
        Shield = ReflectDamage = 0f;
        _shieldEndsAt = _reflectEndsAt = _damageReductionEndsAt = _tenacityEndsAt = 0;
        _stunEndsAt = _silenceEndsAt = _armorShredEndsAt = 0;
        HandOfBaronEndsAt = 0;
        ElderEndsAt = 0;
        ClearIgnite();
        JudgmentHits.Clear();
    }
}
