using Google.Protobuf.Protocol;
using Server.Game.Objects;
using Server.Game.Room;
using System;
using System.Collections.Generic;
using System.Linq;
using Vec3 = System.Numerics.Vector3;

namespace Server.Game.Items
{
    public sealed class ItemSlotState
    {
        public int ItemId;
        public int Stacks;
        public int Charges = -1;
        public long CooldownEndsAt;
        public double CooldownDuration;

        public ItemData Data => ItemId != 0 ? ItemDatabase.Get(ItemId) : null;
        public bool Empty => ItemId == 0;
        public double CooldownRemaining =>
            CooldownEndsAt == 0
                ? 0.0
                : Math.Max(0.0, (CooldownEndsAt - StatusEffects.Now) / (double)System.Diagnostics.Stopwatch.Frequency);

        public void Clear()
        {
            ItemId = 0;
            Stacks = 0;
            Charges = -1;
            CooldownEndsAt = 0;
            CooldownDuration = 0.0;
        }
    }

    public sealed class Inventory
    {
        public const int SlotCount = 6;
        public const float ShopRange = 13f;
        public const int ShopRejectSkillId = 20;
        public const int UseRejectSkillId = 8;

        public readonly ItemSlotState[] Slots = Enumerable.Range(0, SlotCount)
                                                    .Select(
                                                        _ => new ItemSlotState())
                                                    .ToArray();
        public ItemStats Totals { get; private set; } = new ItemStats();
        public bool Dirty;
        public bool InShop;

        private sealed class Transaction
        {
            public bool Sale;
            public int ItemId, Slot, Stacks, Gold;
            public List<(int slot, int itemId, int stacks, int charges)> Consumed = new List<(int, int, int, int)>();
        }

        private readonly List<Transaction> _undo = new List<Transaction>();
        public bool CanUndo => _undo.Count > 0;

        public ItemSlotState Find(int itemId) => Slots.FirstOrDefault(s => s.ItemId == itemId);
        public bool Has(int itemId) => Find(itemId) != null;
        public int Count(int itemId) => Slots.Where(s => s.ItemId == itemId).Sum(s => s.Stacks);
        public IEnumerable<ItemData> Items => Slots.Where(s => !s.Empty).Select(s => s.Data).Where(d => d != null);

        public static bool IsAtShop(GameRoom room, Player player)
        {
            if (room?.Layout == null || player?.Info == null)
                return true;

            if (!room.Layout.Spawns.TryGetValue(player.Info.TeamId, out Vec3 fountain))
                return true;
            Vec3 at = player.Info.Position.ToNumericsVector3();
            at.Y = fountain.Y;

            return Vec3.Distance(at, fountain) <= ShopRange;
        }

        public SKILL_REJECT_REASON Buy(GameRoom room, Player player, int itemId)
        {
            ItemData item = ItemDatabase.Get(itemId);

            if (item == null || item.PriceTotal <= 0)
                return SKILL_REJECT_REASON.SkillRejectInvalid;

            if (!IsAtShop(room, player))
                return SKILL_REJECT_REASON.SkillRejectShopRange;
            var transaction = new Transaction { ItemId = itemId, Stacks = 1 };
            int slot;

            if (itemId == 2055 && ItemEffects.StoreWard(player))
            {
                // A Wardstone keeps the Control Ward (no slot).
                if (player.Progress.Gold < item.PriceTotal)
                {
                    player.ItemState.ChampionWardsStored--;

                    return SKILL_REJECT_REASON.SkillRejectGold;
                }

                player.Progress.Gold -= item.PriceTotal;
                Changed(room, player);

                return SKILL_REJECT_REASON.SkillRejectNone;
            }

            if (item.Consumable)
            {
                // Stacks onto an existing slot; otherwise needs a free one.
                ItemSlotState stack = Slots.FirstOrDefault(s => s.ItemId == itemId && s.Stacks < item.MaxStacks);
                slot = stack != null ? Array.IndexOf(Slots, stack) : Array.FindIndex(Slots, s => s.Empty);

                if (slot < 0 || Count(itemId) >= item.MaxStacks)
                    return SKILL_REJECT_REASON.SkillRejectInventory;

                if (player.Progress.Gold < item.PriceTotal)
                    return SKILL_REJECT_REASON.SkillRejectGold;
                transaction.Gold = item.PriceTotal;
                transaction.Slot = slot;
                Slots[slot].ItemId = itemId;
                Slots[slot].Stacks++;

                if (itemId == 2031 && Slots[slot].Charges < 0)
                    Slots[slot].Charges = 2;

                if (itemId == 2420)
                    Slots[slot].Charges = 1;
            }
            else
            {
                if (item.Unique && Has(itemId))
                    return SKILL_REJECT_REASON.SkillRejectInventory;

                var taken = new HashSet<int>();
                int cost = Cost(item, taken, isRoot: true);

                if (item.IsBoots &&
                    Slots.Any(s => !s.Empty && s.Data?.IsBoots == true && !taken.Contains(Array.IndexOf(Slots, s))))
                    return SKILL_REJECT_REASON.SkillRejectInventory;
                slot = taken.Count > 0 ? taken.Min() : Array.FindIndex(Slots, s => s.Empty);

                if (slot < 0)
                    return SKILL_REJECT_REASON.SkillRejectInventory;

                if (player.Progress.Gold < cost)
                    return SKILL_REJECT_REASON.SkillRejectGold;

                foreach (int index in taken)
                {
                    transaction.Consumed.Add((index, Slots[index].ItemId, Slots[index].Stacks, Slots[index].Charges));
                    Slots[index].Clear();
                }

                transaction.Gold = cost;
                transaction.Slot = slot;
                Slots[slot].ItemId = itemId;
                Slots[slot].Stacks = 1;

                if (itemId == 2420)
                    Slots[slot].Charges = 1;
            }

            player.Progress.Gold -= transaction.Gold;
            _undo.Add(transaction);

            if (_undo.Count > 8)
                _undo.RemoveAt(0);
            Changed(room, player);
            Console.WriteLine(
                $"[Items] {player.Info.ObjectId} bought {item.Name} ({itemId}) for {transaction.Gold}; gold {(int)player.Progress.Gold}");

            return SKILL_REJECT_REASON.SkillRejectNone;
        }

        private int Cost(ItemData item, HashSet<int> taken, bool isRoot)
        {
            int cost = item.PriceTotal;

            foreach (int componentId in item.From)
            {
                ItemData component = ItemDatabase.Get(componentId);

                if (component == null)
                    continue;
                int owned =
                    Array.FindIndex(Slots, s => s.ItemId == componentId && !taken.Contains(Array.IndexOf(Slots, s)));

                if (owned >= 0)
                {
                    taken.Add(owned);
                    cost -= component.PriceTotal;
                }
                else
                    cost -= component.PriceTotal - Cost(component, taken, false);
            }

            return Math.Max(isRoot ? item.Price : 0, cost);
        }

        public SKILL_REJECT_REASON Sell(GameRoom room, Player player, int slot)
        {
            if (slot < 0 || slot >= SlotCount || Slots[slot].Empty)
                return SKILL_REJECT_REASON.SkillRejectInvalid;

            if (!IsAtShop(room, player))
                return SKILL_REJECT_REASON.SkillRejectShopRange;
            ItemData item = Slots[slot].Data;
            int gold = (item?.Sell ?? 0) * Math.Max(1, Slots[slot].Stacks);
            var transaction = new Transaction { Sale = true, ItemId = Slots[slot].ItemId, Slot = slot,
                                                Stacks = Slots[slot].Stacks, Gold = gold };
            transaction.Consumed.Add((slot, Slots[slot].ItemId, Slots[slot].Stacks, Slots[slot].Charges));
            Slots[slot].Clear();
            player.Progress.Gold += gold;
            _undo.Add(transaction);
            Changed(room, player);
            Console.WriteLine(
                $"[Items] {player.Info.ObjectId} sold {item?.Name} for {gold}; gold {(int)player.Progress.Gold}");

            return SKILL_REJECT_REASON.SkillRejectNone;
        }

        public SKILL_REJECT_REASON Undo(GameRoom room, Player player)
        {
            if (_undo.Count == 0)
                return SKILL_REJECT_REASON.SkillRejectInvalid;

            if (!IsAtShop(room, player))
                return SKILL_REJECT_REASON.SkillRejectShopRange;
            Transaction last = _undo[_undo.Count - 1];

            if (last.Sale)
            {
                if (!Slots[last.Slot].Empty || player.Progress.Gold < last.Gold)
                    return SKILL_REJECT_REASON.SkillRejectGold;
                player.Progress.Gold -= last.Gold;
            }
            else
            {
                // The bought item must still be there, untouched.
                ItemSlotState slot = Slots[last.Slot];
                ItemData item = ItemDatabase.Get(last.ItemId);

                if (slot.ItemId != last.ItemId || (item != null && item.Consumable && slot.Stacks < 1))
                    return SKILL_REJECT_REASON.SkillRejectInvalid;

                if (item != null && item.Consumable && slot.Stacks > 1)
                    slot.Stacks--;
                else
                    slot.Clear();
                player.Progress.Gold += last.Gold;
            }

            foreach (var (index, itemId, stacks, charges) in last.Consumed)
            {
                Slots[index].ItemId = itemId;
                Slots[index].Stacks = stacks;
                Slots[index].Charges = charges;
            }

            _undo.RemoveAt(_undo.Count - 1);
            Changed(room, player);

            return SKILL_REJECT_REASON.SkillRejectNone;
        }

        public void CommitPurchases() => _undo.Clear();

        public void Remove(int slot)
        {
            if (slot >= 0 && slot < SlotCount)
                Slots[slot].Clear();
        }

        public void Consume(int slot)
        {
            if (slot < 0 || slot >= SlotCount || Slots[slot].Empty)
                return;

            if (--Slots[slot].Stacks <= 0)
                Slots[slot].Clear();
            CommitPurchases();
        }

        private void Changed(GameRoom room, Player player)
        {
            Recompute();
            Dirty = true;
            player.Progress.Dirty = true;
            player.Progress.SyncTimer = 0f;
            Progression.ApplyLevelStats(player);
            ItemEffects.OnInventoryChanged(room, player);
        }

        public void Recompute()
        {
            var totals = new ItemStats();

            foreach (ItemSlotState slot in Slots)
                if (!slot.Empty && slot.Data is ItemData data && !data.Consumable)
                    totals.Add(data.Stats);
            Totals = totals;
        }

        public void Update(GameRoom room, Player player)
        {
            bool atShop = IsAtShop(room, player) && room?.Layout != null;

            if (atShop && !InShop)
            {
                ItemSlotState refillable = Find(2031);

                if (refillable != null && refillable.Charges < 2)
                {
                    refillable.Charges = 2;
                    Dirty = true;
                }
            }

            if (!atShop && InShop && _undo.Count > 0)
            {
                _undo.Clear();
                Dirty = true;
            }

            if (atShop != InShop)
                Dirty = true;
            InShop = atShop;

            if (Dirty)
                Send(room, player);
        }

        public S_Inventory Build(Player player, bool owner)
        {
            var packet = new S_Inventory { TargetId = player.Info.ObjectId };

            foreach (ItemSlotState slot in Slots)
                packet.Slots.Add(new Google.Protobuf.Protocol.ItemSlot {
                    ItemId = slot.ItemId,
                    Stacks = slot.Stacks,
                    Charges = slot.Charges,
                    Cooldown = (float)slot.CooldownRemaining,
                    CooldownDuration = (float)slot.CooldownDuration,
                });

            if (owner)
            {
                packet.InShop = IsAtShop(player.Room, player);
                packet.CanUndo = CanUndo;
                packet.Gold = (int)player.Progress.Gold;
            }

            return packet;
        }

        public void Send(GameRoom room, Player player)
        {
            Dirty = false;

            if (player?.Info == null)
                return;
            player.Session?.Send(Build(player, true));
            room?.Broadcast(Build(player, false), player.Info.ObjectId);
        }
    }
}
