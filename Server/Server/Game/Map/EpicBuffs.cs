using Google.Protobuf.Protocol;
using Server.Game.Objects;
using Server.Game.Room;
using System;
using System.Collections.Generic;
using System.Linq;

namespace Server.Game.Map
{
    public sealed class EpicBuffs
    {
        public const float InfernalPerStack = 0.08f;
        public const float MountainPerStack = 0.08f;
        public const float OceanPerStack = 0.025f / 5f;
        public const float CloudPerStack = 0.07f;
        public const int MaxStacks = 4;
        public const double ElderSeconds = 150.0;
        public const float ElderExecuteShare = 0.2f;
        public const double SoulCooldownSeconds = 3.0;
        public const float CloudSoulSpeed = 0.15f;

        public const double HandOfBaronSeconds = 180.0;
        public const float HandOfBaronAttackDamage = 40f;
        public const float EmpowerRange = 8.0f;
        public const float EmpoweredMinionDamage = 1.5f;
        public const float EmpoweredMinionReduction = 0.5f;

        public enum Element
        {
            Infernal,
            Mountain,
            Ocean,
            Cloud
        }

        private readonly Dictionary<int, int[]> _stacks = new Dictionary<int, int[]>();
        private readonly Dictionary<int, Element> _souls = new Dictionary<int, Element>();

        public Element? Soul(int team) => _souls.TryGetValue(team, out Element element) ? element : (Element?)null;
        public bool AnySoul => _souls.Count > 0;

        public static BUFF_TYPE BuffOf(Element element) => element switch {
            Element.Infernal => BUFF_TYPE.BuffDragonInfernal,
            Element.Mountain => BUFF_TYPE.BuffDragonMountain,
            Element.Ocean => BUFF_TYPE.BuffDragonOcean,
            _ => BUFF_TYPE.BuffDragonCloud,
        };

        public static Element ElementOf(PLAYER_CHAMPION_TYPE dragon) => dragon switch {
            PLAYER_CHAMPION_TYPE.PlayerTypeSruDragonFire => Element.Infernal,
            PLAYER_CHAMPION_TYPE.PlayerTypeSruDragonEarth => Element.Mountain,
            PLAYER_CHAMPION_TYPE.PlayerTypeSruDragonWater => Element.Ocean,
            _ => Element.Cloud,
        };

        public static PLAYER_CHAMPION_TYPE DragonOf(Element element) => element switch {
            Element.Infernal => PLAYER_CHAMPION_TYPE.PlayerTypeSruDragonFire,
            Element.Mountain => PLAYER_CHAMPION_TYPE.PlayerTypeSruDragonEarth,
            Element.Ocean => PLAYER_CHAMPION_TYPE.PlayerTypeSruDragonWater,
            _ => PLAYER_CHAMPION_TYPE.PlayerTypeSruDragonAir,
        };

        public int Stacks(int team,
                          Element element) => _stacks.TryGetValue(team, out int[] stacks) ? stacks[(int)element] : 0;
        public int Dragons(int team) => _stacks.TryGetValue(team, out int[] stacks) ? stacks.Sum() : 0;

        public bool AddDragon(int team, Element element)
        {
            if (Dragons(team) >= MaxStacks)
                return false;

            if (!_stacks.TryGetValue(team, out int[] stacks))
                _stacks[team] = stacks = new int[4];
            ++stacks[(int)element];
            // The fourth dragon grants the soul of its element.
            if (stacks.Sum() >= MaxStacks && !_souls.ContainsKey(team))
                _souls[team] = element;

            return true;
        }

        public void Clear()
        {
            _stacks.Clear();
            _souls.Clear();
        }

        // ---- souls and Elder in combat ----

        private static float BonusAd(Player p) => CombatMath.BonusAttackDamage(p);
        private static float Ap(Player p) => CombatMath.AbilityPower(p);
        private static float BonusHp(Player p) =>
            Math.Max(0f, p.Info.MaxHp - (CombatMath.DataOf(p)?.Hp.AtLevel(CombatMath.LevelOf(p)) ?? p.Info.MaxHp));

        public static void OnChampionDamage(GameRoom room, Player source, Player target, int dealt)
        {
            if (room?.EpicBuffs == null || source?.Info == null || target?.Info == null || dealt <= 0 ||
                target.Info.Hp <= 0)
                return;

            if (source.Status.HasElder)
            {
                if (target.Info.Hp < target.Info.MaxHp * ElderExecuteShare)
                {
                    room.Broadcast(new S_ItemEffect { ItemId = 90001, CasterId = source.Info.ObjectId,
                                                      TargetId = target.Info.ObjectId,
                                                      Position = target.Info.Position });
                    CombatMath.ApplyDamage(room, source, target, target.Info.Hp + 1, DamageTypes.True,
                                           Buffs.PassiveSkillId, reflectable: false);

                    return;
                }

                if (StatusEffects.Now >= target.Status.ElderBurnUntil)
                {
                    target.Status.ElderBurnUntil = StatusEffects.After(3.0);
                    float minutes = (room.Map?.Elapsed ?? 0f) / 60f;
                    float perTick = (75f + Math.Clamp(minutes - 25f, 0f, 20f) * 7.5f) / 3f;
                    ulong targetId = target.Info.ObjectId;

                    for (int tick = 1; tick <= 3; ++tick)
                        room.PushAfter(1000 * tick,
                                       () =>
                                       {
                                           if (ReferenceEquals(room.FindObject(targetId), target) && target.Info.Hp > 0)
                                               CombatMath.ApplyDamage(room, source, target, perTick, DamageTypes.True,
                                                                      Buffs.PassiveSkillId, reflectable: false);
                                       });
                }
            }

            Element? soul = room.EpicBuffs.Soul(source.Info.TeamId);

            if (soul == null || StatusEffects.Now < source.Status.SoulReadyAt)
                return;

            if (soul == Element.Infernal)
            {
                source.Status.SoulReadyAt = StatusEffects.After(SoulCooldownSeconds);
                float blast = 80f + 0.225f * BonusAd(source) + 0.135f * Ap(source) + 0.0275f * BonusHp(source);

                foreach (GameObject unit in room.GetObjectsInRange(target.Info.Position.ToNumericsVector3(), 2.5f))
                    if (unit is Player enemy && enemy.Info != null && enemy.Info.TeamId != source.Info.TeamId &&
                        enemy.Info.Hp > 0)
                        CombatMath.ApplyDamage(room, source, enemy, blast, DamageTypes.Magic, Buffs.PassiveSkillId,
                                               reflectable: false);
            }
            else if (soul == Element.Ocean)
            {
                source.Status.SoulReadyAt = StatusEffects.After(4.0);
                float heal = (160f + 0.36f * BonusAd(source) + 0.24f * Ap(source) + 0.12f * BonusHp(source)) / 3f;
                ulong id = source.Info.ObjectId;

                for (int tick = 1; tick <= 3; ++tick)
                    room.PushAfter(1000 * tick,
                                   () =>
                                   {
                                       if (ReferenceEquals(room.FindObject(id), source) && source.Info.Hp > 0)
                                           Buffs.Heal(room, source, source, heal, Buffs.PassiveSkillId);
                                   });
            }
        }

        // Per tick: mountain soul's shield after 5 s without damage.
        public static void UpdateSoul(GameRoom room, Player champion)
        {
            if (room?.EpicBuffs == null || champion?.Info == null || champion.Info.Hp <= 0)
                return;

            if (room.EpicBuffs.Soul(champion.Info.TeamId) != Element.Mountain || champion.Status.Shield > 0f)
                return;

            if (StatusEffects.Now - champion.Status.LastDamagedAt <
                (long)(5.0 * System.Diagnostics.Stopwatch.Frequency))
                return;
            float shield = 180f + 0.18f * BonusAd(champion) + 0.135f * Ap(champion) + 0.15f * BonusHp(champion);
            Buffs.Shield(room, null, champion, shield, 3600.0, Buffs.PassiveSkillId);
        }

        public float AttackDamageMultiplier(GameObject unit) => 1f + InfernalPerStack *
                                                                         StacksOf(unit, Element.Infernal);
        public float ResistMultiplier(GameObject unit) => 1f + MountainPerStack * StacksOf(unit, Element.Mountain);
        public float MoveSpeedMultiplier(GameObject unit) => 1f + CloudPerStack * StacksOf(unit, Element.Cloud) +
                                                             (unit?.Info != null && Progression.IsChampion(unit) &&
                                                                      Soul(unit.Info.TeamId) == Element.Cloud
                                                                  ? CloudSoulSpeed
                                                                  : 0f);
        public float MissingHealthRegenPerSecond(GameObject unit) => OceanPerStack * StacksOf(unit, Element.Ocean);

        private int StacksOf(GameObject unit, Element element) => unit?.Info != null && Progression.IsChampion(unit)
                                                                      ? Stacks(unit.Info.TeamId, element)
                                                                      : 0;

        public static bool HasHandOfBaron(GameObject unit) =>
            unit is Player player && Progression.IsChampion(player) && player.Status.HasHandOfBaron;

        public static bool IsEmpowered(Minion minion)
        {
            if (minion?.Room == null || minion.Info == null)
                return false;

            foreach (GameObject unit in minion.Room.GetObjectsInRange(minion.Info.Position.ToNumericsVector3(),
                                                                      EmpowerRange))
                if (unit.Info?.TeamId == minion.Info.TeamId && HasHandOfBaron(unit) &&
                    minion.DistanceXZ(unit) <= EmpowerRange)
                    return true;

            return false;
        }

        public void SendAll(GameRoom room)
        {
            foreach (Player champion in room.Units.OfType<Player>().Where(Progression.IsChampion))
                SendTo(room, champion);
        }

        public void SendTo(GameRoom room, Player champion)
        {
            if (champion.Info == null)
                return;

            foreach (Element element in Enum.GetValues(typeof(Element)))
            {
                int stacks = Stacks(champion.Info.TeamId, element);

                if (stacks > 0)
                    Buffs.Stacks(room, champion, champion, BuffOf(element), Buffs.PassiveSkillId, stacks);
            }

            if (champion.Status.HasHandOfBaron)
                Buffs.HandOfBaron(room, champion, champion,
                                  (champion.Status.HandOfBaronEndsAt - StatusEffects.Now) /
                                      (double)System.Diagnostics.Stopwatch.Frequency);

            if (Soul(champion.Info.TeamId) is Element soul)
                Buffs.State(room, champion, BUFF_TYPE.BuffDragonSoul, Buffs.PassiveSkillId, (int)soul, 3600.0);

            if (champion.Status.HasElder)
                Buffs.State(room, champion, BUFF_TYPE.BuffElder, Buffs.PassiveSkillId, 0f,
                            (champion.Status.ElderEndsAt - StatusEffects.Now) /
                                (double)System.Diagnostics.Stopwatch.Frequency);
        }
    }
}
