using Google.Protobuf.Protocol;
using Server.Game.Items;
using Server.Game.Movement;
using Server.Game.Objects;
using Server.Game.Room;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json;
using Vec3 = System.Numerics.Vector3;

namespace Server.Game.Bots
{
    public enum BotRole
    {
        Top,
        Jungle,
        Mid,
        Bottom,
        Support
    }

    public sealed class BotSettings
    {
        public int BotsBlue, BotsRed;
        public BOT_DIFFICULTY Difficulty = BOT_DIFFICULTY.Normal;
        public List<(int Team, PLAYER_CHAMPION_TYPE Champion, LOBBY_POSITION Position)> Humans =
            new List<(int, PLAYER_CHAMPION_TYPE, LOBBY_POSITION)>();

        public bool DriveHumans;
        // Tests: bots join without waiting for a client's champion.
        public bool StartImmediately;

        public static BotSettings FromEnvironment()
        {
            string value = Environment.GetEnvironmentVariable("DAWN_BOTS");

            if (string.IsNullOrWhiteSpace(value))
                return null;
            string[] parts = value.Split(',');
            var settings = new BotSettings {
                BotsBlue = parts.Length > 0 && int.TryParse(parts[0], out int blue) ? Math.Clamp(blue, 0, 5) : 4,
                BotsRed = parts.Length > 1 && int.TryParse(parts[1], out int red) ? Math.Clamp(red, 0, 5) : 5,
                Difficulty = parts.Length > 2 && parts[2].Trim() == "0" ? BOT_DIFFICULTY.Easy : BOT_DIFFICULTY.Normal,
                DriveHumans = Environment.GetEnvironmentVariable("DAWN_BOT_AUTOPILOT") == "1",
            };

            return settings.BotsBlue + settings.BotsRed > 0 ? settings : null;
        }
    }

    public sealed class BotSkill
    {
        public float ThinkSeconds, LastHitChance, SpellChance, AimError, RetreatHealth, EngageRatio;
        public bool UsesSummoners, DivesLowTargets, Shops;

        public static BotSkill For(BOT_DIFFICULTY difficulty) =>
            difficulty == BOT_DIFFICULTY.Easy
                ? new BotSkill { ThinkSeconds = 0.45f,  LastHitChance = 0.5f,    SpellChance = 0.35f,
                                 AimError = 1.6f,       RetreatHealth = 0.22f,   EngageRatio = 1.35f,
                                 UsesSummoners = false, DivesLowTargets = false, Shops = true }
                : new BotSkill { ThinkSeconds = 0.22f, LastHitChance = 0.92f,  SpellChance = 0.85f,
                                 AimError = 0.35f,     RetreatHealth = 0.32f,  EngageRatio = 1.0f,
                                 UsesSummoners = true, DivesLowTargets = true, Shops = true };
    }

    public sealed class BotDirector
    {
        private sealed class Bot
        {
            public Player Player;
            public BotRole Role;
            public string Lane = "mid";
            public float NextThink, NextMove, NextShop, NextGankCheck;
            public Vec3 MoveGoal = new Vec3(float.NaN);
            public int BuildStep;
            public bool Retreating, GoingHome;
            public ulong JungleTarget;
            public float LastSpellAt;
        }

        private static readonly Dictionary<BotRole, PLAYER_CHAMPION_TYPE[]> RoleChampions =
            new Dictionary<BotRole, PLAYER_CHAMPION_TYPE[]> {
                [BotRole.Top] = new[] { PLAYER_CHAMPION_TYPE.PlayerTypeGaren, PLAYER_CHAMPION_TYPE.PlayerTypeKatarina,
                                        PLAYER_CHAMPION_TYPE.PlayerTypeGraves, PLAYER_CHAMPION_TYPE.PlayerTypeAnnie },
                [BotRole.Jungle] =
                    new[] { PLAYER_CHAMPION_TYPE.PlayerTypeGraves, PLAYER_CHAMPION_TYPE.PlayerTypeKatarina,
                            PLAYER_CHAMPION_TYPE.PlayerTypeGaren, PLAYER_CHAMPION_TYPE.PlayerTypeAhri },
                [BotRole.Mid] = new[] { PLAYER_CHAMPION_TYPE.PlayerTypeAhri, PLAYER_CHAMPION_TYPE.PlayerTypeAnnie,
                                        PLAYER_CHAMPION_TYPE.PlayerTypeKatarina, PLAYER_CHAMPION_TYPE.PlayerTypeSona },
                [BotRole.Bottom] = new[] { PLAYER_CHAMPION_TYPE.PlayerTypeAshe, PLAYER_CHAMPION_TYPE.PlayerTypeGraves,
                                           PLAYER_CHAMPION_TYPE.PlayerTypeAhri, PLAYER_CHAMPION_TYPE.PlayerTypeAnnie },
                [BotRole.Support] = new[] { PLAYER_CHAMPION_TYPE.PlayerTypeSona, PLAYER_CHAMPION_TYPE.PlayerTypeAnnie,
                                            PLAYER_CHAMPION_TYPE.PlayerTypeGaren, PLAYER_CHAMPION_TYPE.PlayerTypeAshe },
            };
        private static readonly Dictionary<PLAYER_CHAMPION_TYPE, string> KoreanNames =
            new Dictionary<PLAYER_CHAMPION_TYPE, string> {
                [PLAYER_CHAMPION_TYPE.PlayerTypeGaren] = "가렌",
                [PLAYER_CHAMPION_TYPE.PlayerTypeAnnie] = "애니",
                [PLAYER_CHAMPION_TYPE.PlayerTypeAshe] = "애쉬",
                [PLAYER_CHAMPION_TYPE.PlayerTypeAhri] = "아리",
                [PLAYER_CHAMPION_TYPE.PlayerTypeSona] = "소나",
                [PLAYER_CHAMPION_TYPE.PlayerTypeGraves] = "그레이브즈",
                [PLAYER_CHAMPION_TYPE.PlayerTypeKatarina] = "카타리나",
            };

        private static readonly Dictionary<PLAYER_CHAMPION_TYPE, int[]> SkillOrder =
            new Dictionary<PLAYER_CHAMPION_TYPE, int[]> {
                [PLAYER_CHAMPION_TYPE.PlayerTypeGaren] = new[] { 3, 1, 2 },
                [PLAYER_CHAMPION_TYPE.PlayerTypeAnnie] = new[] { 1, 2, 3 },
                [PLAYER_CHAMPION_TYPE.PlayerTypeAshe] = new[] { 2, 1, 3 },
                [PLAYER_CHAMPION_TYPE.PlayerTypeAhri] = new[] { 1, 3, 2 },
                [PLAYER_CHAMPION_TYPE.PlayerTypeSona] = new[] { 1, 2, 3 },
                [PLAYER_CHAMPION_TYPE.PlayerTypeGraves] = new[] { 1, 3, 2 },
                [PLAYER_CHAMPION_TYPE.PlayerTypeKatarina] = new[] { 1, 3, 2 },
            };
        private static readonly BotRole[] RoleOrder = { BotRole.Top, BotRole.Jungle, BotRole.Mid, BotRole.Bottom,
                                                        BotRole.Support };

        public const float AggroRange = 10f, TurretRange = 8.6f, CampRange = 7f, AssistRange = 18f, DefendRange = 45f;
        public const float GroupAfterSeconds = 840f;
        public const float SafeFromTurretMargin = 1.2f;

        private readonly GameRoom _room;
        private readonly BotSettings _settings;
        private readonly BotSkill _skill;
        private readonly List<Bot> _bots = new List<Bot>();
        private readonly Random _random = new Random();
        private static Dictionary<string, int[]> _builds;
        private float _elapsed;
        private bool _started;

        public const int PathSearchesPerTick = 2;
        private int _pathBudget;
        private readonly Dictionary<int, float> _objectiveCallUntil = new Dictionary<int, float>();
        private readonly Dictionary<int, ulong> _objectiveTarget = new Dictionary<int, ulong>();

        public int BotCount => _bots.Count(b => b.Player.Session == null);
        public int ThinkFailures { get; private set; }
        public IEnumerable<(Player Player, BotRole Role)> Players => _bots.Select(b => (b.Player, b.Role));
        public bool Started => _started;
        public BotSettings Settings => _settings;

        public BotDirector(GameRoom room, BotSettings settings)
        {
            _room = room;
            _settings = settings;
            _skill = BotSkill.For(settings.Difficulty);
        }

        // ---- Setup ----

        public void Update(float deltaTime)
        {
            _elapsed += deltaTime;

            if (!_started)
            {
                if (_room.Layout == null || _room.Map == null || !_room.Map.Started)
                    return;

                if (!_settings.StartImmediately &&
                    !_room.Units.OfType<Player>().Any(p => p.Session != null && Progression.IsChampion(p)))
                    return;
                Start();
            }

            if (_room.Map.IsOver)
                return;

            if (_settings.DriveHumans)
                AdoptHumans();
            _pathBudget = PathSearchesPerTick;

            foreach (Bot bot in _bots.ToList())
            {
                if (bot.Player.Room != _room || _elapsed < bot.NextThink)
                    continue;
                bot.NextThink = _elapsed + _skill.ThinkSeconds * (0.8f + 0.4f * (float)_random.NextDouble());
                try
                {
                    Think(bot);
                }
                catch (Exception exception)
                {
                    ++ThinkFailures;
                    Console.WriteLine($"[Bots] {bot.Player.Info?.Name} failed to think: {exception}");
                }
            }
        }

        private void Start()
        {
            _started = true;

            foreach (int team in new[] { 1, 2 })
            {
                int count = team == 1 ? _settings.BotsBlue : _settings.BotsRed;
                var humans = _settings.Humans.Where(h => h.Team == team).ToList();
                var takenRoles = new HashSet<BotRole>(
                    humans.Select(h => ToRole(h.Position)).Where(r => r.HasValue).Select(r => r.Value));
                var takenChampions = new HashSet<PLAYER_CHAMPION_TYPE>(humans.Select(h => h.Champion));

                foreach (Player human in _room.Units.OfType<Player>().Where(
                             p => p.Session != null && Progression.IsChampion(p) && p.Info.TeamId == team))
                    takenChampions.Add(human.Info.ChampType);
                var freeRoles = RoleOrder.Where(r => !takenRoles.Contains(r)).ToList();

                for (int i = 0; i < count && i < 5; ++i)
                {
                    BotRole role = i < freeRoles.Count ? freeRoles[i] : RoleOrder[i % RoleOrder.Length];
                    PLAYER_CHAMPION_TYPE champion =
                        RoleChampions[role].FirstOrDefault(c => !takenChampions.Contains(c));

                    if (champion == PLAYER_CHAMPION_TYPE.PlayerTypeNone)
                        champion =
                            ChampionRegistry.All.Select(k => k.Type).FirstOrDefault(c => !takenChampions.Contains(c));

                    if (champion == PLAYER_CHAMPION_TYPE.PlayerTypeNone)
                        break;
                    takenChampions.Add(champion);
                    int[] skins = Server.Game.Lobbies.Roster.Default.Find(champion)?.Skins ?? new[] { 0 };
                    int skin = skins[_random.Next(skins.Length)];
                    var spells = new[] {
                        SUMMONER_SPELL.SummonerFlash,
                        role == BotRole.Jungle                              ? SUMMONER_SPELL.SummonerSmite
                        : role == BotRole.Support || role == BotRole.Bottom ? SUMMONER_SPELL.SummonerHeal
                                                                            : SUMMONER_SPELL.SummonerIgnite,
                    };
                    string name =
                        "봇 " + (KoreanNames.TryGetValue(champion, out string korean) ? korean : champion.ToString());
                    Player player = _room.SpawnBotChampion(champion, skin, team, name, spells);

                    _bots.Add(new Bot { Player = player, Role = role, Lane = LaneOf(role),
                                        NextThink = _elapsed + (float)_random.NextDouble() });
                }
            }

            if (_settings.DriveHumans)
                AdoptHumans();
            Console.WriteLine(
                $"[Bots] Bot game: {string.Join(", ", _bots.Select(b => $"{b.Player.Info.Name}({b.Player.Info.TeamId} {b.Role})"))}, difficulty {_settings.Difficulty}.");
        }

        private void AdoptHumans()
        {
            foreach (Player human in _room.Units.OfType<Player>()
                         .Where(p => p.Session != null && Progression.IsChampion(p))
                         .ToList())
            {
                if (_bots.Any(b => b.Player == human))
                    continue;
                var taken = new HashSet<BotRole>(
                    _bots.Where(b => b.Player.Info.TeamId == human.Info.TeamId).Select(b => b.Role));
                BotRole role = RoleOrder.FirstOrDefault(r => !taken.Contains(r));
                _bots.Add(new Bot { Player = human, Role = role, Lane = LaneOf(role) });
            }
        }

        private static BotRole? ToRole(LOBBY_POSITION position) => position switch {
            LOBBY_POSITION.PositionTop => BotRole.Top,
            LOBBY_POSITION.PositionJungle => BotRole.Jungle,
            LOBBY_POSITION.PositionMid => BotRole.Mid,
            LOBBY_POSITION.PositionBottom => BotRole.Bottom,
            LOBBY_POSITION.PositionSupport => BotRole.Support,
            _ => null,
        };

        private static string LaneOf(BotRole role) => role switch {
            BotRole.Top => "top", BotRole.Mid => "mid", BotRole.Bottom => "bot", BotRole.Support => "bot",
            _ => "mid",
        };

        // ---- The decision loop ----

        private void Think(Bot bot)
        {
            Player me = bot.Player;

            if (me.Info == null || me.Info.Hp <= 0)
            {
                bot.Retreating = bot.GoingHome = false;

                return;
            }

            SpendSkillPoints(me);

            if (me.Status.IsDisabled || me.Status.IsDashing || me.Info.State == OBJECT_STATE_TYPE.Skill)
                return;
            Vec3 at = Pos(me);
            int team = me.Info.TeamId;
            bool atShop = Inventory.IsAtShop(_room, me);
            float health = me.Info.Hp / (float)Math.Max(1, me.Info.MaxHp);

            // 1. At the fountain: shop, heal up, then leave.
            if (atShop)
            {
                bot.GoingHome = false;

                if (_skill.Shops && _elapsed >= bot.NextShop)
                {
                    bot.NextShop = _elapsed + 1.5f;
                    Shop(bot);
                }

                if (health < 0.85f && !EnemyChampionsNear(team, at, AggroRange).Any())
                {
                    Stop(me);

                    return;
                }

                bot.Retreating = false;
            }

            if (Recall.IsRecalling(me))
            {
                if (EnemyChampionsNear(team, at, 8f).Any() ||
                    me.Status.LastDamagedAt > StatusEffects.Now - (long)(System.Diagnostics.Stopwatch.Frequency * 0.5))
                    Recall.Cancel(_room, me);
                else
                    return;
            }

            UsePotion(me, health);

            List<Player> enemies = EnemyChampionsNear(team, at, AggroRange).ToList();

            if (health < _skill.RetreatHealth || bot.Retreating)
            {
                bot.Retreating = health < 0.75f;

                if (bot.Retreating)
                {
                    if (enemies.Count > 0 && health < 0.18f)
                        TryFlashAway(me, enemies);

                    if (enemies.Count > 0 && health < 0.3f)
                        TrySummoner(me, SUMMONER_SPELL.SummonerHeal, me);
                    Player threat = enemies.OrderBy(e => Dist(at, e)).FirstOrDefault();

                    if (threat == null || Dist(at, threat) > 9f)
                    {
                        if (!atShop && Recall.Start(_room, me))
                        {
                            Stop(me);

                            return;
                        }
                    }

                    MoveTo(bot, Fountain(team), 0.6f);

                    return;
                }
            }

            if (enemies.Count > 0 && Fight(bot, enemies))
                return;

            if (Assist(bot) || Defend(bot) || JoinObjective(bot))
                return;

            if (_skill.Shops && !atShop && enemies.Count == 0 && me.Progress.Gold >= NextItemCost(bot) + 350 &&
                me.Progress.Gold >= 1100)
            {
                if (Recall.Start(_room, me))
                {
                    Stop(me);

                    return;
                }
            }

            // 6. The role's job.
            if (bot.Role == BotRole.Jungle && !Grouping())
            {
                Jungle(bot);

                return;
            }

            Lane(bot, Grouping() ? "mid" : bot.Lane);
        }

        // ---- Fighting ----

        private bool Fight(Bot bot, List<Player> enemies)
        {
            Player me = bot.Player;
            Vec3 at = Pos(me);
            int team = me.Info.TeamId;
            Player target =
                enemies.OrderBy(e => e.Info.Hp / (float)Math.Max(1, e.Info.MaxHp)).ThenBy(e => Dist(at, e)).First();
            float targetHealth = target.Info.Hp / (float)Math.Max(1, target.Info.MaxHp);
            float ours = Strength(team, at, 11f), theirs = Strength(3 - team, at, 12f);
            bool targetUnderTurret = EnemyTurretCovering(team, Pos(target)) != null;
            bool lethal = targetHealth < 0.25f;
            bool favorable = ours >= theirs * _skill.EngageRatio || lethal;

            if (targetUnderTurret && !(lethal && _skill.DivesLowTargets && ours > theirs))
                favorable = false;

            Turret aimedAtUs = EnemyTurretCovering(team, at);

            if (aimedAtUs != null && !(aimedAtUs.Target is Minion) &&
                !(lethal && _skill.DivesLowTargets && Grouping(3, at)))
            {
                MoveTo(bot, SafeSpot(team, bot.Lane, at), 0.3f);

                return true;
            }

            if (!favorable)
            {
                // Out of their reach: our side of the lane.
                if (Dist(at, target) < 8f)
                {
                    MoveTo(bot, SafeSpot(team, bot.Lane, at), 0.5f);

                    return true;
                }

                return false;
            }

            float distance = Dist(at, target);

            if (_skill.UsesSummoners && targetHealth < 0.22f && distance < 6f)
                TrySummoner(me, SUMMONER_SPELL.SummonerIgnite, target);

            if (CastSpells(bot, target, enemies.Count))
                return true;

            if (TryAttack(me, target, distance))
                return true;
            MoveTo(bot, Pos(target), 0.35f);

            return true;
        }

        private float Strength(int team, Vec3 at, float range) =>
            _room.Units.OfType<Player>()
                .Where(p => Progression.IsChampion(p) && p.Info.TeamId == team && p.Info.Hp > 0 &&
                            Dist(at, p) <= range && Visible(3 - team, p, team))
                .Sum(p => p.Info.Hp / (float)Math.Max(1, p.Info.MaxHp) + p.Progress.Level * 0.06f);

        private bool CastSpells(Bot bot, Player target, int enemyCount)
        {
            Player me = bot.Player;

            if (_random.NextDouble() > _skill.SpellChance)
                return false;
            float distance = Dist(Pos(me), target);
            var order = new List<int> { 1, 2, 3 };

            for (int i = order.Count - 1; i > 0; --i)
            {
                int j = _random.Next(i + 1);
                (order[i], order[j]) = (order[j], order[i]);
            }

            float targetHealth = target.Info.Hp / (float)Math.Max(1, target.Info.MaxHp);

            if (enemyCount >= 2 || targetHealth < 0.5f)
                order.Insert(0, 4);

            foreach (int skill in order)
            {
                if (!me.IsSkillReady(skill, 0.0))
                    continue;

                if (PacketHandler.CheckCast(me, skill, out SpellData spell, out _, out _) !=
                    SKILL_REJECT_REASON.SkillRejectNone)
                    continue;
                float reach = Math.Max(Math.Max(spell?.CastRangeUnits ?? 0f, spell?.CastRadiusUnits ?? 0f), 3.2f);

                if (distance > reach * 0.95f)
                    continue;
                Vec3 aim = Pos(target);

                if (_skill.AimError > 0f)
                    aim += new Vec3((float)(_random.NextDouble() * 2 - 1) * _skill.AimError, 0f,
                                    (float)(_random.NextDouble() * 2 - 1) * _skill.AimError);
                var packet = new C_SkillCast {
                    CasterId = me.Info.ObjectId,
                    TargetId = target.Info.ObjectId,
                    SkillId = skill,
                    TargetPos = aim.ToProtoVector3(),
                };

                if (PacketHandler.Cast(_room, me, packet))
                {
                    bot.LastSpellAt = _elapsed;

                    return true;
                }
            }

            return false;
        }

        private bool TryAttack(Player me, GameObject target, float distance)
        {
            ChampionCombatStats stats = ChampionCombatStats.For(me.Info.ChampType);
            float radius = target is MapUnit unit ? unit.Radius : 0f;

            if (distance > stats.AttackRange + radius + 0.3f)
                return false;
            PacketHandler.Cast(_room, me,
                               new C_SkillCast {
                                   CasterId = me.Info.ObjectId,
                                   TargetId = target.Info.ObjectId,
                                   SkillId = 0,
                                   IsBasicAttack = true,
                                   TargetPos = target.Info.Position,
                               });

            return true;
        }

        // ---- Summoner spells and items ----

        private int SummonerSlot(Player me, SUMMONER_SPELL spell)
        {
            for (int slot = 0; slot < 2; ++slot)
                if (me.SummonerSlots[slot]?.Spell == spell)
                    return slot;

            return -1;
        }

        private bool TrySummoner(Player me, SUMMONER_SPELL spell, GameObject target)
        {
            if (!_skill.UsesSummoners)
                return false;
            int slot = SummonerSlot(me, spell);

            if (slot < 0)
                return false;

            return SummonerSpells.Cast(_room, me, slot, target?.Info?.ObjectId ?? 0,
                                       target != null ? Pos(target) : Pos(me)) == SKILL_REJECT_REASON.SkillRejectNone;
        }

        private void TryFlashAway(Player me, List<Player> enemies)
        {
            if (!_skill.UsesSummoners)
                return;
            int slot = SummonerSlot(me, SUMMONER_SPELL.SummonerFlash);

            if (slot < 0)
                return;
            Vec3 at = Pos(me);
            Vec3 away =
                Vec3.Normalize(new Vec3(Fountain(me.Info.TeamId).X - at.X, 0f, Fountain(me.Info.TeamId).Z - at.Z));
            SummonerSpells.Cast(_room, me, slot, 0, at + away * 4f);
        }

        private void UsePotion(Player me, float health)
        {
            if (health > 0.5f || me.Status.LastDamagedAt == 0)
                return;

            for (int slot = 0; slot < Inventory.SlotCount; ++slot)
            {
                int id = me.Items.Slots[slot].ItemId;

                if (id == 2003 || id == 2031 || id == 2033)
                {
                    ItemEffects.Use(_room, me, slot, Pos(me), 0);

                    return;
                }
            }
        }

        private static Dictionary<string, int[]> Builds()
        {
            if (_builds != null)
                return _builds;
            var builds = new Dictionary<string, int[]>();
            try
            {
                string path = Path.Combine(AppContext.BaseDirectory, "Content", "RecommendedItems.json");
                using var document = JsonDocument.Parse(File.ReadAllText(path));

                foreach (JsonProperty champion in document.RootElement.EnumerateObject())
                {
                    if (champion.Value.ValueKind != JsonValueKind.Object)
                        continue;
                    var list = new List<int>();

                    foreach (string part in new[] { "starter", "core", "boots", "situational" })
                        if (champion.Value.TryGetProperty(part, out JsonElement items))
                        {
                            var ids = items.EnumerateArray().Select(e => e.GetInt32()).ToList();
                            // One pair of boots; the rest of the list as given.
                            list.AddRange(part == "boots" ? ids.Take(1) : ids);
                        }
                    builds[champion.Name] = list.ToArray();
                }
            }
            catch (Exception exception)
            {
                Console.WriteLine("[Bots] RecommendedItems.json unreadable: " + exception.Message);
            }

            return _builds = builds;
        }

        private int[] BuildOf(Player me) => Builds().TryGetValue(ChampionRegistry.Find(me.Info.ChampType)?.Key ?? "",
                                                                 out int[] build)
                                                ? build
                                                : Array.Empty<int>();

        private int NextItemCost(Bot bot)
        {
            int[] build = BuildOf(bot.Player);

            while (bot.BuildStep < build.Length && bot.Player.Items.Has(build[bot.BuildStep]))
                ++bot.BuildStep;

            if (bot.BuildStep >= build.Length)
                return int.MaxValue / 2;

            return ItemDatabase.Get(build[bot.BuildStep])?.PriceTotal ?? int.MaxValue / 2;
        }

        private void Shop(Bot bot)
        {
            Player me = bot.Player;
            int[] build = BuildOf(me);

            for (int guard = 0; guard < 8 && bot.BuildStep < build.Length; ++guard)
            {
                int wanted = build[bot.BuildStep];
                ItemData data = ItemDatabase.Get(wanted);

                if (data == null || (me.Items.Has(wanted) && !data.Consumable))
                {
                    ++bot.BuildStep;
                    continue;
                }

                if (data.Consumable && me.Items.Count(wanted) >= Math.Max(1, data.MaxStacks > 1 ? 2 : 1))
                {
                    ++bot.BuildStep;
                    continue;
                }

                if (me.Items.Buy(_room, me, wanted) == SKILL_REJECT_REASON.SkillRejectNone)
                {
                    if (data.Consumable)
                        ++bot.BuildStep;
                    continue;
                }

                if (!BuyComponent(me, data, 0))
                    break;
            }

            me.Items.CommitPurchases();
            me.Items.Dirty = true;
        }

        private bool BuyComponent(Player me, ItemData item, int depth)
        {
            if (depth > 3)
                return false;

            foreach (ItemData part in item.From.Select(ItemDatabase.Get)
                         .Where(p => p != null)
                         .OrderByDescending(p => p.PriceTotal))
            {
                if (me.Items.Has(part.Id))
                    continue;

                if (part.PriceTotal <= me.Progress.Gold &&
                    me.Items.Buy(_room, me, part.Id) == SKILL_REJECT_REASON.SkillRejectNone)
                    return true;

                if (BuyComponent(me, part, depth + 1))
                    return true;
            }

            return false;
        }

        private void SpendSkillPoints(Player me)
        {
            if (Progression.SkillPoints(me) <= 0)
                return;
            int[] order = SkillOrder.TryGetValue(me.Info.ChampType, out int[] o) ? o : new[] { 1, 2, 3 };
            bool spent = false;

            for (int guard = 0; guard < 6 && Progression.SkillPoints(me) > 0; ++guard)
            {
                bool any = Progression.LevelUpSkill(me, SkillType.RSpell);
                // The first three levels take one of each, then the max order.
                if (!any)
                    foreach (int skill in me.Progress.Level <= 3 ? order.OrderBy(s => me.Progress.Ranks[s - 1])
                                                                 : order.AsEnumerable())
                        if (Progression.LevelUpSkill(me, (SkillType)skill))
                        {
                            any = true;
                            break;
                        }
                if (!any)
                    break;
                spent = true;
            }

            if (spent)
                Progression.SendStats(_room, me);
        }

        // ---- Laning ----

        private void Lane(Bot bot, string lane)
        {
            Player me = bot.Player;
            Vec3 at = Pos(me);
            int team = me.Info.TeamId;
            float ad = CombatMath.AttackDamage(me) * 0.9f;

            // Support: stay with the bottom lane carry.
            if (bot.Role == BotRole.Support && !Grouping())
            {
                Bot carry = _bots.FirstOrDefault(b => b.Player.Info.TeamId == team && b.Role == BotRole.Bottom &&
                                                      b.Player.Info.Hp > 0);
                Player ally = carry?.Player ?? _room.Units.OfType<Player>().FirstOrDefault(
                                                   p => p.Session != null && p.Info.TeamId == team && p.Info.Hp > 0);

                if (ally != null && Dist(at, ally) > 4f)
                {
                    MoveTo(bot, Pos(ally) + Backward(team, 1.5f), 0.5f);

                    return;
                }
            }

            Turret danger = EnemyTurretCovering(team, at);

            if (danger != null && !(danger.Target is Minion) && !Grouping(3, at))
            {
                MoveTo(bot, SafeSpot(team, lane, at), 0.4f);

                return;
            }

            var minions =
                _room.Units.OfType<Minion>()
                    .Where(m => m.IsAlive && m.Info.TeamId != team && Dist(at, m) <= 9f && Visible(team, m, team))
                    .ToList();

            if (minions.Count > 0)
            {
                Minion lastHit = bot.Role == BotRole.Support
                                     ? null
                                     : minions.Where(m => m.Info.Hp <= ad).OrderBy(m => Dist(at, m)).FirstOrDefault();

                if (lastHit != null && _random.NextDouble() <= _skill.LastHitChance && SafeToStand(team, Pos(lastHit)))
                {
                    if (!TryAttack(me, lastHit, Dist(at, lastHit)))
                        MoveTo(bot, Pos(lastHit), 0.3f);

                    return;
                }

                Minion low = minions.Where(m => SafeToStand(team, Pos(m))).OrderBy(m => m.Info.Hp).FirstOrDefault();

                if (low != null && (bot.Role != BotRole.Support || Grouping()))
                {
                    bool wait = low.Info.Hp < ad * 2.2f && low.Info.Hp > ad;

                    if (!wait && TryAttack(me, low, Dist(at, low)))
                        return;

                    if (Dist(at, low) > ChampionCombatStats.For(me.Info.ChampType).AttackRange + 0.5f)
                    {
                        MoveTo(bot, Pos(low), 0.5f);

                        return;
                    }

                    Stop(me);

                    return;
                }
            }

            GameObject structure = NextStructure(team, lane, at);

            if (structure != null && Dist(at, structure) <= 12f)
            {
                Turret turret = structure as Turret;
                bool tanked = turret == null || turret.Target is Minion || Grouping(3, at);

                if (tanked && _room.Map.IsProtected(structure as MapUnit) == false)
                {
                    if (TryAttack(me, structure, Dist(at, structure)))
                        return;
                    MoveTo(bot, Pos(structure), 0.6f);

                    return;
                }
            }

            if (structure != null && TeamAdvantage(team))
            {
                Turret turret = structure as Turret;
                float distance = Dist(at, structure);
                bool tanked = turret == null || turret.Target is Minion || Grouping(3, Pos(structure));

                if (tanked && _room.Map.IsProtected(structure as MapUnit) == false)
                {
                    if (TryAttack(me, structure, distance))
                        return;
                    MoveTo(bot, Pos(structure), 0.6f);

                    return;
                }

                // Wait for the others just outside its reach.
                Vec3 toward = Vec3.Normalize(new Vec3(at.X - Pos(structure).X, 0f, at.Z - Pos(structure).Z));
                MoveTo(bot, Pos(structure) + toward * (TurretRange + 0.8f), 0.8f);

                return;
            }

            Minion front =
                _room.Units.OfType<Minion>()
                    .Where(m => m.IsAlive && m.Info.TeamId == team && Dist(LanePoint(team, lane, 0.5f), m) < 60f)
                    .OrderBy(m => DistXZ(Pos(m), EnemyBase(team)))
                    .FirstOrDefault();

            if (front != null && DistXZ(Pos(front), at) < 25f)
            {
                MoveTo(bot, Pos(front) + Backward(team, 1.8f), 0.7f);

                return;
            }

            MoveTo(bot, LaneHold(team, lane), 1.0f);
        }

        // ---- Jungle ----

        private void Jungle(Bot bot)
        {
            Player me = bot.Player;
            Vec3 at = Pos(me);
            int team = me.Info.TeamId;

            if (_elapsed >= bot.NextGankCheck)
            {
                bot.NextGankCheck = _elapsed + 4f;
                Player prey = _room.Units.OfType<Player>()
                                  .Where(p => Progression.IsChampion(p) && p.Info.TeamId != team && p.Info.Hp > 0 &&
                                              Visible(team, p, team) && p.Info.Hp < p.Info.MaxHp * 0.55f &&
                                              Dist(at, p) < 40f && EnemyTurretCovering(team, Pos(p)) == null &&
                                              _room.Units.OfType<Player>().Any(
                                                  a => Progression.IsChampion(a) && a.Info.TeamId == team && a != me &&
                                                       a.Info.Hp > 0 && Dist(Pos(a), p) < 12f))
                                  .OrderBy(p => Dist(at, p))
                                  .FirstOrDefault();

                if (prey != null)
                    bot.JungleTarget = prey.Info.ObjectId;
            }

            if (bot.JungleTarget != 0 && _room.FindObject(bot.JungleTarget) is Player gank && gank.Info.Hp > 0 &&
                Progression.IsChampion(gank))
            {
                if (Dist(at, gank) > 50f || EnemyTurretCovering(team, Pos(gank)) != null)
                    bot.JungleTarget = 0;
                else
                {
                    MoveTo(bot, Pos(gank), 0.5f);

                    return;
                }
            }
            else
                bot.JungleTarget = 0;

            JungleMonster monster =
                Monsters().Where(m => !m.IsEpic && m.Camp?.Side == team).OrderBy(m => Dist(at, m)).FirstOrDefault() ??
                Monsters().Where(m => !m.IsEpic).OrderBy(m => Dist(at, m)).FirstOrDefault(m => Dist(at, m) < 45f);

            if (monster == null)
            {
                Lane(bot, NearestLane(team, at));

                return;
            }

            AttackMonster(bot, monster);
        }

        private void AttackMonster(Bot bot, JungleMonster monster)
        {
            Player me = bot.Player;
            Vec3 at = Pos(me);

            JungleMonster target = Monsters()
                                       .Where(m => m.Camp == monster.Camp || Dist(Pos(m), monster) < 5f)
                                       .OrderBy(m => m.Info.Hp)
                                       .FirstOrDefault() ??
                                   monster;
            bool big = target.IsEpic || target.Camp != null && target.Info.ChampType == target.Camp.Monster;

            if (big && target.Info.Hp <= 600 && Dist(at, target) < 5.5f)
                TrySummoner(me, SUMMONER_SPELL.SummonerSmite, target);
            float distance = Dist(at, target);

            if (_random.NextDouble() < _skill.SpellChance * 0.5 && distance < 6f)
                foreach (int skill in new[] { 1, 2, 3 })
                {
                    if (!me.IsSkillReady(skill, 0.0) ||
                        PacketHandler.CheckCast(me, skill, out _, out _, out _) != SKILL_REJECT_REASON.SkillRejectNone)
                        continue;

                    if (PacketHandler.Cast(_room, me,
                                           new C_SkillCast { CasterId = me.Info.ObjectId,
                                                             TargetId = target.Info.ObjectId, SkillId = skill,
                                                             TargetPos = target.Info.Position }))
                        return;
                }
            if (TryAttack(me, target, distance))
                return;
            MoveTo(bot, Pos(target), 0.5f);
        }

        private IEnumerable<JungleMonster> Monsters() => _room.Units.OfType<JungleMonster>().Where(m => m.IsAlive &&
                                                                                                        !m.IsResetting);

        // ---- Team play ----

        private bool Grouping() => _room.Map.Elapsed >= GroupAfterSeconds
                                   || _room.Map.Turrets.Any(t => !t.IsAlive && t.Tier == 1);

        private bool Grouping(int count,
                              Vec3 at) => _room.Units.OfType<Player>().Count(p => Progression.IsChampion(p) &&
                                                                                  p.Info.Hp > 0 &&
                                                                                  Dist(at, p) < 9f) >= count;

        private bool TeamAdvantage(int team)
        {
            int ours = _room.Units.OfType<Player>().Count(p => Progression.IsChampion(p) && p.Info.TeamId == team &&
                                                               p.Info.Hp > 0);
            int theirs = _room.Units.OfType<Player>().Count(p => Progression.IsChampion(p) && p.Info.TeamId != team &&
                                                                 p.Info.Hp > 0);

            return ours - theirs >= 2 || Grouping() && ours >= theirs && ours >= 3;
        }

        private bool Assist(Bot bot)
        {
            Player me = bot.Player;
            Vec3 at = Pos(me);
            int team = me.Info.TeamId;
            Player ally = _room.Units.OfType<Player>()
                              .Where(p => Progression.IsChampion(p) && p.Info.TeamId == team && p != me &&
                                          p.Info.Hp > 0 && Dist(at, p) < AssistRange)
                              .FirstOrDefault(p => EnemyChampionsNear(team, Pos(p), 8f).Any());

            if (ally == null)
                return false;
            Player enemy = EnemyChampionsNear(team, Pos(ally), 8f).OrderBy(e => Dist(Pos(ally), e)).First();

            if (Strength(team, Pos(ally), 12f) + 0.5f < Strength(3 - team, Pos(ally), 12f) * _skill.EngageRatio)
                return false;
            MoveTo(bot, Pos(enemy), 0.4f);

            return true;
        }

        private bool Defend(Bot bot)
        {
            Player me = bot.Player;
            Vec3 at = Pos(me);
            int team = me.Info.TeamId;
            MapUnit attacked =
                _room.Map.Turrets.Cast<MapUnit>()
                    .Concat(_room.Map.Inhibitors)
                    .Concat(new[] { _room.Map.NexusOf(team) })
                    .Where(s => s != null && s.IsAlive && s.Info.TeamId == team && Dist(at, s) < DefendRange)
                    .FirstOrDefault(s => EnemyChampionsNear(team, Pos(s), 9f).Any());

            if (attacked == null)
                return false;
            MoveTo(bot, Pos(attacked), 0.6f);

            return true;
        }

        private bool JoinObjective(Bot bot)
        {
            Player me = bot.Player;
            int team = me.Info.TeamId;

            if (!_objectiveCallUntil.TryGetValue(team, out float until) || _elapsed > until ||
                !_objectiveTarget.TryGetValue(team, out ulong id) || !(_room.FindObject(id) is JungleMonster epic) ||
                !epic.IsAlive)
            {
                if (bot.Role != BotRole.Jungle)
                    return false;
                JungleMonster call = PickObjective(team);

                if (call == null)
                    return false;
                _objectiveTarget[team] = call.Info.ObjectId;
                _objectiveCallUntil[team] = _elapsed + 45f;
                epic = call;
                Console.WriteLine($"[Bots] Team {team} goes for {epic.Info.Name}.");
            }

            Vec3 at = Pos(me);

            if (bot.Role != BotRole.Jungle && Dist(at, epic) > 40f)
                return false;

            if (EnemyChampionsNear(team, Pos(epic), 10f).Any())
            {
                MoveTo(bot, Pos(epic), 0.5f);

                return true;
            }

            AttackMonster(bot, epic);

            return true;
        }

        private JungleMonster PickObjective(int team)
        {
            float time = _room.Map.Elapsed;
            int alive = _room.Units.OfType<Player>().Count(p => Progression.IsChampion(p) && p.Info.TeamId == team &&
                                                                p.Info.Hp > 0);
            int enemiesAlive = _room.Units.OfType<Player>().Count(p => Progression.IsChampion(p) &&
                                                                       p.Info.TeamId != team && p.Info.Hp > 0);

            foreach (JungleMonster epic in _room.Units.OfType<JungleMonster>().Where(m => m.IsAlive && m.IsEpic))
            {
                if ((epic.IsDragon || epic.IsElder) && time >= 300f && alive >= 3 && alive >= enemiesAlive - 1)
                    return epic;

                if (epic.IsHerald && alive >= 3)
                    return epic;

                if (epic.IsBaron && time >= 1200f && alive >= 4 && alive > enemiesAlive)
                    return epic;
            }

            return null;
        }

        // ---- Map knowledge ----

        private IEnumerable<Player> EnemyChampionsNear(int team, Vec3 at, float range) =>
            _room.Units.OfType<Player>().Where(p => Progression.IsChampion(p) && p.Info.TeamId != team &&
                                                    p.Info.Hp > 0 && Dist(at, p) <= range && Visible(team, p, team));

        private Turret EnemyTurretCovering(int team, Vec3 point) => _room.Map.Turrets.FirstOrDefault(
            t => t.IsAlive && t.Info.TeamId != team && DistXZ(point, Pos(t)) <= TurretRange);

        private bool SafeToStand(int team, Vec3 point)
        {
            Turret turret = _room.Map.Turrets.FirstOrDefault(t => t.IsAlive && t.Info.TeamId != team &&
                                                                  DistXZ(point, Pos(t)) <= TurretRange - 1f);

            return turret == null || turret.Target is Minion;
        }

        private GameObject NextStructure(int team, string lane, Vec3 at)
        {
            int enemy = 3 - team;

            foreach (int tier in new[] { 1, 2, 3 })
            {
                Turret turret = _room.Map.Turrets.FirstOrDefault(t => t.IsAlive && t.Info.TeamId == enemy &&
                                                                      t.Lane == lane && t.Tier == tier);

                if (turret != null)
                    return turret;
            }

            Structure inhibitor =
                _room.Map.Inhibitors.FirstOrDefault(i => i.IsAlive && i.Info.TeamId == enemy && i.Lane == lane);

            if (inhibitor != null && _room.Map.IsProtected(inhibitor) == false)
                return inhibitor;
            Turret nexusTurret = _room.Map.Turrets.Where(t => t.IsAlive && t.Info.TeamId == enemy && t.Tier == 4)
                                     .OrderBy(t => Dist(at, t))
                                     .FirstOrDefault();

            if (nexusTurret != null)
                return nexusTurret;
            Structure nexus = _room.Map.NexusOf(enemy);

            return nexus != null && nexus.IsAlive ? nexus : null;
        }

        private Vec3 LaneHold(int team, string lane)
        {
            Turret own =
                _room.Map.Turrets
                    .Where(t => t.IsAlive && t.Info.TeamId == team && t.Lane == lane && t.Tier >= 1 && t.Tier <= 3)
                    .OrderBy(t => t.Tier)
                    .FirstOrDefault();
            Vec3 anchor = own != null ? Pos(own) : LanePoint(team, lane, 0.2f);
            Vec3 toward = Vec3.Normalize(new Vec3(EnemyBase(team).X - anchor.X, 0f, EnemyBase(team).Z - anchor.Z));

            return anchor + toward * 3f;
        }

        private Vec3 SafeSpot(int team, string lane, Vec3 at)
        {
            Turret own = _room.Map.Turrets.Where(t => t.IsAlive && t.Info.TeamId == team)
                             .OrderBy(t => DistXZ(at, Pos(t)))
                             .FirstOrDefault();

            if (own != null && DistXZ(at, Pos(own)) < 30f)
                return Pos(own) + Backward(team, 1.5f);

            return at + Backward(team, 6f);
        }

        private string NearestLane(int team, Vec3 at) =>
            new[] { "top", "mid", "bot" }.OrderBy(l => DistXZ(at, LanePoint(team, l, 0.5f))).First();

        private Vec3 LanePoint(int team, string lane, float t)
        {
            IReadOnlyList<Vec3> path = _room.Layout.LanePath(team, lane);

            if (path.Count < 2)
                return Vec3.Lerp(Fountain(team), EnemyBase(team), t);
            float total = 0f;

            for (int i = 1; i < path.Count; ++i)
                total += Vec3.Distance(path[i - 1], path[i]);
            float goal = total * Math.Clamp(t, 0f, 1f);

            for (int i = 1; i < path.Count; ++i)
            {
                float leg = Vec3.Distance(path[i - 1], path[i]);

                if (goal <= leg)
                    return Vec3.Lerp(path[i - 1], path[i], leg > 0f ? goal / leg : 0f);
                goal -= leg;
            }

            return path[path.Count - 1];
        }

        private Vec3 Fountain(int team) => _room.Layout.Spawns.TryGetValue(team, out Vec3 spawn) ? spawn : Vec3.Zero;
        private Vec3 EnemyBase(int team) => _room.Map.NexusOf(3 - team)?.Info.Position.ToNumericsVector3() ??
                                            Fountain(3 - team);

        // A step back toward the team's own base.
        private Vec3 Backward(int team, float distance)
        {
            Vec3 direction = Fountain(team) - EnemyBase(team);
            direction.Y = 0f;

            return direction.LengthSquared() > 0f ? Vec3.Normalize(direction) * distance : Vec3.Zero;
        }

        private bool Visible(int viewerTeam, GameObject unit,
                             int ownTeam) => unit.Info.TeamId == ownTeam ||
                                             !_room.Vision.IsHiddenFrom(viewerTeam, unit.Info.ObjectId);

        // ---- Movement ----

        private void MoveTo(Bot bot, Vec3 destination, float repathSeconds)
        {
            Player me = bot.Player;

            if (me.IsMovementLockedByCombat)
                return;
            bool sameGoal = !float.IsNaN(bot.MoveGoal.X) && DistXZ(bot.MoveGoal, destination) < 1.2f &&
                            me.MovementState?.IsActive == true;

            if (sameGoal || _elapsed < bot.NextMove || _pathBudget <= 0)
                return;
            --_pathBudget;
            bot.NextMove = _elapsed + repathSeconds;
            bot.MoveGoal = destination;
            me.CancelBasicAttackWindup();

            if (AuthoritativeMoveService.TryStartServerMove(_room, me, destination))
                Recall.Cancel(_room, me);
        }

        private void Stop(Player me)
        {
            if (me.MovementState?.IsActive == true)
                _room.CancelPlayerMovement(me);
        }

        private static Vec3 Pos(GameObject unit) => unit.Info.Position.ToNumericsVector3();
        private static float Dist(Vec3 a, GameObject b) => DistXZ(a, Pos(b));

        private static float DistXZ(Vec3 a, Vec3 b)
        {
            float dx = a.X - b.X, dz = a.Z - b.Z;

            return MathF.Sqrt(dx * dx + dz * dz);
        }
    }
}
