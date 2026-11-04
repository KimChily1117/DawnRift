using Google.Protobuf.Protocol;
using Server.Game.Objects;
using Server.Game.Room;
using System;
using System.Collections.Generic;
using System.Linq;
using Vec3 = System.Numerics.Vector3;

namespace Server.Game.Map
{
    public sealed class MapDirector
    {
        public const float WaveIntervalSeconds = 30f;
        public const float MinionXpRange = 16f;
        public const int MaxMinions = 150;
        private const int CorpseMilliseconds = 1500;

        public float FirstWaveSeconds { get; set; } = 65f;

        public float PlatesFallSeconds { get; set; } = 840f;
        public bool PlatingActive => Started && Elapsed < PlatesFallSeconds;
        private bool _platesFallen;
        public float FirstCampSeconds { get; set; } = 90f;
        public float FirstDragonSeconds { get; set; } = 300f;
        public float DragonRespawnSeconds { get; set; } = 300f;
        public float FirstBaronSeconds { get; set; } = 1200f;
        public float BaronRespawnSeconds { get; set; } = 360f;

        public float FirstHeraldSeconds { get; set; } = 840f;
        public float HeraldLeavesSeconds { get; set; } = 1185f;
        public float ElderRespawnSeconds { get; set; } = 360f;
        public const double HeraldEyeSeconds = 240.0;
        public const float HeraldSummonRange = 6f;
        public const int SummonedHeraldHealth = 3000;

        public float EpicHealthScale { get; set; } = 1f;

        public EpicBuffs.Element? NextDragonElement { get; set; }

        public static readonly(int gold, float xp) DragonReward = (75, 150f);
        public static readonly(int gold, float xp) BaronReward = (300, 600f);
        public static readonly(int gold, float xp) ElderReward = (250, 600f);
        public static readonly(int gold, float xp) HeraldReward = (100, 300f);

        public static readonly Dictionary<PLAYER_CHAMPION_TYPE, (int gold, float xp)> MinionRewards =
            new Dictionary<PLAYER_CHAMPION_TYPE, (int, float)> {
                [PLAYER_CHAMPION_TYPE.PlayerTypeMinionMelee] = (21, 60f),
                [PLAYER_CHAMPION_TYPE.PlayerTypeMinionRanged] = (14, 30f),
                [PLAYER_CHAMPION_TYPE.PlayerTypeMinionSiege] = (60, 93f),
                [PLAYER_CHAMPION_TYPE.PlayerTypeMinionSuper] = (90, 97f),
                [PLAYER_CHAMPION_TYPE.PlayerTypeSruRiftherald] = (0, 0f),
            };

        public static readonly Dictionary<PLAYER_CHAMPION_TYPE, (int gold, float xp, float respawn)> CampRewards =
            new Dictionary<PLAYER_CHAMPION_TYPE, (int, float, float)> {
                [PLAYER_CHAMPION_TYPE.PlayerTypeSruBlue] = (100, 115f, 300f),
                [PLAYER_CHAMPION_TYPE.PlayerTypeSruRed] = (100, 115f, 300f),
                [PLAYER_CHAMPION_TYPE.PlayerTypeSruGromp] = (80, 135f, 135f),
                [PLAYER_CHAMPION_TYPE.PlayerTypeSruMurkwolf] = (85, 95f, 135f),
                [PLAYER_CHAMPION_TYPE.PlayerTypeSruRazorbeak] = (85, 95f, 135f),
                [PLAYER_CHAMPION_TYPE.PlayerTypeSruKrug] = (85, 155f, 135f),

                [PLAYER_CHAMPION_TYPE.PlayerTypeSruKrugMini] = (15, 20f, 135f),
                [PLAYER_CHAMPION_TYPE.PlayerTypeSruKrugMinimini] = (5, 7f, 135f),
                [PLAYER_CHAMPION_TYPE.PlayerTypeSruMurkwolfMini] = (16, 20f, 135f),
                [PLAYER_CHAMPION_TYPE.PlayerTypeSruRazorbeakMini] = (8, 10f, 135f),
            };

        public static readonly Dictionary<PLAYER_CHAMPION_TYPE, (PLAYER_CHAMPION_TYPE kind, Vec3 offset)[]> Companions =
            new Dictionary<PLAYER_CHAMPION_TYPE, (PLAYER_CHAMPION_TYPE, Vec3)[]> {
                [PLAYER_CHAMPION_TYPE.PlayerTypeSruKrug] =
                    new[] { (PLAYER_CHAMPION_TYPE.PlayerTypeSruKrugMini, new Vec3(1.6f, 0f, -0.9f)) },
                [PLAYER_CHAMPION_TYPE.PlayerTypeSruMurkwolf] =
                    new[] {
                        (PLAYER_CHAMPION_TYPE.PlayerTypeSruMurkwolfMini, new Vec3(1.4f, 0f, 0.8f)),
                        (PLAYER_CHAMPION_TYPE.PlayerTypeSruMurkwolfMini, new Vec3(-1.4f, 0f, 0.8f)),
                    },
                [PLAYER_CHAMPION_TYPE.PlayerTypeSruRazorbeak] =
                    new[] {
                        (PLAYER_CHAMPION_TYPE.PlayerTypeSruRazorbeakMini, new Vec3(1.2f, 0f, 0.6f)),
                        (PLAYER_CHAMPION_TYPE.PlayerTypeSruRazorbeakMini, new Vec3(-1.2f, 0f, 0.6f)),
                        (PLAYER_CHAMPION_TYPE.PlayerTypeSruRazorbeakMini, new Vec3(0.9f, 0f, -1.1f)),
                        (PLAYER_CHAMPION_TYPE.PlayerTypeSruRazorbeakMini, new Vec3(-0.9f, 0f, -1.1f)),
                        (PLAYER_CHAMPION_TYPE.PlayerTypeSruRazorbeakMini, new Vec3(0f, 0f, 1.5f)),
                    },
            };

        public static readonly int[] TurretGold = { 150, 175, 175, 50 };
        public const int InhibitorGold = 50;

        private readonly GameRoom _room;
        private readonly MapLayout _layout;
        private readonly List<MapUnit> _units = new List<MapUnit>();
        private readonly List<Turret> _turrets = new List<Turret>();
        private readonly List<Structure> _inhibitors = new List<Structure>();
        private readonly Dictionary<int, Structure> _nexuses = new Dictionary<int, Structure>();
        private readonly Dictionary<MapLayout.Camp, float> _campDueAt = new Dictionary<MapLayout.Camp, float>();
        private readonly Dictionary<Structure, float> _inhibitorDueAt = new Dictionary<Structure, float>();
        private float _nextWaveAt;

        private bool _welcomed, _minionsSoonSaid;
        private readonly HashSet<Structure> _inhibitorWarned = new HashSet<Structure>();
        public const float WelcomeSeconds = 1f;
        private float _dragonDueAt = float.PositiveInfinity, _baronDueAt = float.PositiveInfinity;
        private float _heraldDueAt = float.PositiveInfinity;
        private bool _elderNext;
        private readonly Random _random = new Random();

        private readonly Dictionary<MapUnit, float> _footprints = new Dictionary<MapUnit, float>();

        public const float TurretFootprint = 1.0f;

        public bool Started { get; private set; }
        public bool IsOver { get; private set; }
        public int WinningTeam { get; private set; }
        public bool Surrendered { get; private set; }
        public float Elapsed { get; private set; }
        public int WaveCount { get; private set; }
        public MapLayout Layout => _layout;
        public IReadOnlyList<MapUnit> Units => _units;
        public IEnumerable<Minion> Minions => _units.OfType<Minion>().Where(minion => minion.IsAlive);
        public IReadOnlyList<Turret> Turrets => _turrets;
        public IReadOnlyList<Structure> Inhibitors => _inhibitors;
        public JungleMonster Dragon =>
            _units.OfType<JungleMonster>().FirstOrDefault(monster => monster.IsDragon && monster.IsAlive);
        public JungleMonster Elder =>
            _units.OfType<JungleMonster>().FirstOrDefault(monster => monster.IsElder && monster.IsAlive);
        public JungleMonster Herald =>
            _units.OfType<JungleMonster>().FirstOrDefault(monster => monster.IsHerald && monster.IsAlive);
        public JungleMonster Baron =>
            _units.OfType<JungleMonster>().FirstOrDefault(monster => monster.IsBaron && monster.IsAlive);
        public EpicBuffs EpicBuffs => _room.EpicBuffs;
        public Structure NexusOf(int team) => _nexuses.TryGetValue(team, out Structure nexus) ? nexus : null;

        public MapDirector(GameRoom room, MapLayout layout)
        {
            _room = room ?? throw new ArgumentNullException(nameof(room));
            _layout = layout ?? throw new ArgumentNullException(nameof(layout));
        }

        public static MapDirector ForGame(GameRoom room, MapLayout layout)
        {
            string mode = Environment.GetEnvironmentVariable("DAWN_MAP")?.Trim().ToLowerInvariant();

            if (layout == null || mode == "off")
                return null;
            var director = new MapDirector(room, layout);

            if (mode == "fast")
            {
                director.FirstWaveSeconds = 5f;
                director.PlatesFallSeconds = 60f;
                director.FirstCampSeconds = 3f;
                director.FirstDragonSeconds = 8f;
                director.DragonRespawnSeconds = 30f;
                director.FirstBaronSeconds = 10f;
                director.BaronRespawnSeconds = 30f;
                director.ElderRespawnSeconds = 20f;

                director.Gallery = Environment.GetEnvironmentVariable("DAWN_GALLERY") == "1";

                if (Environment.GetEnvironmentVariable("DAWN_HERALD") == "1" || director.Gallery)
                {
                    director.FirstHeraldSeconds = 6f;
                    director.HeraldLeavesSeconds = 115f;
                    director.FirstBaronSeconds = 120f;
                }
                else
                    director.FirstHeraldSeconds = float.PositiveInfinity;
                director.EpicHealthScale = 0.1f;
            }

            Console.WriteLine(
                $"[Map] Waves from {director.FirstWaveSeconds}s, camps from {director.FirstCampSeconds}s, " +
                $"dragon from {director.FirstDragonSeconds}s, Baron from {director.FirstBaronSeconds}s.");

            return director;
        }

        public void Start()
        {
            if (Started)
                return;
            Started = true;
            IsOver = false;
            WinningTeam = 0;
            Elapsed = 0f;
            WaveCount = 0;
            _nextWaveAt = FirstWaveSeconds;

            foreach (MapLayout.Structure spot in _layout.Turrets)
                _turrets.Add(Place(Turret.Create(_room, spot)));

            foreach (MapLayout.Structure spot in _layout.Inhibitors)
                _inhibitors.Add(Place(Structure.CreateInhibitor(_room, spot)));

            foreach (MapLayout.Structure spot in _layout.Nexuses)
                _nexuses[spot.Team] = Place(Structure.CreateNexus(_room, spot));

            foreach (Turret turret in _turrets)
                turret.Protection = () => IsProtected(turret);

            foreach (Turret turret in _turrets)
                Block(turret, TurretFootprint);

            foreach (Structure structure in _inhibitors.Concat(_nexuses.Values))
                Block(structure, structure.Radius);

            foreach (Structure inhibitor in _inhibitors)
                inhibitor.Protection = () => IsProtected(inhibitor);

            foreach (Structure nexus in _nexuses.Values)
                nexus.Protection = () => IsProtected(nexus);

            foreach (MapLayout.Camp camp in _layout.Camps)
                _campDueAt[camp] = FirstCampSeconds;
            _dragonDueAt = _layout.Epics.ContainsKey("dragon") ? FirstDragonSeconds : float.PositiveInfinity;
            _baronDueAt = _layout.Epics.ContainsKey("baron") ? FirstBaronSeconds : float.PositiveInfinity;
            _heraldDueAt = _layout.Epics.ContainsKey("baron") && FirstHeraldSeconds < FirstBaronSeconds
                               ? FirstHeraldSeconds
                               : float.PositiveInfinity;
            _elderNext = Gallery;
            _galleryStep = 0;
            _room.EpicBuffs.Clear();
            Console.WriteLine($"[Map] Room {_room.RoomId}: {_turrets.Count} turrets, {_inhibitors.Count} inhibitors, " +
                              $"{_nexuses.Count} nexuses placed.");
        }

        // The last champion left: remove every map unit.
        public void Reset()
        {
            foreach (MapUnit structure in _footprints.Keys.ToList())
                Unblock(structure);

            foreach (MapUnit unit in _units)
                unit.Despawn();
            _units.Clear();
            _turrets.Clear();
            _inhibitors.Clear();
            _nexuses.Clear();
            _campDueAt.Clear();
            _inhibitorDueAt.Clear();
            _inhibitorWarned.Clear();
            _welcomed = _minionsSoonSaid = false;
            _dragonDueAt = _baronDueAt = _heraldDueAt = float.PositiveInfinity;
            _elderNext = false;
            _room.EpicBuffs.Clear();
            _room.Announcer.Reset();
            Started = false;
            IsOver = false;
            _platesFallen = false;
            Console.WriteLine($"[Map] Room {_room.RoomId}: map cleared.");
        }

        // ---- Gallery (DAWN_GALLERY=1 with DAWN_MAP=fast) ----

        public bool Gallery { get; private set; }
        private int _galleryStep;

        public static readonly float[] GalleryKills = { 20f, 24f, 28f, 32f, 80f, 84f, 88f };

        private void UpdateGallery()
        {
            while (_galleryStep < GalleryKills.Length && Elapsed >= GalleryKills[_galleryStep])
            {
                int step = _galleryStep++;
                Player killer = Champions(1).FirstOrDefault();

                if (killer == null)
                    continue;
                MapUnit target = step switch {
                    <
                        3 => _turrets.FirstOrDefault(t => t.IsAlive && t.Info.TeamId == 2 && t.Lane == "mid" &&
                                                          t.Tier == step + 1),
                    3 => _inhibitors.FirstOrDefault(i => i.IsAlive && i.Info.TeamId == 2 && i.Lane == "mid"),
                    <
                        6 => _turrets.Where(t => t.IsAlive && t.Info.TeamId == 2 && t.Tier == 4).FirstOrDefault(),
                    _ => NexusOf(2),
                };

                if (target == null || !target.IsAlive)
                    continue;
                Console.WriteLine($"[Map] Gallery: {target.Info.Name} falls at {Elapsed:0}s.");
                CombatMath.ApplyDamage(_room, killer, target, target.Info.Hp + 100000, DamageTypes.True,
                                       reflectable: false);
            }
        }

        public void Update(float deltaTime)
        {
            if (!Started || IsOver)
                return;
            Elapsed += deltaTime;

            if (Gallery)
                UpdateGallery();

            if (!_platesFallen && Elapsed >= PlatesFallSeconds)
            {
                _platesFallen = true;

                foreach (Turret turret in _turrets)
                    turret.DropPlates();
            }

            if (!_welcomed && Elapsed >= WelcomeSeconds)
            {
                _welcomed = true;
                _room.Announcer.Welcome();
            }

            if (!_minionsSoonSaid && FirstWaveSeconds - 30f > WelcomeSeconds + 3f && Elapsed >= FirstWaveSeconds - 30f)
            {
                _minionsSoonSaid = true;
                _room.Announcer.MinionsSoon();
            }

            if (Elapsed >= _nextWaveAt)
            {
                _nextWaveAt += WaveIntervalSeconds;
                SpawnWave();

                if (WaveCount == 1)
                    _room.Announcer.MinionsSpawned();
            }

            foreach (MapLayout.Camp camp in _campDueAt.Where(due => Elapsed >= due.Value)
                         .Select(due => due.Key)
                         .ToList())
            {
                _campDueAt.Remove(camp);
                Place(JungleMonster.Create(_room, this, camp));

                if (Companions.TryGetValue(camp.Monster, out var companions))
                    foreach (var (kind, offset) in companions)
                        Place(JungleMonster.Create(_room, this, camp, kind, offset));
            }

            if (Elapsed >= _heraldDueAt)
            {
                _heraldDueAt = float.PositiveInfinity;
                SpawnHerald();
            }

            if (Elapsed >= HeraldLeavesSeconds && Herald is JungleMonster leaving && !leaving.IsResettingOrFighting)
            {
                Console.WriteLine($"[Map] Room {_room.RoomId}: the Rift Herald left the pit.");
                leaving.Despawn();
            }

            if (Elapsed >= _dragonDueAt)
            {
                _dragonDueAt = float.PositiveInfinity;
                SpawnDragon();
            }

            if (Elapsed >= _baronDueAt)
            {
                _baronDueAt = float.PositiveInfinity;
                SpawnBaron();
            }

            foreach (Structure inhibitor in _inhibitorDueAt
                         .Where(due => Elapsed >= due.Value - Announcer.InhibitorWarningSeconds)
                         .Select(due => due.Key)
                         .Where(inhibitor => _inhibitorWarned.Add(inhibitor))
                         .ToList())
                _room.Announcer.OnInhibitorRespawnSoon(inhibitor);

            foreach (Structure inhibitor in _inhibitorDueAt.Where(due => Elapsed >= due.Value)
                         .Select(due => due.Key)
                         .ToList())
            {
                _inhibitorDueAt.Remove(inhibitor);
                _inhibitorWarned.Remove(inhibitor);
                inhibitor.Restore();
                _room.Announcer.OnInhibitorRespawned(inhibitor);
            }

            _units.RemoveAll(unit => unit.Room == null);
        }

        public void SpawnWave()
        {
            ++WaveCount;
            var kinds = new List<PLAYER_CHAMPION_TYPE> {
                PLAYER_CHAMPION_TYPE.PlayerTypeMinionMelee,
                PLAYER_CHAMPION_TYPE.PlayerTypeMinionMelee,
                PLAYER_CHAMPION_TYPE.PlayerTypeMinionMelee,
            };

            if (WaveCount % 3 == 0)
                kinds.Add(PLAYER_CHAMPION_TYPE.PlayerTypeMinionSiege);
            kinds.AddRange(Enumerable.Repeat(PLAYER_CHAMPION_TYPE.PlayerTypeMinionRanged, 3));

            int alive = Minions.Count();

            foreach (int team in new[] { 1, 2 })
            {
                Structure enemyNexus = NexusOf(3 - team);
                Vec3 enemyBase = enemyNexus?.Info.Position.ToNumericsVector3() ?? Vec3.Zero;

                foreach (string lane in MapLayout.LaneNames)
                {
                    if (!_layout.MinionSpawns.TryGetValue((team, lane), out Vec3 spawn))
                        continue;
                    IReadOnlyList<Vec3> path = _layout.LanePath(team, lane);

                    List<PLAYER_CHAMPION_TYPE> laneKinds = kinds;

                    if (_inhibitors.Any(inhibitor => inhibitor.Info.TeamId == 3 - team && inhibitor.Lane == lane &&
                                                     !inhibitor.IsAlive))
                    {
                        laneKinds = kinds.Where(kind => kind != PLAYER_CHAMPION_TYPE.PlayerTypeMinionSiege).ToList();
                        laneKinds.Insert(3, PLAYER_CHAMPION_TYPE.PlayerTypeMinionSuper);
                    }

                    Vec3 forward = path.Count > 0 ? path[Math.Min(1, path.Count - 1)] - spawn : Vec3.UnitX;

                    forward.Y = 0f;

                    forward = forward.LengthSquared() > 0.0001f ? Vec3.Normalize(forward) : Vec3.UnitX;

                    for (int i = 0; i < laneKinds.Count && alive < MaxMinions; ++i, ++alive)
                    {
                        int row = i / 3, file = i % 3;
                        Vec3 side = new Vec3(-forward.Z, 0f, forward.X) * ((file - 1) * 0.8f);
                        Vec3 position = spawn - forward * (row * 1.2f) + side;
                        Place(Minion.Create(_room, this, team, lane, laneKinds[i], position, file, path, enemyBase));
                    }
                }
            }
        }

        public JungleMonster SpawnDragon()
        {
            if (_elderNext)
            {
                JungleMonster elder =
                    SpawnEpic("dragon", "Elder Dragon", PLAYER_CHAMPION_TYPE.PlayerTypeSruDragonElder);
                Console.WriteLine($"[Map] Room {_room.RoomId}: the Elder Dragon spawned.");

                return elder;
            }

            EpicBuffs.Element element = NextDragonElement ?? (EpicBuffs.Element)_random.Next(4);
            NextDragonElement = null;
            JungleMonster dragon = SpawnEpic("dragon", element + " Drake", EpicBuffs.DragonOf(element));
            Console.WriteLine($"[Map] Room {_room.RoomId}: the {element} Drake spawned.");

            return dragon;
        }

        public JungleMonster SpawnHerald()
        {
            JungleMonster herald = SpawnEpic("baron", "Rift Herald", PLAYER_CHAMPION_TYPE.PlayerTypeSruRiftherald);
            Console.WriteLine($"[Map] Room {_room.RoomId}: the Rift Herald spawned.");

            return herald;
        }

        public bool SummonHerald(Player owner, Vec3 point)
        {
            if (owner?.Info == null || owner.HeraldEyeEndsAt <= StatusEffects.Now)
                return false;
            Vec3 at = owner.Info.Position.ToNumericsVector3();
            Vec3 delta = point - at;
            delta.Y = 0f;

            if (delta.Length() > HeraldSummonRange)
                point = at + delta / delta.Length() * HeraldSummonRange;
            int team = owner.Info.TeamId;
            string lane = MapLayout.LaneNames
                              .OrderBy(name => _layout.LanePath(team, name)
                                                   .Select(p => Vec3.Distance(Flat(p), Flat(point)))
                                                   .DefaultIfEmpty(float.MaxValue)
                                                   .Min())
                              .First();
            IReadOnlyList<Vec3> path = _layout.LanePath(team, lane);
            Structure enemyNexus = NexusOf(3 - team);
            Minion herald = Minion.Create(_room, this, team, lane, PLAYER_CHAMPION_TYPE.PlayerTypeSruRiftherald, point,
                                          1, path, enemyNexus?.Info.Position.ToNumericsVector3() ?? Vec3.Zero);
            herald.Info.Hp = herald.Info.MaxHp = SummonedHeraldHealth;
            Place(herald);
            owner.HeraldEyeEndsAt = 0;
            Buffs.State(_room, owner, BUFF_TYPE.BuffHeraldEye, Buffs.PassiveSkillId, 0f, 0.0);
            Console.WriteLine($"[Map] {owner.Info.ObjectId} summoned the Rift Herald in {lane}.");

            return true;
        }

        private static Vec3 Flat(Vec3 v) => new Vec3(v.X, 0f, v.Z);

        public JungleMonster SpawnBaron()
        {
            JungleMonster baron = SpawnEpic("baron", "Baron Nashor", PLAYER_CHAMPION_TYPE.PlayerTypeSruBaron);
            Console.WriteLine($"[Map] Room {_room.RoomId}: Baron Nashor spawned.");

            return baron;
        }

        private JungleMonster SpawnEpic(string pit, string name, PLAYER_CHAMPION_TYPE type)
        {
            var camp = new MapLayout.Camp { Name = name, Side = 0, Monster = type, Position = _layout.Epics[pit] };
            JungleMonster epic = JungleMonster.Create(_room, this, camp);

            if (EpicHealthScale != 1f)
                epic.Info.Hp = epic.Info.MaxHp = Math.Max(1, (int)Math.Round(epic.Info.MaxHp * EpicHealthScale));

            return Place(epic);
        }

        private int OnEpicKilled(JungleMonster epic, Player killer)
        {
            if (epic.IsDragon)
                _dragonDueAt = Elapsed + DragonRespawnSeconds;
            else if (epic.IsElder)
                _dragonDueAt = Elapsed + ElderRespawnSeconds;
            else if (epic.IsBaron)
                _baronDueAt = Elapsed + BaronRespawnSeconds;
            RemoveLater(epic);

            if (!epic.IsHerald)
                _room.Announcer.OnEpicKilled(epic, killer);

            if (killer == null)
                return 0;
            int team = killer.Info.TeamId;
            (int gold, float xp) = epic.IsDragon   ? DragonReward
                                   : epic.IsElder  ? ElderReward
                                   : epic.IsHerald ? HeraldReward
                                                   : BaronReward;

            foreach (Player champion in Champions(team))
            {
                Progression.GrantGold(champion, gold);
                Progression.GrantXp(_room, champion, xp);
            }

            if (epic.IsDragon)
            {
                EpicBuffs.Element element = EpicBuffs.ElementOf(epic.Info.ChampType);

                if (_room.EpicBuffs.AddDragon(team, element))
                    _room.EpicBuffs.SendAll(_room);
                Console.WriteLine(
                    $"[Map] Team {team} took the {element} Drake ({_room.EpicBuffs.Dragons(team)} dragons).");
                // A soul: from now on the pit spawns the Elder Dragon.
                if (_room.EpicBuffs.AnySoul)
                {
                    _elderNext = true;
                    Console.WriteLine(
                        $"[Map] Team {team} has the {_room.EpicBuffs.Soul(team)} soul; the Elder Dragon comes next.");
                }
            }
            else if (epic.IsElder)
            {
                foreach (Player champion in Champions(team).Where(champion => champion.Info.Hp > 0))
                {
                    champion.Status.ElderEndsAt = StatusEffects.After(EpicBuffs.ElderSeconds);
                    Buffs.State(_room, champion, BUFF_TYPE.BuffElder, Buffs.PassiveSkillId, 0f, EpicBuffs.ElderSeconds);
                }

                Console.WriteLine($"[Map] Team {team} slew the Elder Dragon.");
            }
            else if (epic.IsHerald)
            {
                killer.HeraldEyeEndsAt = StatusEffects.After(HeraldEyeSeconds);
                Buffs.State(_room, killer, BUFF_TYPE.BuffHeraldEye, Buffs.PassiveSkillId, 1f, HeraldEyeSeconds);
                Console.WriteLine($"[Map] {killer.Info.ObjectId} took the Eye of the Herald.");
            }
            else
            {
                foreach (Player champion in Champions(team).Where(champion => champion.Info.Hp > 0))
                    Buffs.HandOfBaron(_room, epic, champion, EpicBuffs.HandOfBaronSeconds);
                Console.WriteLine($"[Map] Team {team} slew Baron Nashor.");
            }

            return gold;
        }

        private T Place<T>(T unit)
            where T : MapUnit
        {
            unit.Director = this;
            _units.Add(unit);
            unit.Spawn();

            return unit;
        }

        public bool IsProtected(MapUnit structure)
        {
            int team = structure.Info.TeamId;

            switch (structure)
            {
            case Turret turret when turret.Tier <= 3:
                return _turrets.Any(other => other.IsAlive && other.Info.TeamId == team && other.Lane == turret.Lane &&
                                             other.Tier < turret.Tier);
            case Turret _:
                return _inhibitors.All(inhibitor => inhibitor.Info.TeamId != team || inhibitor.IsAlive);
            case Structure nexus when nexus.IsNexus:
                return _turrets.Any(turret => turret.IsAlive && turret.Info.TeamId == team && turret.Tier == 4);
            case Structure inhibitor:
                return _turrets.Any(turret => turret.IsAlive && turret.Info.TeamId == team &&
                                              turret.Lane == inhibitor.Lane && turret.Tier == 3);
            default:
                return false;
            }
        }

        public int OnUnitKilled(MapUnit unit, GameObject source)
        {
            Player killer = MapUnit.ChampionOf(_room, source);

            if (killer != null && killer.Info.TeamId == unit.Info.TeamId)
                killer = null;

            if (killer != null && (unit is Minion || unit is JungleMonster))
            {
                _room.Scoreboard.OnCreepKilled(killer);
                ChampionRegistry.Find(killer.Info.ChampType)?.OnKill?.Invoke(_room, killer, unit);
            }

            int gold = 0;

            switch (unit)
            {
            case Minion minion:
                (int minionGold, float xp) = MinionRewards[minion.Info.ChampType];

                if (killer != null)
                {
                    gold = minionGold;
                    Progression.GrantGold(killer, gold);
                }

                List<Player> sharing =
                    Champions(3 - minion.Info.TeamId)
                        .Where(champion => champion.Info.Hp > 0 && minion.DistanceXZ(champion) <= MinionXpRange)
                        .ToList();

                foreach (Player champion in sharing)
                    Progression.GrantXp(_room, champion, xp / sharing.Count);
                RemoveLater(minion);
                break;
            case JungleMonster epic when epic.IsEpic:
                gold = OnEpicKilled(epic, killer);
                break;
            case JungleMonster monster:
                (int campGold, float campXp, float respawn) = CampRewards[monster.Info.ChampType];

                if (killer != null)
                {
                    gold = campGold;
                    Progression.GrantGold(killer, gold);
                    Progression.GrantXp(_room, killer, campXp);
                }

                if (monster.Info.ChampType == PLAYER_CHAMPION_TYPE.PlayerTypeSruKrugMini)
                    foreach (float side in new[] { -0.6f, 0.6f })
                        Place(JungleMonster.Create(_room, this, monster.Camp,
                                                   PLAYER_CHAMPION_TYPE.PlayerTypeSruKrugMinimini,
                                                   monster.Info.Position.ToNumericsVector3() - monster.Camp.Position +
                                                       new Vec3(side, 0f, 0f)));
                // The camp comes back when its last monster is gone.
                if (!_units.OfType<JungleMonster>().Any(other => !ReferenceEquals(other, monster) && other.IsAlive &&
                                                                 other.Camp == monster.Camp))
                    _campDueAt[monster.Camp] = Elapsed + CampRewards[monster.Camp.Monster].respawn;
                RemoveLater(monster);
                break;
            case Turret turret:
                Unblock(turret);
                gold = TurretGold[Math.Clamp(turret.Tier, 1, 4) - 1];
                PayTeam(3 - turret.Info.TeamId, gold);
                RemoveLater(turret);
                _room.Announcer.OnTurretDestroyed(turret);
                break;
            case Structure nexus when nexus.IsNexus:
                EndGame(3 - nexus.Info.TeamId);
                break;
            case Structure inhibitor:
                gold = InhibitorGold;
                PayTeam(3 - inhibitor.Info.TeamId, gold);
                _inhibitorDueAt[inhibitor] = Elapsed + Structure.InhibitorRespawnSeconds;
                _room.Announcer.OnInhibitorDestroyed(inhibitor);
                break;
            }

            Console.WriteLine($"[Map] {unit.Info.Name} {unit.Info.ObjectId} (team {unit.Info.TeamId}) died" +
                              (killer != null ? $"; {killer.Info.ObjectId} +{gold} gold" : "") + ".");

            return gold;
        }

        public bool BlocksMovement(MapUnit structure) => _footprints.ContainsKey(structure);

        private void Block(MapUnit structure, float radius)
        {
            if (_footprints.ContainsKey(structure))
                return;
            _footprints[structure] = radius;
            _room.Navigation.AddBlocker(structure.Info.Position.ToNumericsVector3(), radius);
        }

        private void Unblock(MapUnit structure)
        {
            if (!_footprints.Remove(structure, out float radius))
                return;
            _room.Navigation.RemoveBlocker(structure.Info.Position.ToNumericsVector3(), radius);
        }

        private IEnumerable<Player> Champions(int team) =>
            _room.Units.OfType<Player>().Where(player => Progression.IsChampion(player) && player.Info?.TeamId == team);

        private void PayTeam(int team, int gold)
        {
            foreach (Player champion in Champions(team))
                Progression.GrantGold(champion, gold);
        }

        private void RemoveLater(MapUnit unit) => _room.PushAfter(CorpseMilliseconds, unit.Despawn);

        public void EndGame(int winningTeam, bool surrendered = false)
        {
            if (IsOver)
                return;
            IsOver = true;
            WinningTeam = winningTeam;
            Surrendered = surrendered;
            _room.Broadcast(new S_GameOver { WinningTeam = winningTeam });
            _room.Broadcast(_room.Scoreboard.BuildSummary(winningTeam, surrendered));
            Console.WriteLine($"[Map] Room {_room.RoomId}: team {winningTeam} destroyed the nexus.");
        }
    }
}
