using Google.Protobuf.Protocol;
using Server.Game.Movement;
using Server.Game.Objects;
using Server.Game.Room;
using System;
using System.Collections.Generic;
using System.Linq;
using Vec3 = System.Numerics.Vector3;

namespace Server.Game.Demo
{
    public sealed class DemoDirector
    {
        private static readonly(PLAYER_CHAMPION_TYPE Type, int Skin)[] BlueBots = {
            (PLAYER_CHAMPION_TYPE.PlayerTypeGaren, 4),
            (PLAYER_CHAMPION_TYPE.PlayerTypeAshe, 17),
            (PLAYER_CHAMPION_TYPE.PlayerTypeSona, 6),
            (PLAYER_CHAMPION_TYPE.PlayerTypeKatarina, 29),
        };
        private static readonly(PLAYER_CHAMPION_TYPE Type, int Skin)[] RedBots = {
            (PLAYER_CHAMPION_TYPE.PlayerTypeAnnie, 1), (PLAYER_CHAMPION_TYPE.PlayerTypeGraves, 5),
            (PLAYER_CHAMPION_TYPE.PlayerTypeAhri, 15), (PLAYER_CHAMPION_TYPE.PlayerTypeKatarina, 12),
            (PLAYER_CHAMPION_TYPE.PlayerTypeAshe, 8),
        };
        public const int BlueLevel = 16, RedLevel = 11;
        public const float BlueRespawnSeconds = 6f, RedRespawnSeconds = 75f;
        public const float LineUpSeconds = 6f;
        public const float ThinkInterval = 0.2f;
        public const float AggroRange = 11f, SpellRange = 7.5f, ObjectiveRange = 9f;

        public const float StructureHealthShare = 0.3f;

        private readonly GameRoom _room;
        private readonly List<Player> _bots = new List<Player>();
        private readonly Dictionary<ulong, float> _nextThink = new Dictionary<ulong, float>();
        private readonly Dictionary<ulong, float> _nextMove = new Dictionary<ulong, float>();
        private readonly Random _random = new Random(7);
        private float _elapsed;
        private bool _started;
        private float _startedAt;

        public bool Started => _started;
        public float SinceStart => _started ? _elapsed - _startedAt : 0f;

        private DemoDirector(GameRoom room) => _room = room;

        public static DemoDirector ForRoom(GameRoom room) => Environment.GetEnvironmentVariable("DAWN_DEMO") == "1"
                                                                 ? new DemoDirector(room)
                                                                 : null;

        public float RespawnSeconds(Player victim) => victim.Info.TeamId == 1 ? BlueRespawnSeconds : RedRespawnSeconds;

        public void Update(float deltaTime)
        {
            _elapsed += deltaTime;

            if (!_started)
            {
                if (_room.Layout == null || _room.Map == null || !_room.Map.Started)
                    return;

                if (!_room.Units.OfType<Player>().Any(p => p.Session != null && Progression.IsChampion(p)))
                    return;
                Start();

                return;
            }

            if (_room.Map.IsOver)
                return;

            foreach (Player champion in _room.Units.OfType<Player>().Where(Progression.IsChampion).ToList())
            {
                ulong id = champion.Info.ObjectId;

                if (_nextThink.TryGetValue(id, out float at) && _elapsed < at)
                    continue;
                _nextThink[id] = _elapsed + ThinkInterval;
                try
                {
                    Think(champion);
                }
                catch (Exception exception)
                {
                    Console.WriteLine($"[Demo] Think failed for {champion.Info.Name}: {exception.Message}");
                }
            }
        }

        // ---- Setup ----

        private void Start()
        {
            _started = true;
            _startedAt = _elapsed;

            foreach (var (type, skin) in BlueBots)
                _bots.Add(_room.SpawnBotChampion(type, skin, 1, ChampionRegistry.Find(type)?.Key ?? type.ToString()));

            foreach (var (type, skin) in RedBots)
                _bots.Add(_room.SpawnBotChampion(type, skin, 2, ChampionRegistry.Find(type)?.Key ?? type.ToString()));
            var blue =
                _room.Units.OfType<Player>().Where(p => Progression.IsChampion(p) && p.Info.TeamId == 1).ToList();
            var red = _room.Units.OfType<Player>().Where(p => Progression.IsChampion(p) && p.Info.TeamId == 2).ToList();

            foreach (MapUnit structure in _room.Map.Turrets.Cast<MapUnit>()
                         .Concat(_room.Map.Inhibitors)
                         .Concat(new[] { _room.Map.NexusOf(1), _room.Map.NexusOf(2) })
                         .Where(s => s?.Info != null))
            {
                float share = structure is Structure building && building.IsNexus ? StructureHealthShare * 0.5f
                                                                                  : StructureHealthShare;
                structure.Info.MaxHp = Math.Max(1, (int)(structure.Info.MaxHp * share));
                structure.Info.Hp = Math.Min(structure.Info.Hp, structure.Info.MaxHp);
                Buffs.SendHp(_room, structure);
            }

            LineUp(blue, 1, BlueLevel);
            LineUp(red, 2, RedLevel);
            Console.WriteLine(
                $"[Demo] 5v5 started: blue {string.Join(", ", blue.Select(p => p.Info.Name))} vs red {string.Join(", ", red.Select(p => p.Info.Name))}.");
        }

        private void LineUp(List<Player> team, int teamId, int level)
        {
            Vec3 spot = LanePoint(teamId == 1 ? 0.40f : 0.60f);
            Vec3 across = Vec3.Normalize(new Vec3(1f, 0f, -1f));

            for (int i = 0; i < team.Count; ++i)
            {
                Player champion = team[i];

                while (champion.Progress.Level < level)
                    Progression.GrantXp(_room, champion, Progression.XpToNext(champion.Progress.Level));
                champion.Info.Hp = champion.Info.MaxHp;
                champion.Progress.Mana = champion.Progress.MaxMana;
                champion.Info.Mp = (int)champion.Progress.Mana;
                champion.Progress.Dirty = true;
                Progression.SendStats(_room, champion);
                Buffs.SendHp(_room, champion);
                Vec3 place = spot + across * ((i - (team.Count - 1) * 0.5f) * 1.6f) + new Vec3(0f, 0f, 0f);
                place = MapUnit.Walkable(_room, new Vec3(place.X, champion.Info.Position.Y, place.Z));
                _room.TeleportPlayer(champion, place);
                _room.Broadcast(
                    new S_Recall { TargetId = champion.Info.ObjectId, State = 1, Position = champion.Info.Position });
            }
        }

        private Vec3 LanePoint(float t)
        {
            IReadOnlyList<Vec3> path = _room.Layout.LanePath(1, "mid");

            if (path.Count < 2)
                return Vec3.Lerp(_room.Layout.Spawns[1], _room.Layout.Spawns[2], t);
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

        // ---- Behaviour ----

        private void Think(Player me)
        {
            if (me.Info.Hp <= 0 || me.Status.IsDisabled || me.Status.IsDashing || me.IsPerformingCombatAction ||
                me.Info.State == OBJECT_STATE_TYPE.Skill)
                return;
            int team = me.Info.TeamId;
            Vec3 at = me.Info.Position.ToNumericsVector3();

            if (SinceStart < LineUpSeconds)
                return;

            Player enemy =
                _room.Units.OfType<Player>()
                    .Where(p => Progression.IsChampion(p) && p.Info.TeamId != team && p.Info.Hp > 0 && Visible(team, p))
                    .OrderBy(p => DistanceXZ(at, p))
                    .FirstOrDefault(p => DistanceXZ(at, p) <= AggroRange);

            if (enemy != null)
            {
                float distance = DistanceXZ(at, enemy);

                if (distance <= SpellRange && TryCastSpell(me, enemy, at))
                    return;

                if (TryAttack(me, enemy, distance))
                    return;
                MoveTo(me, enemy.Info.Position.ToNumericsVector3(), 0.4f);

                return;
            }

            GameObject objective = NearestEnemyMinion(team, at) ?? NextStructure(team, at);

            if (objective != null)
            {
                float distance = DistanceXZ(at, objective);

                if (distance <= ObjectiveRange || objective is Structure || objective is Turret)
                {
                    if (TryAttack(me, objective, distance))
                        return;
                    MoveTo(me, objective.Info.Position.ToNumericsVector3(), 0.6f);

                    return;
                }
            }

            MoveTo(me, LanePoint(team == 1 ? 0.95f : 0.05f), 1.0f);
        }

        private bool TryCastSpell(Player me, Player enemy, Vec3 at)
        {
            int nearbyEnemies =
                _room.Units.OfType<Player>().Count(p => Progression.IsChampion(p) && p.Info.TeamId != me.Info.TeamId &&
                                                        p.Info.Hp > 0 && DistanceXZ(at, p) <= SpellRange);
            var order = new List<int> { 1, 2, 3 };

            for (int i = order.Count - 1; i > 0; --i)
            {
                int j = _random.Next(i + 1);
                (order[i], order[j]) = (order[j], order[i]);
            }

            if (nearbyEnemies >= 2 || enemy.Info.Hp < enemy.Info.MaxHp * 0.45f)
                order.Insert(0, 4);

            foreach (int skill in order)
            {
                if (!me.IsSkillReady(skill, 0.0))
                    continue;

                if (PacketHandler.CheckCast(me, skill, out _, out _, out _) != SKILL_REJECT_REASON.SkillRejectNone)
                    continue;
                var packet = new C_SkillCast {
                    CasterId = me.Info.ObjectId,
                    TargetId = enemy.Info.ObjectId,
                    SkillId = skill,
                    TargetPos = enemy.Info.Position,
                };

                if (PacketHandler.Cast(_room, me, packet))
                    return true;
            }

            return false;
        }

        private bool TryAttack(Player me, GameObject target, float distance)
        {
            ChampionCombatStats stats = ChampionCombatStats.For(me.Info.ChampType);
            float radius = target is MapUnit unit ? unit.Radius : 0f;

            if (distance > stats.AttackRange + radius + 0.3f)
                return false;
            var packet = new C_SkillCast {
                CasterId = me.Info.ObjectId, TargetId = target.Info.ObjectId,  SkillId = 0,
                IsBasicAttack = true,        TargetPos = target.Info.Position,
            };
            PacketHandler.Cast(_room, me, packet);

            return true;
        }

        private void MoveTo(Player me, Vec3 destination, float repathSeconds)
        {
            ulong id = me.Info.ObjectId;

            if (_nextMove.TryGetValue(id, out float at) && _elapsed < at)
                return;
            _nextMove[id] = _elapsed + repathSeconds;

            if (AuthoritativeMoveService.TryStartServerMove(_room, me, destination))
                Recall.Cancel(_room, me);
        }

        private GameObject NearestEnemyMinion(int team,
                                              Vec3 at) => _room.Units.OfType<Minion>()
                                                              .Where(m => m.Info.TeamId != team && m.IsAlive &&
                                                                          DistanceXZ(at, m) <= ObjectiveRange &&
                                                                          Visible(team, m))
                                                              .OrderBy(m => DistanceXZ(at, m))
                                                              .FirstOrDefault();

        private GameObject NextStructure(int team, Vec3 at)
        {
            var map = _room.Map;
            int enemy = 3 - team;

            foreach (int tier in new[] { 1, 2, 3 })
            {
                Turret turret = map.Turrets.FirstOrDefault(t => t.IsAlive && t.Info.TeamId == enemy &&
                                                                t.Lane == "mid" && t.Tier == tier);

                if (turret != null)
                    return turret;
            }

            Structure inhibitor =
                map.Inhibitors.FirstOrDefault(i => i.IsAlive && i.Info.TeamId == enemy && i.Lane == "mid");

            if (inhibitor != null)
                return inhibitor;
            Turret nexusTurret = map.Turrets.Where(t => t.IsAlive && t.Info.TeamId == enemy && t.Tier == 4)
                                     .OrderBy(t => DistanceXZ(at, t))
                                     .FirstOrDefault();

            if (nexusTurret != null)
                return nexusTurret;
            Structure nexus = map.NexusOf(enemy);

            return nexus != null && nexus.IsAlive ? nexus : null;
        }

        private bool Visible(int team, GameObject unit) => !_room.Vision.IsHiddenFrom(team, unit.Info.ObjectId);

        private static float DistanceXZ(Vec3 a, GameObject b)
        {
            float dx = a.X - b.Info.Position.X, dz = a.Z - b.Info.Position.Z;

            return MathF.Sqrt(dx * dx + dz * dz);
        }
    }
}
