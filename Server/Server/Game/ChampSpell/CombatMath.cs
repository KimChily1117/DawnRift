using Google.Protobuf.Protocol;
using Server.Game.Map;
using Server.Game.Objects;
using Server.Game.Room;
using System;

public static class CombatMath
{
    public static int LevelOf(GameObject unit) => unit is Player player && Progression.IsChampion(player)
                                                      ? player.Progress.Level
                                                      : 1;

    public static ChampionData DataOf(GameObject unit) => unit?.Info == null
                                                              ? null
                                                              : ChampionDatabase.Get(unit.Info.ChampType);

    public static float AttackDamage(GameObject unit)
    {
        float damage = DataOf(unit)?.AttackDamage.AtLevel(LevelOf(unit)) ?? unit?.Info?.Attack ?? 0f;

        if (unit is Player armed && Progression.IsChampion(armed))
            damage += armed.ItemStats.Ad;

        if (EpicBuffs.HasHandOfBaron(unit))
            damage += EpicBuffs.HandOfBaronAttackDamage;

        return unit?.Room?.EpicBuffs != null ? damage * unit.Room.EpicBuffs.AttackDamageMultiplier(unit) : damage;
    }

    public static float BonusAttackDamage(GameObject unit) => Math.Max(
        0f, AttackDamage(unit) - (DataOf(unit)?.AttackDamage.AtLevel(LevelOf(unit)) ?? unit?.Info?.Attack ?? 0f));

    public static float AbilityPower(GameObject unit) => unit is Player caster && Progression.IsChampion(caster)
                                                             ? caster.ItemStats.Ap
                                                             : 0f;

    public static float Mitigate(GameObject target, float damage, int damageType, GameObject source = null)
    {
        if (damageType == DamageTypes.True)
            return damage;
        ChampionData data = DataOf(target);

        if (data == null)
            return damage;
        int level = LevelOf(target);
        float resist = damageType == DamageTypes.Magic ? data.MagicResist.AtLevel(level) : data.Armor.AtLevel(level);

        if (target is Player bonus)
        {
            resist += damageType == DamageTypes.Magic ? bonus.Status.BonusMagicResist : bonus.Status.BonusArmor;

            if (Progression.IsChampion(bonus))
                resist += damageType == DamageTypes.Magic ? bonus.ItemStats.Mr : bonus.ItemStats.Armor;
        }

        if (source is Player piercer && Progression.IsChampion(piercer) && resist > 0f)
        {
            Server.Game.Items.ItemStats items = piercer.ItemStats;

            if (damageType == DamageTypes.Magic)
                resist = Math.Max(0f, resist * (1f - items.MagicPenPercent) - items.MagicPen);
            else
                resist = Math.Max(0f, resist * (1f - items.ArmorPen) -
                                          items.Lethality * (0.6f + 0.4f * LevelOf(piercer) / 18f));
        }

        if (target.Room?.EpicBuffs != null)
            resist *= target.Room.EpicBuffs.ResistMultiplier(target);

        if (damageType == DamageTypes.Physical && target is Player shredded && resist > 0f)
            resist *= 1f - shredded.Status.ArmorShred;

        return resist >= 0f ? damage * 100f / (100f + resist) : damage * (2f - 100f / (100f - resist));
    }

    public static int ApplyDamage(GameRoom room, GameObject source, GameObject target, float rawDamage, int damageType,
                                  int skillId = -1, bool reflectable = true, bool crit = false)
    {
        if (target?.Info == null || target.Info.Hp <= 0)
            return 0;

        if (target is MapUnit removed && removed.Room == null)
            return 0;

        if (target is MapUnit structure && !structure.IsTargetable)
            return 0;
        // Zhonya's / Stopwatch / Guardian Angel: untargetable.
        if (target is Player frozen && frozen.Status.InStasis)
            return 0;

        if (source is Player weakened)
            rawDamage *= weakened.Status.OutgoingDamageMultiplier;

        if (source is Player armed && Progression.IsChampion(armed))
            rawDamage *= Server.Game.Items.ItemEffects.OutgoingMultiplier(armed, target, damageType, skillId);

        if (target is Player wearer && Progression.IsChampion(wearer))
        {
            float incoming =
                Server.Game.Items.ItemEffects.IncomingMultiplier(room, wearer, source, damageType, skillId, crit);

            if (incoming <= 0f)
                return 0;
            rawDamage *= incoming;
        }

        float mitigated = Mitigate(target, rawDamage, damageType, source);

        if (target is Ward)
            mitigated = 1f;
        Player player = target as Player;
        float afterShield = mitigated;
        float shieldBefore = 0f;

        if (player != null)
        {
            mitigated *= 1f - player.Status.DamageReduction;

            if (player is Minion minion && EpicBuffs.IsEmpowered(minion))
                mitigated *= 1f - EpicBuffs.EmpoweredMinionReduction;
            shieldBefore = player.Status.Shield;
            afterShield = player.Status.Absorb(mitigated);
            player.Status.LastDamagedAt = StatusEffects.Now;
        }

        int dealt = Math.Max(0, (int)Math.Round(mitigated));

        if (source is Player dealer && source.Info != null && source.Info.TeamId != target.Info.TeamId)
        {
            dealer.LastHitTargetId = target.Info.ObjectId;
            dealer.LastHitAt = StatusEffects.Now;
        }

        if (source?.Info != null && source.Info.TeamId != target.Info.TeamId && Progression.IsChampion(target))
            room.Vision.Reveal(source, target.Info.TeamId);
        int healthLoss = Math.Max(0, (int)Math.Round(afterShield));

        if (player != null && healthLoss >= target.Info.Hp &&
            Server.Game.Items.ItemEffects.TryPreventDeath(room, player, source))
            healthLoss = target.Info.Hp - 1;
        target.Info.Hp = Math.Max(0, target.Info.Hp - healthLoss);

        if (target is MapUnit hitUnit && source != null)
            hitUnit.OnDamaged(source);

        if (healthLoss > 0 || dealt > 0)
            room.Scoreboard.OnDamaged(source, target, skillId, dealt, damageType);

        if (player != null && dealt > 0)
            Recall.Cancel(room, player);
        var damage = new S_Damage { TargetId = target.Info.ObjectId, Damage = dealt, RemainHp = target.Info.Hp,
                                    SourceId = source?.Info?.ObjectId ?? 0, DamageType = damageType };

        if (skillId >= 0)
            damage.SkillId = skillId;
        damage.Crit = crit;
        room.Broadcast(damage);

        if (source is Player dealerItems && Progression.IsChampion(dealerItems))
            Server.Game.Items.ItemEffects.OnDamageDealt(room, dealerItems, target, dealt, rawDamage, damageType,
                                                        skillId, crit);

        if (player != null && Progression.IsChampion(player))
            Server.Game.Items.ItemEffects.OnDamageTaken(room, player, source, dealt, rawDamage, damageType, skillId);

        if (player != null && Progression.IsChampion(player) &&
            MapUnit.ChampionOf(room, source) is Player dragonSource && dragonSource.Info.TeamId != player.Info.TeamId &&
            skillId != Buffs.PassiveSkillId)
            EpicBuffs.OnChampionDamage(room, dragonSource, player, dealt);

        if (player != null && shieldBefore > 0f)
        {
            Buffs.SendHp(room, player);

            if (player.Status.Shield <= 0f)
                room.Broadcast(new S_Buff { TargetId = player.Info.ObjectId, BuffType = BUFF_TYPE.BuffShield,
                                            SkillId = player.Status.ShieldSkillId });
        }

        if (player != null && reflectable && skillId >= 0 && player.Status.HasReflect && source is Player attacker &&
            attacker.Info != null && attacker.Info.TeamId != player.Info.TeamId)
        {
            GameObject shieldOwner = room.FindObject(player.Status.ShieldSourceId) ?? player;
            ApplyDamage(room, shieldOwner, attacker, player.Status.ReflectDamage, DamageTypes.Magic,
                        (int)SkillType.ESpell, false);
        }

        if (target.Info.Hp <= 0)
        {
            var dead = new S_Dead { TargetId = target.Info.ObjectId };

            if (player != null)
            {
                player.CancelBasicAttackWindup();
                room.CancelPlayerMovement(player);

                if (player.Status.HasHandOfBaron)
                    room.Broadcast(new S_Buff { TargetId = player.Info.ObjectId, BuffType = BUFF_TYPE.BuffHandOfBaron,
                                                SkillId = Buffs.PassiveSkillId });
                player.Status.Clear();

                if (player is MapUnit mapUnit)
                {
                    dead.Gold = mapUnit.OnKilled(source);
                    dead.KillerId = MapUnit.ChampionOf(room, source)?.Info?.ObjectId ?? 0;
                    Server.Game.Items.ItemEffects.OnUnitKilled(room, MapUnit.ChampionOf(room, source), mapUnit);
                }
                else if (Progression.IsChampion(player))
                {
                    dead.RespawnSeconds = Progression.OnChampionDeath(room, player, source, out ulong killerId);
                    dead.KillerId = killerId;
                }
            }

            room.Broadcast(dead);
        }

        return dealt;
    }
}
