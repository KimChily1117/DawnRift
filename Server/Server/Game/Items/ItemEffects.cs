using Google.Protobuf.Protocol;
using Server.Game.Objects;
using Server.Game.Room;
using System;
using System.Collections.Generic;
using System.Linq;
using Vec3 = System.Numerics.Vector3;

namespace Server.Game.Items
{
    public sealed class ItemEffectState
    {
        public sealed class TimedStats
        {
            public int ItemId;
            public ItemStats Stats;
            public long EndsAt;
            public string Tag;
        }

        public sealed class Potion
        {
            public int ItemId;
            public float PerSecond;
            public long EndsAt;
            public float Carry;
        }

        public readonly List<TimedStats> Timed = new List<TimedStats>();
        public readonly List<Potion> Potions = new List<Potion>();
        public readonly Dictionary<int, long> Cooldowns = new Dictionary<int, long>();
        public readonly Dictionary<ulong, long> PerTarget = new Dictionary<ulong, long>();
        public readonly Dictionary<ulong, (int hits, long at)> Hits = new Dictionary<ulong, (int, long)>();
        public long SpellbladeReadyUntil, SpellbladeCooldownUntil;
        public bool SpellShieldUp;
        public long SpellShieldReadyAt;
        public int Glory;
        public int RodStacks;
        public long NextRodStackAt;
        public float HeartsteelHp;
        public float YunTalCrit;
        public int KrakenHits, HullbreakerHits, EnergyStacks, GuinsooStacks, HubrisStacks, ForceStacks, ManaCharge;
        public long GuinsooEndsAt, HubrisEndsAt, ForceDecayAt, LastEnergyTickAt;
        public long ImmolateUntil, ZekeUntil, CombatStartedAt, LastCombatAt, LastMagicDamageAt, LastDamageTakenAt;
        public long DespairNextAt, RookernReadyAt, MaelstromUntil, DivineUntil;
        public int DivineAttacks;
        public float ImmolateCarry, DespairCarry, WarmogCarry, DoranCarry;
        public long DeathfireUntil;
        public ulong DeathfireFrom;
        public Vec3? Anchor;
        public long AnchorUntil;
        public int AnchorSlot;
        public ulong OathAlly, VowAlly;
        public bool TitanicCrescent;
        public readonly Dictionary<int, long> Burns = new Dictionary<int, long>();
        public int ChampionWardsStored;
        public List<Ward> ControlWards = new List<Ward>();
        public bool NightStalkerReady;
        public float Momentum;
        public readonly HashSet<int> Burning = new HashSet<int>();
        public readonly Dictionary<ulong, (float damage, long at)> StormDamage = new Dictionary<ulong, (float, long)>();
        public int SoulCharges;
        public long VulnerableUntil;
        public long ShieldReaverUntil;
        public int ShojinStacks;
        public long ShojinEndsAt;
        public float Deferred, DeferredPerSecond;
        public float DeferredCarry;
        public bool TerminusDark;
        public int TerminusLight;
        public int BombardmentAttacks;
        public float RingCarry, OrbCarry;

        public bool On(int itemId) => Cooldowns.TryGetValue(itemId, out long until) && StatusEffects.Now < until;
        public void Cool(int itemId, double seconds) => Cooldowns[itemId] = StatusEffects.After(seconds);

        public void AddTimed(int itemId, ItemStats stats, double seconds, string tag = null)
        {
            Timed.RemoveAll(t => t.ItemId == itemId && t.Tag == tag);
            Timed.Add(
                new TimedStats { ItemId = itemId, Stats = stats, EndsAt = StatusEffects.After(seconds), Tag = tag });
        }

        public bool HasTimed(int itemId, string tag = null) => Timed.Any(t => t.ItemId == itemId && t.Tag == tag &&
                                                                              StatusEffects.Now < t.EndsAt);
    }

    public static class ItemEffects
    {
        private static readonly Random Rng = new Random();
        public static Func<double> CritRoll = () => Rng.NextDouble();
        private const float UnitScale = 0.01f;

        private static long Now => StatusEffects.Now;
        private static float Seconds(long ticks) => (float)(ticks / (double)System.Diagnostics.Stopwatch.Frequency);
        private static Vec3 PositionOf(GameObject unit) => unit.Info.Position.ToNumericsVector3();
        private static float ByLevel(float min, float max,
                                     GameObject unit) => min + (max - min) *
                                                                   (Math.Clamp(CombatMath.LevelOf(unit), 1, 18) - 1) /
                                                                   17f;
        private static bool Champion(GameObject unit) => unit is Player p && Progression.IsChampion(p);
        private static bool Enemy(GameObject me,
                                  GameObject other) => me?.Info != null && other?.Info != null
                                                       && other.Info.Hp > 0 && other.Info.TeamId != me.Info.TeamId &&
                                                       !(other is MapUnit unit && !unit.IsTargetable) &&
                                                       !(other is Ward);
        private static bool Ally(GameObject me,
                                 GameObject other) => me?.Info != null && other?.Info != null
                                                      && other.Info.Hp > 0 && other.Info.TeamId == me.Info.TeamId &&
                                                      Champion(other);

        private static float DistanceXZ(GameObject a, GameObject b)
        {
            Vec3 d = PositionOf(a) - PositionOf(b);
            d.Y = 0f;

            return d.Length();
        }

        private static IEnumerable<GameObject> EnemiesWithin(GameRoom room, GameObject me, Vec3 at, float range) =>
            room.GetObjectsInRange(at, range).Where(u => Enemy(me, u)).ToList();
        private static IEnumerable<Player> AlliesWithin(GameRoom room, Player me, float range, bool includeSelf) =>
            room.Units.OfType<Player>()
                .Where(p => Champion(p) && p.Info.Hp > 0 && p.Info.TeamId == me.Info.TeamId &&
                            (includeSelf || !ReferenceEquals(p, me)) && DistanceXZ(p, me) <= range)
                .ToList();
        private static bool Has(Player p, int itemId) => p?.Items?.Has(itemId) == true;
        private static bool HasAny(Player p, params int[] ids) => ids.Any(id => Has(p, id));
        private static float BaseAttackDamage(Player p) =>
            CombatMath.DataOf(p)?.AttackDamage.AtLevel(CombatMath.LevelOf(p)) ?? p.Info.Attack;
        private static float BonusAttackDamage(Player p) => Math.Max(0f,
                                                                     CombatMath.AttackDamage(p) - BaseAttackDamage(p));
        private static float BonusHealth(Player p) =>
            Math.Max(0f, p.Info.MaxHp - (CombatMath.DataOf(p)?.Hp.AtLevel(CombatMath.LevelOf(p)) ?? 0f));
        private static bool Ranged(Player p) => CombatMath.DataOf(p)?.Ranged ?? false;

        private static void Effect(GameRoom room, int itemId, GameObject caster, GameObject target = null,
                                   Vec3? at = null, IEnumerable<ulong> hits = null)
        {
            var packet = new S_ItemEffect { ItemId = itemId, CasterId = caster?.Info?.ObjectId ?? 0,
                                            TargetId = target?.Info?.ObjectId ?? 0 };
            Vec3 position = at ?? (target != null   ? PositionOf(target)
                                   : caster != null ? PositionOf(caster)
                                                    : Vec3.Zero);
            packet.Position = position.ToProtoVector3();

            if (hits != null)
                packet.HitObjects.AddRange(hits);
            room.Broadcast(packet);
        }

        // ---- stat totals ----

        public static ItemStats TotalStats(Player player)
        {
            var total = new ItemStats();

            if (player?.Items == null)
                return total;
            ItemEffectState s = player.ItemState;
            total.Add(player.Items.Totals);

            foreach (ItemEffectState.TimedStats timed in s.Timed)
                if (Now < timed.EndsAt)
                    total.Add(timed.Stats);

            if (Has(player, 3083))
                total.Hp += player.Items.Totals.Hp * 0.12f;
            total.Hp += s.HeartsteelHp;

            if (Has(player, 6657))
            {
                total.Hp += 10f * s.RodStacks;
                total.Mana += 30f * s.RodStacks;
                total.Ap += 3f * s.RodStacks;
            }

            total.Mana += s.ManaCharge;

            if (Has(player, 1082))
                total.Ap += 4f * Math.Min(10, s.Glory);

            if (Has(player, 3041))
            {
                total.Ap += 5f * Math.Min(25, s.Glory);

                if (s.Glory >= 10)
                    total.MsPercent += 0.10f;
            }

            if (Has(player, 3003))
                total.Ap += total.Mana * 0.01f;

            if (Has(player, 3004))
                total.Ad += total.Mana * 0.025f;

            if (Has(player, 4633))
                total.Ap += total.Hp * 0.02f;

            if (Has(player, 2501))
                total.Ad += total.Hp * 0.025f;

            if (Has(player, 3053))
                total.Ad += BaseAttackDamage(player) * 0.45f;

            if (Has(player, 3039))
                total.Crit += Math.Min(0.30f, total.Hp / 2000f * 0.30f);

            if (Has(player, 3032))
                total.Crit += s.YunTalCrit;

            if (Has(player, 6621))
            {
                total.HealShieldPower += total.ManaRegen * 0.02f;
                total.Ap += total.ManaRegen * 15f;
            }

            if (Has(player, 4011))
                total.AttackSpeed += total.HealShieldPower * 1.2f;

            if (Has(player, 2517))
                total.Haste += total.Ad * 0.1f;

            if (Has(player, 6697) && Now < s.HubrisEndsAt)
                total.Ad += 15f + 15f * Math.Min(10, s.HubrisStacks);

            if (Has(player, 4016) && s.VowAlly != 0 && player.Room?.FindObject(s.VowAlly) is Player vowed)
                total.Ap += vowed.Items.Totals.Ap * 0.10f;

            if (Has(player, 2501) && player.Info.MaxHp > 0)
                total.Ad += BaseAttackDamage(player) * 0.12f * (1f - player.Info.Hp / (float)player.Info.MaxHp);

            if (Has(player, 3089))
                total.Ap *= 1.30f;

            if (Has(player, 2503))
                total.Ap *= 1f + 0.04f * Math.Min(4, s.Burns.Count);
            total.Crit = Math.Min(1f, total.Crit);

            return total;
        }

        public static void OnInventoryChanged(GameRoom room, Player player)
        {
            ItemEffectState s = player.ItemState;

            if (HasAny(player, 3102, 3814, 4632) && !s.SpellShieldUp && Now >= s.SpellShieldReadyAt)
            {
                s.SpellShieldUp = true;
                Buffs.State(room, player, BUFF_TYPE.BuffSpellShield, Buffs.PassiveSkillId, 1f, 3600.0);
            }

            if (!HasAny(player, 3102, 3814, 4632) && s.SpellShieldUp)
            {
                s.SpellShieldUp = false;
                Buffs.State(room, player, BUFF_TYPE.BuffSpellShield, Buffs.PassiveSkillId, 0f, 0.0);
            }

            if (!Has(player, 3083))
                s.WarmogCarry = 0f;

            if (!Has(player, 6657))
            {
                s.RodStacks = 0;
                s.NextRodStackAt = 0;
            }
            else if (s.NextRodStackAt == 0)
                s.NextRodStackAt = StatusEffects.After(60.0);

            if (!HasAny(player, 1082, 3041))
                s.Glory = 0;

            if (!HasAny(player, 3070, 3004, 3003, 3119, 2526))
                s.ManaCharge = 0;

            if (!Has(player, 3084))
                s.HeartsteelHp = 0f;

            if (!Has(player, 3032))
                s.YunTalCrit = 0f;

            if (!HasAny(player, 4638, 4643) && s.ChampionWardsStored > 0)
                s.ChampionWardsStored = 0;

            if (!Has(player, 3109))
                s.OathAlly = 0;

            if (!Has(player, 4016))
                s.VowAlly = 0;
        }

        public static float AttackSpeedBonus(GameRoom room, Player player)
        {
            if (player?.Items == null)
                return 0f;
            ItemStats stats = TotalStats(player);
            ItemEffectState s = player.ItemState;
            float bonus = stats.AttackSpeed;

            if (Has(player, 3124) && Now < s.GuinsooEndsAt)
                bonus += 0.08f * Math.Min(4, s.GuinsooStacks);

            if (Has(player, 3131) && Now < s.DivineUntil)
                bonus += 1.0f;

            if (room != null && room.Units.OfType<Player>().Any(p => Champion(p) && p.Info.Hp > 0 &&
                                                                     p.Info.TeamId != player.Info.TeamId &&
                                                                     Has(p, 3110) && DistanceXZ(p, player) <= 7f))
                bonus -= 0.20f;

            return bonus;
        }

        public static (float flat, float percent) MoveSpeed(Player player)
        {
            if (player?.Items == null)
                return (0f, 0f);
            ItemStats stats = TotalStats(player);
            ItemEffectState s = player.ItemState;
            float flat = stats.Ms, percent = stats.MsPercent;
            // Youmuu's Haunt: out of combat for 5 s.
            if (Has(player, 3142) && Now - s.LastCombatAt > StatusEffects.After(5.0) - Now)
                flat += 40f;

            if (Has(player, 3742))
                flat += 20f * s.Momentum / 100f;

            return (flat, percent);
        }

        public static float Haste(Player player, int skillId)
        {
            if (player?.Items == null)
                return 0f;
            float haste = TotalStats(player).Haste;

            if (skillId >= 1 && skillId <= 3 && Has(player, 3161))
                haste += 25f;

            if (skillId >= 1 && skillId <= 3 && Has(player, 4005))
                haste += 0f;

            if (skillId == 4)
            {
                if (Has(player, 3118))
                    haste += 20f;

                if (Has(player, 3073))
                    haste += 30f;

                if (Has(player, 2512))
                    haste += 30f;
            }

            if (skillId >= 1 && skillId <= 3 && player.ItemState.HasTimed(2522, "realize"))
                haste += 43f;

            return haste;
        }

        public static float ManaCostMultiplier(Player player) => player?.ItemState != null
                                                                         && player.ItemState.HasTimed(2522, "realize")
                                                                     ? 2f
                                                                     : 1f;
        public static float SummonerHaste(Player player) => Has(player, 3158) ? 10f : 0f;

        public static float HpRegenMultiplier(Player player) => 1f + (player?.Items != null ? TotalStats(player).HpRegen
                                                                                            : 0f);
        public static float ManaRegenMultiplier(Player player) => 1f + (player?.Items != null
                                                                            ? TotalStats(player).ManaRegen
                                                                            : 0f);

        // ---- combat multipliers ----

        public static bool RollCrit(Player attacker)
        {
            if (attacker?.Items == null)
                return false;
            ItemEffectState s = attacker.ItemState;

            if (Has(attacker, 3131) && Now < s.DivineUntil)
                return true;
            float chance = TotalStats(attacker).Crit;

            if (Has(attacker, 3430))
                chance = Math.Min(1f, chance + 0.025f * Math.Min(8, s.KrakenHits));

            return chance > 0f && CritRoll() < chance;
        }

        public static float CritMultiplier(Player attacker) => 1.75f + (attacker?.Items != null
                                                                            ? TotalStats(attacker).CritDamage
                                                                            : 0f);

        public static float OutgoingMultiplier(Player source, GameObject target, int damageType, int skillId)
        {
            if (source?.Items == null || target?.Info == null)
                return 1f;
            ItemEffectState s = source.ItemState;
            float multiplier = 1f;
            bool ability = skillId >= 1 && skillId <= 4 || skillId == Buffs.PassiveSkillId;

            if (Champion(target))
            {
                float inCombat = s.CombatStartedAt != 0 && Now - s.LastCombatAt < StatusEffects.After(3.0) - Now
                                     ? Seconds(Now - s.CombatStartedAt)
                                     : 0f;

                if (Has(source, 6653))
                    multiplier += Math.Min(0.06f, 0.02f * inCombat);

                if (Has(source, 3147))
                    multiplier += Math.Min(0.06f, 0.02f * inCombat);

                if (Has(source, 4633))
                    multiplier += Math.Min(0.08f, 0.02f * inCombat);

                if (Has(source, 3036) || Has(source, 4015))
                    multiplier += Math.Min(0.15f, Math.Max(0f, target.Info.MaxHp - source.Info.MaxHp) / 1500f * 0.15f);

                if (Has(source, 4645) && (damageType == DamageTypes.Magic || damageType == DamageTypes.True) &&
                    target.Info.MaxHp > 0 && target.Info.Hp < target.Info.MaxHp * 0.4f)
                    multiplier += 0.20f;

                if (target is Player victim && victim.ItemState.DeathfireUntil > Now)
                    multiplier += 0.15f;

                if (target is Player marked && marked.ItemState.VulnerableUntil > Now)
                    multiplier += 0.07f;
            }

            if (ability && Has(source, 3161) && Now < s.ShojinEndsAt)
                multiplier += 0.03f * Math.Min(4, s.ShojinStacks);

            if (skillId == 0 && Has(source, 2523))
                multiplier += 0.10f * Math.Clamp(DistanceXZ(source, target) / 5f, 0f, 1f);

            if (ability && s.HasTimed(2522, "realize"))
                multiplier += 0.25f;

            if (ability && Has(source, 4013))
                multiplier -= 0.20f;

            return Math.Max(0.1f, multiplier);
        }

        public static float IncomingMultiplier(GameRoom room, Player victim, GameObject source, int damageType,
                                               int skillId, bool crit)
        {
            if (victim?.Items == null)
                return 1f;
            ItemEffectState s = victim.ItemState;
            Player attacker = source as Player;
            bool enemyChampion = attacker != null && Champion(attacker) && attacker.Info.TeamId != victim.Info.TeamId;

            if (s.SpellShieldUp && enemyChampion && skillId >= 1 && skillId <= 4)
            {
                s.SpellShieldUp = false;
                s.SpellShieldReadyAt = StatusEffects.After(40.0);
                Buffs.State(room, victim, BUFF_TYPE.BuffSpellShield, Buffs.PassiveSkillId, 0f, 0.0);
                Effect(room, Has(victim, 3102) ? 3102 : Has(victim, 4632) ? 4632 : 3814, victim);

                return 0f;
            }

            float multiplier = 1f;

            if (skillId == 0)
            {
                if (Has(victim, 3047))
                    multiplier -= 0.10f;

                if (Has(victim, 3082))
                    multiplier -= 0.10f;

                if (Has(victim, 2051))
                    multiplier -= 0.10f;
            }

            if (crit && Has(victim, 3143))
                multiplier -= 0.30f;

            if (damageType == DamageTypes.Magic && room != null && Champion(victim) &&
                room.Units.OfType<Player>().Any(p => Champion(p) && p.Info.Hp > 0 &&
                                                     p.Info.TeamId != victim.Info.TeamId && Has(p, 8020) &&
                                                     DistanceXZ(p, victim) <= 7f))
                multiplier += 0.12f;

            return Math.Max(0f, multiplier);
        }

        public static bool TryPreventDeath(GameRoom room, Player victim, GameObject source)
        {
            if (!Champion(victim) || !Has(victim, 3026) || victim.ItemState.On(3026))
                return false;
            victim.ItemState.Cool(3026, 300.0);
            victim.Info.Hp = 1;
            Stasis(room, victim, 4.0, 30260);
            ulong id = victim.Info.ObjectId;
            room.PushAfter(4000, () =>
                                 {
                                     if (!ReferenceEquals(room.FindObject(id), victim) || victim.Info == null ||
                                         victim.Info.Hp <= 0)
                                         return;
                                     float baseHp = CombatMath.DataOf(victim)?.Hp.AtLevel(CombatMath.LevelOf(victim)) ??
                                                    victim.Info.MaxHp;
                                     victim.Info.Hp = Math.Min(
                                         victim.Info.MaxHp, Math.Max(victim.Info.Hp, (int)Math.Round(baseHp * 0.5f)));
                                     victim.Progress.Mana = victim.Progress.MaxMana;
                                     victim.Info.Mp = (int)victim.Progress.Mana;
                                     victim.Progress.Dirty = true;
                                     Buffs.SendHp(room, victim);
                                     Effect(room, 3026, victim);
                                 });

            return true;
        }

        public static void Stasis(GameRoom room, Player player, double seconds, int itemId)
        {
            player.Status.StasisEndsAt = StatusEffects.After(seconds);
            player.CancelBasicAttackWindup();
            room.CancelPlayerMovement(player);
            Buffs.State(room, player, BUFF_TYPE.BuffStasis, Buffs.PassiveSkillId, itemId, seconds);
            Effect(room, itemId, player);
        }

        // ---- combat events ----

        public static void OnBasicAttackHit(GameRoom room, Player attacker, GameObject target, int dealt, bool crit)
        {
            if (attacker?.Items == null || target?.Info == null || !Enemy(attacker, target))
                return;
            ItemEffectState s = attacker.ItemState;
            ItemStats stats = TotalStats(attacker);
            float ad = CombatMath.AttackDamage(attacker), bonusAd = BonusAttackDamage(attacker);
            bool champion = Champion(target), structure = target is Structure || target is Turret;
            var onHitMagic = 0f;
            var onHitPhysical = 0f;
            var onHitTrue = 0f;
            float onHitTimes = 1f;

            if (Has(attacker, 3124))
            {
                s.GuinsooStacks = Now < s.GuinsooEndsAt ? Math.Min(4, s.GuinsooStacks + 1) : 1;
                s.GuinsooEndsAt = StatusEffects.After(4.0);
                onHitMagic += 30f;

                if (s.GuinsooStacks >= 4 && ++s.KrakenHits % 3 == 0)
                    onHitTimes = 2f;
            }

            if (Has(attacker, 1043))
                onHitPhysical += 15f;

            if (Has(attacker, 3115))
                onHitMagic += 15f + 0.20f * stats.Ap;

            if (Has(attacker, 3091))
                onHitMagic += ByLevel(15f, 80f, attacker);

            if (Has(attacker, 3302))
                onHitMagic += 30f;

            if (Has(attacker, 3144) && s.On(3144))
                s.Cooldowns[3144] -= System.Diagnostics.Stopwatch.Frequency;

            if (Has(attacker, 3179) && champion && s.NightStalkerReady)
            {
                s.NightStalkerReady = false;
                onHitTrue += ByLevel(65f, 175f, attacker);
                Effect(room, 3179, attacker, target);
            }

            if (Has(attacker, 3742) && s.Momentum >= 100f)
            {
                onHitPhysical += 40f + 0.0f;
                s.Momentum = 0f;
                Effect(room, 3742, attacker, target);
            }

            if (Has(attacker, 3302) && champion)
            {
                s.TerminusDark = !s.TerminusDark;

                if (s.TerminusDark)
                    s.AddTimed(3302, new ItemStats { ArmorPen = 0.10f, MagicPenPercent = 0.10f }, 5.0, "dark");
                else
                {
                    s.TerminusLight = s.HasTimed(3302, "light") ? Math.Min(3, s.TerminusLight + 1) : 1;
                    s.AddTimed(3302, new ItemStats { Armor = 6f * s.TerminusLight, Mr = 6f * s.TerminusLight }, 5.0,
                               "light");
                }
            }

            if (Has(attacker, 2512) && s.BombardmentAttacks > 0)
            {
                s.BombardmentAttacks--;

                if (crit)
                    onHitTrue += dealt * 0.15f;
                else
                    onHitPhysical += ad * 0.6f;
            }

            if (Has(attacker, 3032) && champion && !s.On(3032))
            {
                s.Cool(3032, 30.0);
                s.AddTimed(3032, new ItemStats { AttackSpeed = 0.30f }, 6.0, "windspeed");
            }
            else if (Has(attacker, 3032) && s.On(3032))
                s.Cooldowns[3032] -= (long)((crit ? 2.0 : 1.0) * System.Diagnostics.Stopwatch.Frequency);

            if (Has(attacker, 6675))
                for (int skill = 1; skill <= 3; ++skill)
                    if (attacker.CooldownRemaining(skill) > 0.0)
                        attacker.ReduceSkillCooldown(skill, attacker.CooldownRemaining(skill) * 0.15);

            if (Has(attacker, 4011))
            {
                Player weakest = AlliesWithin(room, attacker, 7f, false)
                                     .OrderBy(p => p.Info.Hp / (float)Math.Max(1, p.Info.MaxHp))
                                     .FirstOrDefault();

                if (weakest != null)
                    Heal(room, attacker, weakest, 10f + 0.05f * stats.Ap, Buffs.PassiveSkillId, quiet: true);
            }

            if (HasAny(attacker, 1056, 1054, 1120, 3070) && target is Minion)
                onHitPhysical += 5f;

            if (Has(attacker, 1083))
                Heal(room, attacker, attacker, 3f, Buffs.PassiveSkillId);

            if (Has(attacker, 3153))
            {
                float percent = Ranged(attacker) ? 0.05f : 0.08f;
                float mist = Math.Max(15f, target.Info.Hp * percent);

                if (!champion)
                    mist = Math.Min(mist, 60f);
                onHitPhysical += mist;

                if (champion && Track(s, target, 3153, 3, 3.0))
                {
                    Buffs.Slow(room, attacker, (Player)target, 0.30f, 1.0, Buffs.PassiveSkillId);
                    Effect(room, 3153, attacker, target);
                }
            }

            if (Has(attacker, 6672) && ++s.KrakenHits % 3 == 0)
            {
                float missing = attacker.Info.MaxHp > 0 ? 1f - attacker.Info.Hp / (float)attacker.Info.MaxHp : 0f;
                onHitPhysical += (35f + 0.65f * bonusAd) * (1f + 0.5f * missing);
                Effect(room, 6672, attacker, target);
            }

            if (Has(attacker, 3181) && (champion || structure || target is JungleMonster m && m.IsEpic) &&
                ++s.HullbreakerHits % 5 == 0)
                onHitPhysical += (65f + 0.035f * attacker.Info.MaxHp) * (structure ? 2f : 1f);

            if (Has(attacker, 3084) && champion && !PerTargetOn(s, target, 3084))
            {
                PerTargetCool(s, target, 3084, 30.0);
                float colossal = 70f + 0.06f * attacker.Info.MaxHp;
                onHitPhysical += colossal;
                s.HeartsteelHp += colossal * 0.10f;
                Progression.ApplyLevelStats(attacker);
                Effect(room, 3084, attacker, target);
            }

            if (Has(attacker, 6610) && champion && !PerTargetOn(s, target, 6610))
            {
                PerTargetCool(s, target, 6610, 8.0);
                onHitPhysical += ad * 0.75f;
                Heal(room, attacker, attacker, 0.06f * attacker.Info.MaxHp + BaseAttackDamage(attacker) * 1.2f,
                     Buffs.PassiveSkillId);
            }

            if (Has(attacker, 3748) && !structure)
            {
                onHitPhysical += 4f + 0.015f * attacker.Info.MaxHp;

                if (s.TitanicCrescent)
                {
                    onHitPhysical += 0.4f * ad;
                    s.TitanicCrescent = false;
                }

                Cleave(room, attacker, target, (Ranged(attacker) ? 0.2f : 0.4f) * ad + 0.015f * attacker.Info.MaxHp,
                       3748);
            }

            if (HasAny(attacker, 3077, 3074, 6698, 6631) && !structure)
                Cleave(room, attacker, target, (Ranged(attacker) ? 0.2f : 0.4f) * ad,
                       Has(attacker, 3074)   ? 3074
                       : Has(attacker, 6698) ? 6698
                       : Has(attacker, 6631) ? 6631
                                             : 3077);

            if (Has(attacker, 3085))
                foreach (GameObject other in EnemiesWithin(room, attacker, PositionOf(target), 3.5f)
                             .Where(u => !ReferenceEquals(u, target))
                             .Take(2))
                    CombatMath.ApplyDamage(room, attacker, other, 0.4f * ad, DamageTypes.Physical,
                                           Buffs.PassiveSkillId);
            // Spellblade: the attack after a spell.
            if (Now < s.SpellbladeReadyUntil && HasAny(attacker, 3057, 3078, 3100, 6662, 3508, 2510))
            {
                s.SpellbladeReadyUntil = 0;
                s.SpellbladeCooldownUntil = StatusEffects.After(1.5);
                float baseAd = BaseAttackDamage(attacker);
                int spellblade = Has(attacker, 3078)   ? 3078
                                 : Has(attacker, 3100) ? 3100
                                 : Has(attacker, 6662) ? 6662
                                 : Has(attacker, 3508) ? 3508
                                 : Has(attacker, 2510) ? 2510
                                                       : 3057;

                switch (spellblade)
                {
                case 3078:
                    onHitPhysical += 2.0f * baseAd;
                    break;
                case 3100:
                    onHitMagic += 0.75f * baseAd + 0.4f * stats.Ap;
                    break;
                case 6662:
                    onHitPhysical += baseAd;

                    if (target is Player frozen)
                        Buffs.Slow(room, attacker, frozen, 0.20f, 2.0, Buffs.PassiveSkillId);
                    break;
                case 3508:
                    onHitPhysical += baseAd + 0.4f * bonusAd;
                    attacker.Progress.Mana =
                        Math.Min(attacker.Progress.MaxMana, attacker.Progress.Mana + attacker.Progress.MaxMana * 0.15f);
                    attacker.Progress.Dirty = true;
                    break;
                case 2510:
                    onHitMagic += baseAd + 0.3f * stats.Ap;
                    Heal(room, attacker, attacker, 0.5f * baseAd, Buffs.PassiveSkillId);
                    onHitTimes += 1f;
                    break;
                default:
                    onHitPhysical += baseAd;
                    break;
                }

                Effect(room, spellblade, attacker, target);
            }

            if (HasAny(attacker, 3087, 3095, 3094, 6699))
            {
                s.EnergyStacks = Math.Min(100, s.EnergyStacks + 25);

                if (s.EnergyStacks >= 100)
                {
                    s.EnergyStacks = 0;

                    if (Has(attacker, 6699))
                    {
                        onHitPhysical += 100f + 0.05f * target.Info.Hp;
                        s.AddTimed(6699, new ItemStats { Lethality = 18f }, 4.0, "firmament");
                        Effect(room, 6699, attacker, target);
                    }

                    if (Has(attacker, 3087))
                    {
                        onHitMagic += 90f;
                        var hits = new List<ulong>();

                        foreach (GameObject other in EnemiesWithin(room, attacker, PositionOf(target), 4.5f)
                                     .Where(u => !ReferenceEquals(u, target))
                                     .Take(5))
                        {
                            CombatMath.ApplyDamage(room, attacker, other, 90f, DamageTypes.Magic, Buffs.PassiveSkillId);
                            hits.Add(other.Info.ObjectId);
                        }

                        Effect(room, 3087, attacker, target, null, hits);
                    }

                    if (Has(attacker, 3095))
                    {
                        onHitMagic += 90f;
                        Buffs.MoveSpeed(room, attacker, attacker, 0.45f, 1.5, Buffs.PassiveSkillId, decays: true);
                        Effect(room, 3095, attacker, target);
                    }

                    if (Has(attacker, 3094))
                    {
                        onHitMagic += 40f;
                        Effect(room, 3094, attacker, target);
                    }
                }
            }

            if (Has(attacker, 3131) && Now < s.DivineUntil && ++s.DivineAttacks >= 3)
                s.DivineUntil = 0;

            if (Has(attacker, 3032))
                s.YunTalCrit = Math.Min(0.25f, s.YunTalCrit + 0.0025f);

            if (HasAny(attacker, 3044, 3078))
                s.AddTimed(3044, new ItemStats { Ms = 20f }, 2.0, "rage");
            // Eclipse: two hits on a champion within 2 s shield.
            if (Has(attacker, 6692) && champion && Track(s, target, 6692, 2, 2.0) && !s.On(6692))
            {
                s.Cool(6692, 6.0);
                Shield(room, attacker, attacker, 100f + 0.4f * bonusAd, 2.0, Buffs.PassiveSkillId);
                Effect(room, 6692, attacker);
            }

            // Thornmail / Bramble Vest on the victim: reflect and wounds.
            if (target is Player hurt && HasAny(hurt, 3075, 3076))
            {
                float reflect = Has(hurt, 3075) ? 10f + 0.25f * TotalStats(hurt).Armor : 10f;
                CombatMath.ApplyDamage(room, hurt, attacker, reflect, DamageTypes.Magic, Buffs.PassiveSkillId,
                                       reflectable: false);
                GrievousWounds(room, hurt, attacker, 0.4f, 3.0);
                Effect(room, Has(hurt, 3075) ? 3075 : 3076, hurt, attacker);
            }

            if (onHitPhysical > 0f)
                CombatMath.ApplyDamage(room, attacker, target, onHitPhysical * onHitTimes, DamageTypes.Physical,
                                       Buffs.PassiveSkillId);

            if (onHitMagic > 0f)
                CombatMath.ApplyDamage(room, attacker, target, onHitMagic * onHitTimes, DamageTypes.Magic,
                                       Buffs.PassiveSkillId);

            if (onHitTrue > 0f)
                CombatMath.ApplyDamage(room, attacker, target, onHitTrue, DamageTypes.True, Buffs.PassiveSkillId);
        }

        private static void Cleave(GameRoom room, Player attacker, GameObject target, float damage, int itemId)
        {
            var hits = new List<ulong>();

            foreach (GameObject other in EnemiesWithin(room, attacker, PositionOf(target), 3.5f)
                         .Where(u => !ReferenceEquals(u, target)))
            {
                CombatMath.ApplyDamage(room, attacker, other, damage, DamageTypes.Physical, Buffs.PassiveSkillId);
                hits.Add(other.Info.ObjectId);
            }

            if (hits.Count > 0)
                Effect(room, itemId, attacker, target, null, hits);
        }

        private static bool Track(ItemEffectState s, GameObject target, int key, int needed, double window)
        {
            ulong id = target.Info.ObjectId ^ ((ulong)key << 40);
            int hits = s.Hits.TryGetValue(id, out var entry) && Now - entry.at < StatusEffects.After(window) - Now
                           ? entry.hits + 1
                           : 1;

            if (hits >= needed)
            {
                s.Hits.Remove(id);

                return true;
            }

            s.Hits[id] = (hits, Now);

            return false;
        }

        private static bool PerTargetOn(ItemEffectState s, GameObject target, int key) =>
            s.PerTarget.TryGetValue(target.Info.ObjectId ^ ((ulong)key << 40), out long until) && Now < until;
        private static void PerTargetCool(ItemEffectState s, GameObject target, int key, double seconds) =>
            s.PerTarget[target.Info.ObjectId ^ ((ulong)key << 40)] = StatusEffects.After(seconds);

        public static void OnDamageDealt(GameRoom room, Player source, GameObject target, int dealt, float rawDamage,
                                         int damageType, int skillId, bool crit)
        {
            if (source?.Items == null || target?.Info == null || dealt <= 0 || !Enemy(source, target) ||
                source.Info.Hp <= 0)
                return;
            ItemEffectState s = source.ItemState;
            ItemStats stats = TotalStats(source);
            bool champion = Champion(target), ability = skillId >= 1 && skillId <= 4,
                 structure = target is Structure || target is Turret;

            if (champion)
            {
                if (s.CombatStartedAt == 0 || Now - s.LastCombatAt > StatusEffects.After(3.0) - Now)
                    s.CombatStartedAt = Now;
                s.LastCombatAt = Now;
            }

            if (!structure)
            {
                float vamp = 0f;

                if (skillId == 0 || skillId == Buffs.PassiveSkillId && damageType == DamageTypes.Physical)
                    vamp += stats.LifeSteal;
                vamp += stats.Omnivamp;

                if (Has(source, 2140) && champion && damageType == DamageTypes.Physical && s.HasTimed(2140))
                    vamp += 0.12f;

                if (Has(source, 3156) && s.HasTimed(3156, "lifeline"))
                    vamp += 0.10f;

                if (vamp > 0f)
                {
                    float heal = dealt * vamp;
                    int missing = source.Info.MaxHp - source.Info.Hp;

                    if (Has(source, 3072) && heal > missing)
                    {
                        float overflow = heal - missing;

                        if (overflow > source.Status.Shield)
                            Buffs.Shield(room, source, source, Math.Min(overflow, 50f + 0.12f * source.Info.MaxHp),
                                         25.0, Buffs.PassiveSkillId);
                    }

                    Heal(room, source, source, heal, Buffs.PassiveSkillId, quiet: true);
                }
            }

            if (champion && target is Player victim)
            {
                // Wounds: physical, magic.
                if (damageType == DamageTypes.Physical && HasAny(source, 3123, 3033, 6609))
                    GrievousWounds(room, source, victim, 0.4f, 3.0);

                if (damageType == DamageTypes.Magic && HasAny(source, 3916, 3165))
                    GrievousWounds(room, source, victim, 0.4f, 3.0);
                // Black Cleaver: armor shred stacks.
                if (damageType == DamageTypes.Physical && Has(source, 3071))
                {
                    int stacks = Math.Min(5, (victim.ItemState.Hits.TryGetValue(3071, out var c) &&
                                                      Now - c.at < StatusEffects.After(6.0) - Now
                                                  ? c.hits
                                                  : 0) +
                                                 1);
                    victim.ItemState.Hits[3071] = (stacks, Now);
                    Buffs.ArmorShred(room, source, victim, 0.06f * stacks, 6.0, Buffs.PassiveSkillId);
                    s.AddTimed(3071, new ItemStats { Ms = 20f }, 2.0, "rage");
                }

                if (damageType == DamageTypes.Magic && ability && HasAny(source, 4010, 8010))
                {
                    int stacks = Math.Min(4, (victim.ItemState.Hits.TryGetValue(8010, out var c) &&
                                                      Now - c.at < StatusEffects.After(6.0) - Now
                                                  ? c.hits
                                                  : 0) +
                                                 1);
                    victim.ItemState.Hits[8010] = (stacks, Now);
                    victim.ItemState.AddTimed(
                        8010,
                        new ItemStats { Mr = -0.075f * stacks *
                                             Math.Max(0f, TotalStats(victim).Mr +
                                                              (CombatMath.DataOf(victim)?.MagicResist.AtLevel(
                                                                   CombatMath.LevelOf(victim)) ??
                                                               0f)) },
                        6.0, "shred");
                }

                if (Has(source, 2139) && s.HasTimed(2139) && !PerTargetOn(s, victim, 2139))
                {
                    PerTargetCool(s, victim, 2139, 5.0);
                    CombatMath.ApplyDamage(room, source, victim, 25f, DamageTypes.True, Buffs.PassiveSkillId);
                }

                if (Has(source, 6676) && victim.Info.Hp > 0 && victim.Info.Hp < victim.Info.MaxHp * 0.05f)
                {
                    Effect(room, 6676, source, victim);
                    CombatMath.ApplyDamage(room, source, victim, victim.Info.Hp + 1f, DamageTypes.True,
                                           Buffs.PassiveSkillId);
                }

                if (Has(source, 4629) && (damageType == DamageTypes.Magic || damageType == DamageTypes.True))
                    s.AddTimed(4629, new ItemStats { Ms = 20f }, 4.0, "dance");

                if (Has(source, 4628) && ability && DistanceXZ(source, victim) >= 6f)
                    room.Vision.Reveal(victim, source.Info.TeamId);

                if (Has(source, 6695))
                    victim.ItemState.ShieldReaverUntil = StatusEffects.After(3.0);

                if (Has(source, 3145) && !s.On(3145))
                {
                    s.Cool(3145, 40.0);
                    CombatMath.ApplyDamage(room, source, victim, ByLevel(50f, 75f, source), DamageTypes.Magic,
                                           Buffs.PassiveSkillId);
                    Effect(room, 3145, source, victim);
                }

                if (Has(source, 3144) && !s.On(3144))
                {
                    s.Cool(3144, 10.0);
                    CombatMath.ApplyDamage(room, source, victim, ByLevel(30f, 50f, source), DamageTypes.Magic,
                                           Buffs.PassiveSkillId);
                }

                if (Has(source, 6620))
                {
                    s.SoulCharges = Math.Min(2, s.SoulCharges + (rawDamage >= 100f ? 1 : 0));
                }

                if (Has(source, 4646) && !s.On(4646))
                {
                    (float damage, long at) window = s.StormDamage.TryGetValue(victim.Info.ObjectId, out var w) &&
                                                             Now - w.at < StatusEffects.After(2.5) - Now
                                                         ? w
                                                         : (0f, Now);
                    window.damage += dealt;
                    s.StormDamage[victim.Info.ObjectId] = window;

                    if (window.damage >= 0.25f * victim.Info.MaxHp)
                    {
                        s.Cool(4646, 30.0);
                        s.StormDamage.Remove(victim.Info.ObjectId);
                        float squall = 110f + 0.25f * stats.Ap;
                        Effect(room, 4646, source, victim);
                        room.PushAfter(2000, () =>
                                             {
                                                 if (!ReferenceEquals(room.FindObject(source.Info.ObjectId), source) ||
                                                     source.Info.Hp <= 0)
                                                     return;
                                                 Effect(room, 46461, source, victim);

                                                 if (victim.Info.Hp > 0)
                                                     CombatMath.ApplyDamage(room, source, victim, squall,
                                                                            DamageTypes.Magic, Buffs.PassiveSkillId);
                                                 else
                                                     foreach (GameObject other in EnemiesWithin(room, source,
                                                                                                PositionOf(victim), 4f))
                                                         CombatMath.ApplyDamage(room, source, other, squall,
                                                                                DamageTypes.Magic,
                                                                                Buffs.PassiveSkillId);
                                             });
                    }
                }

                if (ability && Has(source, 3118) && skillId == 4)
                {
                    Burn(room, source, victim, 3118, 15f + 0.025f * stats.Ap, 3);
                    victim.ItemState.AddTimed(3118, new ItemStats { Mr = -10f }, 3.0, "hatefog");
                }

                if (ability && Has(source, 4017) && !PerTargetOn(s, victim, 4017))
                {
                    PerTargetCool(s, victim, 4017, 10.0);
                    float share = 0.01f + 0.03f * Math.Clamp((victim.Info.MaxHp - source.Info.MaxHp) / 3000f, 0f, 1f);
                    float perTick = victim.Info.Hp * share / 4f;
                    ulong victimId = victim.Info.ObjectId;

                    for (int tick = 1; tick <= 4; ++tick)
                        room.PushAfter(1000 * tick,
                                       () =>
                                       {
                                           if (ReferenceEquals(room.FindObject(victimId), victim) && victim.Info.Hp > 0)
                                               CombatMath.ApplyDamage(room, source, victim, perTick,
                                                                      DamageTypes.Physical, Buffs.PassiveSkillId);
                                       });
                }

                if (ability && Has(source, 6699))
                    s.EnergyStacks = 100;

                if (ability && Has(source, 3161))
                {
                    s.ShojinStacks = Now < s.ShojinEndsAt ? Math.Min(4, s.ShojinStacks + 1) : 1;
                    s.ShojinEndsAt = StatusEffects.After(6.0);
                }
            }

            if (ability && !structure)
            {
                if (Has(source, 3116) && target is Player chilled)
                {
                    Buffs.Slow(room, source, chilled, 0.30f, 1.0, Buffs.PassiveSkillId);

                    if (!PerTargetOn(s, chilled, 3116))
                    {
                        PerTargetCool(s, chilled, 3116, 1.0);
                        Effect(room, 3116, source, chilled);
                    }
                }

                if (Has(source, 6694) && target is Player frozen && target.Info.Hp < target.Info.MaxHp * 0.5f)
                    Buffs.Slow(room, source, frozen, 0.30f, 1.0, Buffs.PassiveSkillId);

                if (Has(source, 6653))
                    Burn(room, source, target, 6653,
                         Math.Min(champion ? float.MaxValue : 50f, 0.02f * target.Info.MaxHp), 3);

                if (Has(source, 2503))
                    Burn(room, source, target, 2503, 7.5f + 0.015f * stats.Ap, 3);

                if (Has(source, 2508))
                    Burn(room, source, target, 2508, 5f, 3);

                if (Has(source, 6655) && !s.On(6655))
                {
                    s.Cool(6655, 10.0);
                    float echo = 100f + 0.10f * stats.Ap;
                    var hits = new List<ulong> { target.Info.ObjectId };
                    CombatMath.ApplyDamage(room, source, target, echo, DamageTypes.Magic, Buffs.PassiveSkillId);

                    foreach (GameObject other in EnemiesWithin(room, source, PositionOf(target), 4f)
                                 .Where(u => !ReferenceEquals(u, target))
                                 .Take(5))
                    {
                        CombatMath.ApplyDamage(room, source, other, echo * 0.2f, DamageTypes.Magic,
                                               Buffs.PassiveSkillId);
                        hits.Add(other.Info.ObjectId);
                    }

                    Effect(room, 6655, source, target, null, hits);
                }

                if (Has(source, 4013) && champion)
                    foreach (GameObject other in EnemiesWithin(room, source, PositionOf(target), 4f)
                                 .Where(u => !ReferenceEquals(u, target))
                                 .Take(1))
                        CombatMath.ApplyDamage(room, source, other, dealt * 0.666f, damageType, Buffs.PassiveSkillId);

                if (Has(source, 2520) && (champion || target is JungleMonster epic && epic.IsEpic) && !s.On(2520))
                {
                    s.Cool(2520, 8.0);
                    CombatMath.ApplyDamage(room, source, target, 40f + 0.04f * target.Info.MaxHp, DamageTypes.True,
                                           Buffs.PassiveSkillId);
                }

                if (Has(source, 3146) || Has(source, 3115))
                {
                }

                if (HasAny(source, 3070, 3004, 3003, 3119, 2526) && !s.On(3070))
                {
                    s.Cool(3070, 8.0);
                    s.ManaCharge = Math.Min(360, s.ManaCharge + (champion ? 6 : 3));
                    Progression.ApplyLevelStats(source);
                }
            }

            if (skillId == 0 && HasAny(source, 3004, 3119) && !s.On(3070))
            {
                s.Cool(3070, 8.0);
                s.ManaCharge = Math.Min(360, s.ManaCharge + (champion ? 6 : 3));
                Progression.ApplyLevelStats(source);
            }

            if (HasAny(source, 3068, 6660, 6664) && !structure)
                s.ImmolateUntil = StatusEffects.After(3.0);
        }

        private static void Burn(GameRoom room, Player source, GameObject target, int itemId, float perTick, int ticks)
        {
            if (!(target is Player victim))
                return;

            victim.ItemState.Burns[itemId] = StatusEffects.After(ticks + 0.5);

            if (!victim.ItemState.Burning.Add(itemId))
                return;
            Effect(room, itemId, source, victim);
            ulong sourceId = source.Info.ObjectId, targetId = victim.Info.ObjectId;
            void Tick()
            {
                if (!ReferenceEquals(room.FindObject(targetId), victim) || victim.Info.Hp <= 0 ||
                    !ReferenceEquals(room.FindObject(sourceId), source) ||
                    !victim.ItemState.Burns.TryGetValue(itemId, out long until) || Now >= until)
                {
                    victim.ItemState.Burns.Remove(itemId);
                    victim.ItemState.Burning.Remove(itemId);

                    return;
                }

                CombatMath.ApplyDamage(room, source, victim, perTick, DamageTypes.Magic, Buffs.PassiveSkillId);
                room.PushAfter(1000, Tick);
            }

            room.PushAfter(1000, Tick);
        }

        public static void GrievousWounds(GameRoom room, GameObject source, Player target, float fraction,
                                          double seconds)
        {
            target.Status.SetGrievousWounds(fraction, seconds);
            Buffs.State(room, target, BUFF_TYPE.BuffGrievousWounds, Buffs.PassiveSkillId, fraction, seconds);
        }

        public static void OnDamageTaken(GameRoom room, Player victim, GameObject source, int dealt, float rawDamage,
                                         int damageType, int skillId)
        {
            if (victim?.Items == null || dealt <= 0 || victim.Info == null)
                return;
            ItemEffectState s = victim.ItemState;
            s.LastDamageTakenAt = Now;
            Player attacker = source as Player;
            bool fromChampion = attacker != null && Champion(attacker) && attacker.Info.TeamId != victim.Info.TeamId;

            if (fromChampion)
            {
                if (s.CombatStartedAt == 0 || Now - s.LastCombatAt > StatusEffects.After(3.0) - Now)
                    s.CombatStartedAt = Now;
                s.LastCombatAt = Now;
            }

            if (damageType == DamageTypes.Magic)
                s.LastMagicDamageAt = Now;

            if (victim.Info.Hp <= 0)
                return;
            float fraction = victim.Info.MaxHp > 0 ? victim.Info.Hp / (float)victim.Info.MaxHp : 1f;
            // Lifelines at 30% health.
            if (fraction < 0.30f)
            {
                if (Has(victim, 3053) && !s.On(3053))
                {
                    s.Cool(3053, 90.0);
                    Shield(room, victim, victim, Math.Max(300f, 0.6f * BonusHealth(victim)), 4.5, Buffs.PassiveSkillId);
                    Effect(room, 3053, victim);
                }

                if (Has(victim, 6673) && !s.On(6673))
                {
                    s.Cool(6673, 90.0);
                    Shield(room, victim, victim, ByLevel(320f, 530f, victim), 3.0, Buffs.PassiveSkillId);
                    Effect(room, 6673, victim);
                }

                if (damageType == DamageTypes.Magic && Has(victim, 3155) && !s.On(3155))
                {
                    s.Cool(3155, 90.0);
                    Shield(room, victim, victim, ByLevel(110f, 280f, victim), 2.5, Buffs.PassiveSkillId);
                    Effect(room, 3155, victim);
                }

                if (damageType == DamageTypes.Magic && Has(victim, 3156) && !s.On(3156))
                {
                    s.Cool(3156, 90.0);
                    Shield(room, victim, victim, ByLevel(200f, 450f, victim), 3.0, Buffs.PassiveSkillId);
                    s.AddTimed(3156, new ItemStats(), 8.0, "lifeline");
                    Effect(room, 3156, victim);
                }

                if (Has(victim, 2525) && !s.On(2525))
                {
                    s.Cool(2525, 90.0);
                    s.AddTimed(2525,
                               new ItemStats { Hp = 0.15f * victim.Info.MaxHp, MsPercent = 0.10f, Tenacity = 0.25f },
                               5.0, "lifeline");
                    Progression.ApplyLevelStats(victim);
                    Potion(victim, 2525, 0.15f * victim.Info.MaxHp / 5f, 5.0);
                    Effect(room, 2525, victim);
                }
            }

            // Eternity: mana from damage taken.
            if (fromChampion && HasAny(victim, 3803, 6657, 4402) && victim.Progress.MaxMana > 0f)
            {
                victim.Progress.Mana = Math.Min(victim.Progress.MaxMana,
                                                victim.Progress.Mana + rawDamage * (Has(victim, 4402) ? 0.15f : 0.10f));
                victim.Progress.Dirty = true;
            }

            if (fromChampion && damageType == DamageTypes.Magic && Has(victim, 4401))
            {
                s.ForceStacks = Now < s.ForceDecayAt ? Math.Min(8, s.ForceStacks + 1) : 1;
                s.ForceDecayAt = StatusEffects.After(6.0);

                if (s.ForceStacks >= 8)
                    s.AddTimed(4401, new ItemStats { Mr = 70f, MsPercent = 0.06f }, 6.0, "steadfast");
            }

            if (fromChampion && Has(victim, 6665) && Seconds(Now - s.CombatStartedAt) >= 5f)
                s.AddTimed(
                    6665,
                    new ItemStats { Armor = 0.3f * victim.Items.Totals.Armor, Mr = 0.3f * victim.Items.Totals.Mr }, 3.0,
                    "voidborn");

            if (Has(victim, 1054) && fromChampion)
                s.AddTimed(1054, new ItemStats(), 8.0, "focus");

            if (Has(victim, 6333) && fromChampion && damageType == DamageTypes.Physical)
            {
                float deferred = dealt * 0.30f;
                victim.Info.Hp = Math.Min(victim.Info.MaxHp, victim.Info.Hp + (int)Math.Round(deferred));
                s.Deferred += deferred;
                s.DeferredPerSecond = s.Deferred / 3f;
                Buffs.SendHp(room, victim);
            }

            if (HasAny(victim, 3068, 6660, 6664))
                s.ImmolateUntil = StatusEffects.After(3.0);

            if (fromChampion && room != null)
                foreach (Player protector in room.Units.OfType<Player>().Where(
                             p => Has(p, 3109) && p.ItemState.OathAlly == victim.Info.ObjectId && p.Info.Hp > 0 &&
                                  DistanceXZ(p, victim) <= 10f))
                    CombatMath.ApplyDamage(room, source, protector, dealt * 0.14f, DamageTypes.True,
                                           Buffs.PassiveSkillId, reflectable: false);
        }

        // Spells arm spellblades, charge mana items and tick Eternity.
        public static void OnSpellCast(GameRoom room, Player player, int skillId, float manaSpent)
        {
            if (player?.Items == null)
                return;
            ItemEffectState s = player.ItemState;

            if (HasAny(player, 3057, 3078, 3100, 6662, 3508, 2510) && Now >= s.SpellbladeCooldownUntil)
                s.SpellbladeReadyUntil = StatusEffects.After(10.0);

            if (HasAny(player, 3803, 6657, 4402) && manaSpent > 0f)
                Heal(room, player, player, Math.Min(20f, manaSpent * 0.25f), Buffs.PassiveSkillId, quiet: true);

            if (Has(player, 4402))
            {
                Potion(player, 4402, 0.01f * (player.Info.MaxHp - player.Info.Hp), 3.0);
            }

            if (Has(player, 3430))
                s.KrakenHits = Math.Min(8, s.KrakenHits + 1);

            if (skillId == 4)
            {
                if (Has(player, 3050))
                {
                    s.ZekeUntil = StatusEffects.After(5.0);
                    Effect(room, 3050, player);
                }

                if (Has(player, 3073))
                    s.AddTimed(3073, new ItemStats { AttackSpeed = 0.30f, MsPercent = 0.15f }, 8.0, "overdrive");

                if (Has(player, 2512))
                {
                    s.AddTimed(2512, new ItemStats { AttackSpeed = 0.50f }, 8.0, "bombardment");
                    s.BombardmentAttacks = 3;
                }
            }
        }

        public static void OnLevelUp(GameRoom room, Player player)
        {
            if (player?.Items == null || player.Progress.MaxMana <= 0f)
                return;

            if (HasAny(player, 3802, 6655, 2503, 3118, 4646))
            {
                player.Progress.Mana =
                    Math.Min(player.Progress.MaxMana, player.Progress.Mana + player.Progress.MaxMana * 0.2f);
                player.Progress.Dirty = true;
            }
        }

        public static void OnTakedown(GameRoom room, Player player, Player victim, bool killer)
        {
            if (player?.Items == null)
                return;
            ItemEffectState s = player.ItemState;

            if (HasAny(player, 1082, 3041))
                s.Glory = Math.Min(Has(player, 3041) ? 25 : 10, s.Glory + 1);

            if (Has(player, 6697))
            {
                s.HubrisStacks = Math.Min(10, s.HubrisStacks + 1);
                s.HubrisEndsAt = StatusEffects.After(90.0);
            }

            if (Has(player, 6676) && killer)
                Progression.GrantGold(player, 25f);

            if (Has(player, 6696) && player.CooldownRemaining(4) > 0.0)
                player.ReduceSkillCooldown(4, player.CooldownRemaining(4) * 0.15f);

            if (Has(player, 3137))
                Heal(room, player, player, ByLevel(50f, 200f, player), Buffs.PassiveSkillId);

            if (Has(player, 6333))
            {
                s.Deferred = 0f;
                Heal(room, player, player, 0.10f * player.Info.MaxHp, Buffs.PassiveSkillId);
            }

            if (Has(player, 2517))
                s.AddTimed(2517, new ItemStats { Omnivamp = 0.12f }, 6.0, "feast");

            if (Has(player, 3008))
                s.AddTimed(3008, new ItemStats { Omnivamp = 0.006f * Math.Min(10, ++s.ForceStacks) }, 3600.0,
                           "slaughter");
            Progression.ApplyLevelStats(player);
        }

        public static void OnUnitKilled(GameRoom room, Player killer, GameObject unit)
        {
            if (killer?.Items == null || unit == null)
                return;
            ItemEffectState s = killer.ItemState;

            if (Has(killer, 1083) && unit is Minion && s.ForceStacks < 100)
            {
                Progression.GrantGold(killer, 1f);

                if (++s.ForceStacks >= 100)
                    Progression.GrantGold(killer, 350f);
            }

            if (Has(killer, 6664))
            {
                Effect(room, 66641, killer, null, PositionOf(unit));

                foreach (GameObject other in EnemiesWithin(room, killer, PositionOf(unit), 3.5f))
                    CombatMath.ApplyDamage(room, killer, other, 60f, DamageTypes.Magic, Buffs.PassiveSkillId);
            }
        }

        public static void OnDeath(GameRoom room, Player victim)
        {
            if (victim?.Items == null)
                return;
            ItemEffectState s = victim.ItemState;

            if (Has(victim, 3041))
                s.Glory = Math.Max(0, s.Glory - 10);
            else if (Has(victim, 1082))
                s.Glory = Math.Max(0, s.Glory - 5);
            s.Timed.RemoveAll(t => t.Tag != null && t.ItemId != 2138 && t.ItemId != 2139 && t.ItemId != 2140);
            s.Potions.Clear();
            s.ImmolateUntil = s.ZekeUntil = s.SpellbladeReadyUntil = s.AnchorUntil = 0;
            s.Anchor = null;
            s.GuinsooStacks = s.KrakenHits = s.EnergyStacks = 0;
            s.Deferred = s.Momentum = 0f;
            s.ShojinStacks = s.SoulCharges = s.BombardmentAttacks = 0;
            s.Burns.Clear();
            s.CombatStartedAt = 0;
            victim.Status.StasisEndsAt = 0;
            Progression.ApplyLevelStats(victim);
        }

        // ---- healing and shields with heal and shield power ----

        public static int Heal(GameRoom room, GameObject source, Player target, float amount, int skillId,
                               bool quiet = false)
        {
            if (target?.Info == null || target.Info.Hp <= 0 || amount <= 0f)
                return 0;

            if (source is Player healer && Champion(healer) && healer.Items != null)
                amount *= 1f + TotalStats(healer).HealShieldPower;

            if (Has(target, 3065))
                amount *= 1.25f;
            amount *= target.Status.HealingMultiplier;
            int before = target.Info.Hp;
            target.Info.Hp = Math.Min(target.Info.MaxHp, target.Info.Hp + (int)Math.Round(amount));
            int healed = target.Info.Hp - before;
            target.Progress.Dirty = true;

            if (!quiet || healed > 0)
                Buffs.SendHp(room, target);

            if (source is Player ally && !ReferenceEquals(ally, target) && Ally(ally, target))
                OnHealOrShieldAlly(room, ally, target, amount, isHeal: true);

            return healed;
        }

        public static void Shield(GameRoom room, GameObject source, Player target, float amount, double seconds,
                                  int skillId)
        {
            Buffs.Shield(room, source, target, amount, seconds, skillId);
        }

        public static float ShieldAmount(GameObject source, Player target, float amount)
        {
            if (source is Player caster && Champion(caster) && caster.Items != null)
                amount *= 1f + TotalStats(caster).HealShieldPower;

            if (Has(target, 3065))
                amount *= 1.25f;

            if (target?.ItemState != null && target.ItemState.ShieldReaverUntil > Now)
                amount *= 0.5f;

            return amount;
        }

        public static void OnCrowdControl(GameRoom room, GameObject source, Player target, bool immobilize,
                                          double seconds)
        {
            if (!(source is Player caster) || !Champion(caster) || caster.Items == null || target?.Info == null ||
                !Champion(target))
                return;
            ItemEffectState s = caster.ItemState;

            if (target.Info.TeamId != caster.Info.TeamId)
            {
                if (Has(caster, 2524) && !s.On(2524))
                {
                    s.Cool(2524, 10.0);
                    s.AddTimed(2524, new ItemStats { Ms = 20f }, 4.0, "fanfare");

                    foreach (Player ally in AlliesWithin(room, caster, 7f, true))
                        ally.ItemState.AddTimed(2524, new ItemStats { AttackSpeed = 0.10f }, 4.0, "fanfare");
                    Effect(room, 2524, caster);
                }

                if (immobilize && Has(caster, 4005))
                {
                    target.ItemState.VulnerableUntil = StatusEffects.After(4.0);
                    Effect(room, 4005, caster, target);
                }

                if (Has(caster, 3119) && !s.On(3119))
                {
                    s.Cool(3119, 8.0);
                    Buffs.Shield(room, caster, caster, 100f + 0.045f * caster.Progress.MaxMana, 2.5,
                                 Buffs.PassiveSkillId);
                }

                if (immobilize && room != null)
                    foreach (Player protector in room.Units.OfType<Player>()
                                 .Where(p => !ReferenceEquals(p, target) && Has(p, 4012) && p.Info.Hp > 0 &&
                                             p.Info.TeamId == target.Info.TeamId && !p.ItemState.On(4012) &&
                                             DistanceXZ(p, target) <= 6f)
                                 .Take(1)
                                 .ToList())
                    {
                        protector.ItemState.Cool(4012, 20.0);
                        target.Status.Cleanse();
                        Buffs.State(room, target, BUFF_TYPE.BuffStun, Buffs.PassiveSkillId, 0f, 0.0);
                        Buffs.State(room, target, BUFF_TYPE.BuffCharm, Buffs.PassiveSkillId, 0f, 0.0);
                        Buffs.Stun(room, source, protector, seconds, Buffs.PassiveSkillId);
                        Effect(room, 4012, protector, target);
                    }
            }
        }

        // Support items that fire when an ally is healed or shielded.
        public static void OnHealOrShieldAlly(GameRoom room, Player source, Player target, float amount, bool isHeal)
        {
            if (source?.Items == null || room == null)
                return;
            ItemEffectState s = source.ItemState;

            if (Has(source, 3504))
                foreach (Player blessed in new[] { source, target })
                    blessed.ItemState.AddTimed(3504, new ItemStats { AttackSpeed = 0.25f }, 6.0, "ardent");

            if (Has(source, 6616))
                foreach (Player blessed in new[] { source, target })
                    blessed.ItemState.AddTimed(6616, new ItemStats { Ap = 40f, Haste = 15f }, 6.0, "rapids");

            if (Has(source, 6617) && !s.On(6617))
            {
                s.Cool(6617, 0.5);
                Player other = AlliesWithin(room, source, 10f, false)
                                   .Where(p => !ReferenceEquals(p, target))
                                   .OrderBy(p => p.Info.Hp / (float)Math.Max(1, p.Info.MaxHp))
                                   .FirstOrDefault();

                if (other != null)
                {
                    if (isHeal)
                        Heal(room, null, other, amount * 0.3f, Buffs.PassiveSkillId);
                    else
                        Buffs.Shield(room, null, other, amount * 0.35f, 2.0, Buffs.PassiveSkillId);
                }
            }

            if (Has(source, 6620) && s.SoulCharges > 0)
            {
                Heal(room, source, target, ByLevel(60f, 110f, source) * s.SoulCharges, Buffs.PassiveSkillId);
                s.SoulCharges = 0;
                Effect(room, 6620, source, target);
            }
        }

        // ---- per tick ----

        public static void Update(GameRoom room, Player player, float deltaTime)
        {
            if (player?.Items == null || player.Info == null || !Progression.IsChampion(player))
                return;
            ItemEffectState s = player.ItemState;
            player.Items.Update(room, player);

            player.Status.SlowResist =
                1f - (1f - (Has(player, 3009) ? 0.25f : 0f)) * (1f - (Has(player, 3742) ? 0.15f : 0f));
            // Shadow Blade's Night Stalker: 3 s out of combat.
            s.NightStalkerReady = Has(player, 3179) && Now - s.LastCombatAt > StatusEffects.After(3.0) - Now;

            if (Has(player, 3742))
                s.Momentum = Math.Clamp(
                    s.Momentum + ((player.MovementState?.IsActive ?? false) ? 40f : -50f) * deltaTime, 0f, 100f);
            // Timed effects run out: the stats change.
            if (s.Timed.RemoveAll(t => Now >= t.EndsAt) > 0)
                Progression.ApplyLevelStats(player);

            if (player.Info.Hp <= 0)
                return;
            // Potions and other heals over time.
            for (int i = s.Potions.Count - 1; i >= 0; --i)
            {
                ItemEffectState.Potion potion = s.Potions[i];
                potion.Carry += potion.PerSecond * deltaTime;

                if (potion.Carry >= 1f)
                {
                    int heal = (int)potion.Carry;
                    potion.Carry -= heal;
                    Heal(room, null, player, heal, Buffs.PassiveSkillId, quiet: true);
                }

                if (Now >= potion.EndsAt)
                    s.Potions.RemoveAt(i);
            }

            // Death's Dance: the deferred damage ticks as true damage.
            if (s.Deferred > 0f)
            {
                s.DeferredCarry += s.DeferredPerSecond * deltaTime;

                if (s.DeferredCarry >= 1f)
                {
                    int tick = (int)Math.Min(s.DeferredCarry, s.Deferred);
                    s.DeferredCarry -= tick;
                    s.Deferred -= tick;
                    CombatMath.ApplyDamage(room, null, player, tick, DamageTypes.True, Buffs.PassiveSkillId,
                                           reflectable: false);

                    if (s.Deferred < 1f)
                        s.Deferred = 0f;
                }
            }

            if (HasAny(player, 1056, 3112, 2049))
            {
                float perSecond =
                    (Has(player, 1056) ? (Now - s.LastCombatAt < StatusEffects.After(5.0) - Now ? 2f : 1f) : 0f) +
                    (Has(player, 3112) || Has(player, 2049) ? 2f : 0f);
                s.RingCarry += perSecond * deltaTime;

                if (s.RingCarry >= 1f)
                {
                    int amount = (int)s.RingCarry;
                    s.RingCarry -= amount;

                    if (player.Progress.MaxMana > 0f)
                    {
                        player.Progress.Mana = Math.Min(player.Progress.MaxMana, player.Progress.Mana + amount);
                        player.Progress.Dirty = true;
                    }
                    else
                        Heal(room, null, player, amount * (Has(player, 1056) ? 0.45f : 1.5f), Buffs.PassiveSkillId,
                             quiet: true);
                }
            }

            if (Has(player, 1054) && player.Info.Hp < player.Info.MaxHp)
            {
                s.DoranCarry += (s.HasTimed(1054, "focus") ? ByLevel(20f, 45f, player) / 5f : 0.8f) * deltaTime;

                if (s.DoranCarry >= 1f)
                {
                    int heal = (int)s.DoranCarry;
                    s.DoranCarry -= heal;
                    Heal(room, null, player, heal, Buffs.PassiveSkillId, quiet: true);
                }
            }

            // Warmog's Heart: out of combat with 1500+ bonus health.
            if (Has(player, 3083) && BonusHealth(player) >= 1500f && player.Info.Hp < player.Info.MaxHp &&
                Now - s.LastDamageTakenAt > StatusEffects.After(8.0) - Now)
            {
                s.WarmogCarry += player.Info.MaxHp * 0.025f * deltaTime;

                if (s.WarmogCarry >= 1f)
                {
                    int heal = (int)s.WarmogCarry;
                    s.WarmogCarry -= heal;
                    Heal(room, null, player, heal, Buffs.PassiveSkillId, quiet: true);
                }
            }

            // Rod of Ages: a stack a minute.
            if (Has(player, 6657) && s.RodStacks < 10 && s.NextRodStackAt != 0 && Now >= s.NextRodStackAt)
            {
                s.RodStacks++;
                s.NextRodStackAt = StatusEffects.After(60.0);
                Progression.ApplyLevelStats(player);

                if (s.RodStacks == 10)
                    Progression.GrantXp(room, player, Progression.XpToNext(player.Progress.Level));
            }

            // Immolate: Sunfire Aegis, Bami's Cinder, Hollow Radiance.
            if (Now < s.ImmolateUntil && HasAny(player, 3068, 6660, 6664))
            {
                s.ImmolateCarry += deltaTime;

                if (s.ImmolateCarry >= 1f)
                {
                    s.ImmolateCarry -= 1f;
                    float burn = Has(player, 3068)   ? 15f + 0.0175f * BonusHealth(player)
                                 : Has(player, 6664) ? 12f + 0.015f * BonusHealth(player)
                                                     : 12f + 0.01f * BonusHealth(player);
                    var hits = new List<ulong>();

                    foreach (GameObject enemy in EnemiesWithin(room, player, PositionOf(player), 3.5f)
                                 .Where(u => !(u is Structure) && !(u is Turret)))
                    {
                        CombatMath.ApplyDamage(room, player, enemy, burn * (Champion(enemy) ? 1f : 1.25f),
                                               DamageTypes.Magic, Buffs.PassiveSkillId);
                        hits.Add(enemy.Info.ObjectId);
                    }

                    if (hits.Count > 0)
                        Effect(room,
                               Has(player, 3068)   ? 3068
                               : Has(player, 6664) ? 6664
                                                   : 6660,
                               player, null, null, hits);
                }
            }

            // Zeke's storm after an ultimate.
            if (Now < s.ZekeUntil && Has(player, 3050))
            {
                s.DespairCarry += deltaTime;

                if (s.DespairCarry >= 1f)
                {
                    s.DespairCarry -= 1f;

                    foreach (Player enemy in EnemiesWithin(room, player, PositionOf(player), 5f)
                                 .OfType<Player>()
                                 .Where(Champion))
                    {
                        CombatMath.ApplyDamage(room, player, enemy, 30f, DamageTypes.Magic, Buffs.PassiveSkillId);
                        Buffs.Slow(room, player, enemy, 0.30f, 1.0, Buffs.PassiveSkillId);
                    }
                }
            }

            // Unending Despair: in combat with champions, every 4 s.
            if (Has(player, 2502) && s.CombatStartedAt != 0 && Now - s.LastCombatAt < StatusEffects.After(3.0) - Now)
            {
                if (s.DespairNextAt == 0)
                    s.DespairNextAt = StatusEffects.After(4.0);

                if (Now >= s.DespairNextAt)
                {
                    s.DespairNextAt = StatusEffects.After(4.0);
                    float anguish = 20f + 0.03f * BonusHealth(player);
                    int total = 0;

                    foreach (Player enemy in EnemiesWithin(room, player, PositionOf(player), 4f)
                                 .OfType<Player>()
                                 .Where(Champion))
                        total += CombatMath.ApplyDamage(room, player, enemy, anguish, DamageTypes.Magic,
                                                        Buffs.PassiveSkillId);

                    if (total > 0)
                    {
                        Heal(room, player, player, total * 2.5f, Buffs.PassiveSkillId);
                        Effect(room, 2502, player);
                    }
                }
            }
            else
                s.DespairNextAt = 0;

            if (Has(player, 2504) && Now - s.LastMagicDamageAt > StatusEffects.After(15.0) - Now &&
                Now >= s.RookernReadyAt && player.Status.Shield <= 0f)
            {
                s.RookernReadyAt = StatusEffects.After(15.0);
                Buffs.Shield(room, player, player, 0.18f * player.Info.MaxHp, 15.0, Buffs.PassiveSkillId);
            }

            // Spell shield returns after its cooldown.
            if (HasAny(player, 3102, 3814, 4632) && !s.SpellShieldUp && Now >= s.SpellShieldReadyAt)
            {
                s.SpellShieldUp = true;
                Buffs.State(room, player, BUFF_TYPE.BuffSpellShield, Buffs.PassiveSkillId, 1f, 3600.0);
            }

            if (HasAny(player, 3087, 3095, 3094) && Now - s.LastEnergyTickAt > StatusEffects.After(1.0) - Now)
            {
                s.LastEnergyTickAt = Now;
                s.EnergyStacks = Math.Min(100, s.EnergyStacks + 10);
            }

            if (Has(player, 3430) && s.KrakenHits > 0 && Now - s.LastCombatAt > StatusEffects.After(6.0) - Now)
                s.KrakenHits = 0;
            // Spectral Cutlass / Lifeline: the anchor pulls back.
            if (s.Anchor.HasValue && Now >= s.AnchorUntil)
                ReturnToAnchor(room, player);
            // Control wards reveal enemy wards around them.
            s.ControlWards.RemoveAll(w => w.Room == null || !w.IsAlive);

            foreach (Ward control in s.ControlWards)
                foreach (Ward enemyWard in room.Units.OfType<Ward>().Where(
                             w => w.Info.TeamId != player.Info.TeamId && w.IsAlive &&
                                  DistanceXZ(w, control) <= Ward.SightRange))
                    room.Vision.Reveal(enemyWard, player.Info.TeamId);
        }

        private static void Potion(Player player, int itemId, float perSecond, double seconds)
        {
            player.ItemState.Potions.RemoveAll(p => p.ItemId == itemId);
            player.ItemState.Potions.Add(new ItemEffectState.Potion { ItemId = itemId, PerSecond = perSecond,
                                                                      EndsAt = StatusEffects.After(seconds) });
        }

        private static void ReturnToAnchor(GameRoom room, Player player)
        {
            ItemEffectState s = player.ItemState;

            if (!s.Anchor.HasValue)
                return;
            Vec3 back = s.Anchor.Value;
            s.Anchor = null;
            s.AnchorUntil = 0;
            player.CancelBasicAttackWindup();
            room.TeleportPlayer(player, back);
            Effect(room, Has(player, 4004) ? 40041 : 40031, player, null, back);
        }

        // ---- actives ----

        public static SKILL_REJECT_REASON Use(GameRoom room, Player player, int slot, Vec3 position, ulong targetId)
        {
            if (player?.Items == null || slot < 0 || slot >= Inventory.SlotCount || player.Items.Slots[slot].Empty)
                return SKILL_REJECT_REASON.SkillRejectInvalid;
            ItemSlotState state = player.Items.Slots[slot];
            ItemData item = state.Data;

            if (item == null || !item.Active)
                return SKILL_REJECT_REASON.SkillRejectInvalid;

            if (player.Info.Hp <= 0)
                return SKILL_REJECT_REASON.SkillRejectDead;

            if (player.Status.InStasis)
                return SKILL_REJECT_REASON.SkillRejectStunned;
            bool cleanse = item.Id == 3140 || item.Id == 3139;

            if (player.Status.IsDisabled && !cleanse)
                return SKILL_REJECT_REASON.SkillRejectStunned;

            if (state.CooldownRemaining > 0.0)
                return SKILL_REJECT_REASON.SkillRejectCooldown;

            if (state.Charges == 0)
                return SKILL_REJECT_REASON.SkillRejectCooldown;
            ItemEffectState s = player.ItemState;
            GameObject target = targetId != 0 ? room.FindObject(targetId) : null;
            Vec3 at = PositionOf(player);

            double cooldown = 0.0;
            bool consume = false, charge = false;

            switch (item.Id)
            {
            case 2003:
                Potion(player, 2003, 120f / 15f, 15.0);
                consume = true;
                Effect(room, 2003, player);
                break;
            case 2031:
                Potion(player, 2031, 100f / 12f, 12.0);
                charge = true;
                Effect(room, 2031, player);
                break;
            case 2055:
                if (!PlaceControlWard(room, player, position))
                    return SKILL_REJECT_REASON.SkillRejectInvalid;
                consume = true;
                break;
            case 4638:
            case 4643:
                if (s.ChampionWardsStored <= 0 || !PlaceControlWard(room, player, position))
                    return SKILL_REJECT_REASON.SkillRejectInvalid;
                s.ChampionWardsStored--;
                player.Items.Dirty = true;
                break;
            case 2138:
                s.AddTimed(2138, new ItemStats { Hp = 300f, Tenacity = 0.25f }, 180.0);
                consume = true;
                break;
            case 2139:
                s.AddTimed(2139, new ItemStats { Ap = 50f, ManaRegen = 0.15f }, 180.0);
                consume = true;
                break;
            case 2140:
                s.AddTimed(2140, new ItemStats { Ad = 30f }, 180.0);
                consume = true;
                break;
            case 3005:
                s.AddTimed(3005, new ItemStats { Ms = 60f }, 6.0, "ghost");
                cooldown = 45.0;
                break;
            case 3077:
            case 3074:
            case 6698: {
                float ad = CombatMath.AttackDamage(player);
                float ratio = item.Id == 3077   ? (Ranged(player) ? 0.5f : 0.75f)
                              : item.Id == 3074 ? (Ranged(player) ? 0.6f : 1.0f)
                                                : 0.8f;
                var hits = new List<ulong>();

                foreach (GameObject enemy in EnemiesWithin(room, player, at, 4.5f)
                             .Where(u => !(u is Structure) && !(u is Turret)))
                {
                    int dealt = CombatMath.ApplyDamage(
                        room, player, enemy, ratio * ad + (item.Id == 6698 ? TotalStats(player).Lethality : 0f),
                        DamageTypes.Physical, Buffs.PassiveSkillId);

                    if (item.Id == 3074)
                        Heal(room, player, player, dealt * TotalStats(player).LifeSteal, Buffs.PassiveSkillId,
                             quiet: true);
                    hits.Add(enemy.Info.ObjectId);
                }

                Effect(room, item.Id, player, null, at, hits);
                cooldown = 10.0;
                break;
            }
            case 3748:
                s.TitanicCrescent = true;
                cooldown = 20.0;
                Effect(room, 3748, player);
                break;
            case 6631: {
                float ad = CombatMath.AttackDamage(player);
                var hits = new List<ulong>();
                int champions = 0;

                foreach (GameObject enemy in EnemiesWithin(room, player, at, 4.5f)
                             .Where(u => !(u is Structure) && !(u is Turret)))
                {
                    CombatMath.ApplyDamage(room, player, enemy, 0.8f * ad, DamageTypes.Physical, Buffs.PassiveSkillId);

                    if (enemy is Player slowed)
                    {
                        Buffs.Slow(room, player, slowed, 0.35f, 2.0, Buffs.PassiveSkillId);

                        if (Champion(slowed))
                            champions++;
                    }

                    hits.Add(enemy.Info.ObjectId);
                }

                if (champions > 0)
                    Buffs.MoveSpeed(room, player, player, 0.35f * champions, 3.0, Buffs.PassiveSkillId, decays: true);
                Effect(room, 6631, player, null, at, hits);
                cooldown = 15.0;
                break;
            }
            case 3140:
            case 3139:
                player.Status.Cleanse();
                Buffs.State(room, player, BUFF_TYPE.BuffStun, Buffs.PassiveSkillId, 0f, 0.0);
                Buffs.State(room, player, BUFF_TYPE.BuffSilence, Buffs.PassiveSkillId, 0f, 0.0);
                Buffs.State(room, player, BUFF_TYPE.BuffCharm, Buffs.PassiveSkillId, 0f, 0.0);
                Buffs.State(room, player, BUFF_TYPE.BuffSlow, Buffs.PassiveSkillId, 0f, 0.0);

                if (item.Id == 3139)
                    Buffs.MoveSpeed(room, player, player, 0.5f, 1.0, Buffs.PassiveSkillId, decays: true);
                Effect(room, item.Id, player);
                cooldown = 90.0;
                break;
            case 2420:
                Stasis(room, player, 2.5, 2420);
                state.Charges = 0;
                player.Items.Dirty = true;
                break;
            case 3157:
                Stasis(room, player, 2.5, 3157);
                cooldown = 120.0;
                break;
            case 4003:
            case 4004:
                if (s.Anchor.HasValue)
                {
                    ReturnToAnchor(room, player);
                    cooldown = 60.0;
                }
                else
                {
                    s.Anchor = at;
                    s.AnchorUntil = StatusEffects.After(4.0);
                    s.AnchorSlot = slot;
                    Effect(room, item.Id, player);
                }

                break;
            case 2065:
                foreach (Player ally in AlliesWithin(room, player, 7f, true))
                    Buffs.MoveSpeed(room, player, ally, 0.30f, 4.0, Buffs.PassiveSkillId);
                Effect(room, 2065, player);
                cooldown = 90.0;
                break;
            case 3190:
                foreach (Player ally in AlliesWithin(room, player, 7f, true))
                    Buffs.Shield(room, player, ally, ByLevel(290f, 360f, player), 2.5, Buffs.PassiveSkillId);
                Effect(room, 3190, player);
                cooldown = 90.0;
                break;
            case 3107: {
                Vec3 spot = Vec3.Distance(at, position) > 55f ? at + Vec3.Normalize(position - at) * 55f : position;
                Effect(room, 3107, player, null, spot);
                room.PushAfter(2500, () =>
                                     {
                                         if (!ReferenceEquals(room.FindObject(player.Info.ObjectId), player))
                                             return;
                                         var hits = new List<ulong>();

                                         foreach (GameObject unit in room.GetObjectsInRange(spot, 5.5f).ToList())
                                         {
                                             if (Ally(player, unit) || ReferenceEquals(unit, player))
                                             {
                                                 Heal(room, player, (Player)unit, ByLevel(150f, 350f, player),
                                                      Buffs.PassiveSkillId);
                                                 hits.Add(unit.Info.ObjectId);
                                             }
                                             else if (Enemy(player, unit) && Champion(unit))
                                             {
                                                 CombatMath.ApplyDamage(room, player, unit, 0.10f * unit.Info.MaxHp,
                                                                        DamageTypes.True, Buffs.PassiveSkillId);
                                                 hits.Add(unit.Info.ObjectId);
                                             }
                                         }

                                         Effect(room, 3107, player, null, spot, hits);
                                     });
                cooldown = 90.0;
                break;
            }
            case 3109:
                if (!(target is Player oath) || !Ally(player, oath) || ReferenceEquals(oath, player))
                    return SKILL_REJECT_REASON.SkillRejectInvalid;
                s.OathAlly = oath.Info.ObjectId;
                Effect(room, 3109, player, oath);
                cooldown = 1.0;
                break;
            case 4016:
                if (!(target is Player vow) || !Ally(player, vow) || ReferenceEquals(vow, player))
                    return SKILL_REJECT_REASON.SkillRejectInvalid;
                s.VowAlly = vow.Info.ObjectId;
                Progression.ApplyLevelStats(player);
                Effect(room, 4016, player, vow);
                cooldown = 1.0;
                break;
            case 3131:
                s.DivineUntil = StatusEffects.After(3.0);
                s.DivineAttacks = 0;
                Effect(room, 3131, player);
                cooldown = 90.0;
                break;
            case 3222: {
                Player ally = target as Player ?? AlliesWithin(room, player, 6.5f, false)
                                                      .OrderBy(p => p.Info.Hp / (float)Math.Max(1, p.Info.MaxHp))
                                                      .FirstOrDefault();

                if (ally == null || !Ally(player, ally) || DistanceXZ(player, ally) > 6.5f)
                    return SKILL_REJECT_REASON.SkillRejectInvalid;
                ally.Status.Cleanse();
                Buffs.State(room, ally, BUFF_TYPE.BuffStun, Buffs.PassiveSkillId, 0f, 0.0);
                Buffs.State(room, ally, BUFF_TYPE.BuffCharm, Buffs.PassiveSkillId, 0f, 0.0);
                Heal(room, player, ally, ByLevel(100f, 250f, player), Buffs.PassiveSkillId);
                Effect(room, 3222, player, ally);
                cooldown = 120.0;
                break;
            }
            case 3152: {
                Vec3 direction = position - at;
                direction.Y = 0f;

                if (direction.LengthSquared() < 1e-4f)
                    return SKILL_REJECT_REASON.SkillRejectInvalid;
                direction = Vec3.Normalize(direction);
                Vec3 landing = MapUnit.Walkable(room, at + direction * 2.75f);
                player.CancelBasicAttackWindup();
                room.TeleportPlayer(player, landing);
                var hits = new List<ulong>();
                float bolt = 100f + 0.10f * TotalStats(player).Ap;

                foreach (GameObject enemy in EnemiesWithin(room, player, landing, 7f))
                {
                    Vec3 to = PositionOf(enemy) - landing;
                    to.Y = 0f;

                    if (to.LengthSquared() > 1e-4f && Vec3.Dot(Vec3.Normalize(to), direction) < 0.5f)
                        continue;
                    CombatMath.ApplyDamage(room, player, enemy, bolt, DamageTypes.Magic, Buffs.PassiveSkillId);
                    hits.Add(enemy.Info.ObjectId);
                }

                Effect(room, 3152, player, null, landing, hits);
                cooldown = 40.0;
                break;
            }
            case 3143: {
                var hits = new List<ulong>();

                foreach (Player enemy in EnemiesWithin(room, player, at, 4.5f).OfType<Player>())
                {
                    Buffs.Slow(room, player, enemy, 0.70f, 2.0, Buffs.PassiveSkillId);
                    hits.Add(enemy.Info.ObjectId);
                }

                Effect(room, 3143, player, null, at, hits);
                cooldown = 60.0;
                break;
            }
            case 2522:
                s.AddTimed(2522, new ItemStats(), 8.0, "realize");
                Effect(room, 2522, player);
                cooldown = 60.0;
                break;
            case 3142:
                Buffs.MoveSpeed(room, player, player, 0.20f, 6.0, Buffs.PassiveSkillId);
                Effect(room, 3142, player);
                cooldown = 45.0;
                break;
            case 3128:
                if (!(target is Player grasped) || !Enemy(player, grasped) || !Champion(grasped) ||
                    DistanceXZ(player, grasped) > 7.5f)
                    return SKILL_REJECT_REASON.SkillRejectInvalid;
                grasped.ItemState.DeathfireUntil = StatusEffects.After(4.0);
                grasped.ItemState.DeathfireFrom = player.Info.ObjectId;
                CombatMath.ApplyDamage(room, player, grasped, 0.15f * grasped.Info.MaxHp, DamageTypes.Magic,
                                       Buffs.PassiveSkillId);
                Buffs.State(room, grasped, BUFF_TYPE.BuffItemActive, Buffs.PassiveSkillId, 3128, 4.0);
                Effect(room, 3128, player, grasped);
                cooldown = 90.0;
                break;
            case 3146:
                if (!(target is Player shocked) || !Enemy(player, shocked) || !Champion(shocked) ||
                    DistanceXZ(player, shocked) > 7f)
                    return SKILL_REJECT_REASON.SkillRejectInvalid;
                CombatMath.ApplyDamage(room, player, shocked, 175f + 0.30f * TotalStats(player).Ap, DamageTypes.Magic,
                                       Buffs.PassiveSkillId);
                Buffs.Slow(room, player, shocked, 0.25f, 1.5, Buffs.PassiveSkillId);
                Effect(room, 3146, player, shocked);
                cooldown = 40.0;
                break;
            default:
                return SKILL_REJECT_REASON.SkillRejectInvalid;
            }

            if (consume)
                player.Items.Consume(slot);
            else
                player.Items.CommitPurchases();

            if (charge && state.Charges > 0)
            {
                state.Charges--;
                player.Items.Dirty = true;
            }

            if (cooldown > 0.0)
            {
                state.CooldownEndsAt = StatusEffects.After(cooldown);
                state.CooldownDuration = cooldown;
            }

            // Timed stats changed the totals.
            Progression.ApplyLevelStats(player);
            player.Items.Dirty = true;

            if (item.Active && !consume && item.Id != 2031)
                Buffs.State(room, player, BUFF_TYPE.BuffItemActive, Buffs.PassiveSkillId, item.Id,
                            item.Id == 2138 || item.Id == 2139 || item.Id == 2140 ? 180.0 : 0.0);
            Console.WriteLine($"[Items] {player.Info.ObjectId} used {item.Name} ({item.Id})");

            return SKILL_REJECT_REASON.SkillRejectNone;
        }

        private static bool PlaceControlWard(GameRoom room, Player player, Vec3 position)
        {
            Vec3 start = PositionOf(player);
            Vec3 delta = position - start;
            delta.Y = 0f;

            if (delta.Length() > Trinket.PlaceRange)
                position = start + delta / delta.Length() * Trinket.PlaceRange;
            Vec3 spot = MapUnit.Walkable(room, new Vec3(position.X, start.Y, position.Z));
            ItemEffectState s = player.ItemState;
            s.ControlWards.RemoveAll(w => w.Room == null || !w.IsAlive);
            int allowed = Has(player, 4643) ? 2 : 1;

            while (s.ControlWards.Count >= allowed)
            {
                s.ControlWards[0].Expire();
                s.ControlWards.RemoveAt(0);
            }

            Ward ward = Ward.Create(room, player, spot, 3600.0);
            ward.Spawn();
            s.ControlWards.Add(ward);
            Effect(room, 2055, player, ward, spot);

            return true;
        }

        public static bool StoreWard(Player player)
        {
            if (!HasAny(player, 4638, 4643) || player.ItemState.ChampionWardsStored >= 3)
                return false;
            player.ItemState.ChampionWardsStored++;

            return true;
        }
    }
}
