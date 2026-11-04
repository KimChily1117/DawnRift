using Google.Protobuf.Protocol;
using Server.Game.Map;
using Server.Game.Movement;
using Server.Game.Room;
using System;
using System.Collections.Generic;
using Vec3 = System.Numerics.Vector3;

namespace Server.Game.Objects
{
    public sealed class Minion : MapUnit
    {
        public const float AcquisitionRange = 7.0f;
        private const float KeepTargetRange = 9.0f;
        private const float WaypointReach = 2.0f;
        private const float FileSpacing = 0.8f;
        private const float RepathSeconds = 0.5f;

        public string Lane { get; private set; }

        public bool IsChargingHerald => Info?.ChampType == PLAYER_CHAMPION_TYPE.PlayerTypeSruRiftherald;
        public const float HeraldTurretDamage = 1500f;
        private bool _charged;
        public int WaypointIndex => _waypoint;
        private IReadOnlyList<Vec3> _path;
        private Vec3 _finalGoal;
        private int _waypoint;
        private int _movingTo = -1;
        private float _fileOffset;

        public static Minion Create(GameRoom room, MapDirector director, int team, string lane,
                                    PLAYER_CHAMPION_TYPE kind, Vec3 spawn, int file, IReadOnlyList<Vec3> path,
                                    Vec3 enemyNexus)
        {
            ChampionData data = ChampionDatabase.Get(kind);
            int hp = (int)Math.Round(data?.Hp.AtLevel(1) ?? 450f);
            var minion = new Minion {
                Lane = lane,
                Director = director,
                _path = path,
                _finalGoal = enemyNexus,
                _fileOffset = (file - 1) * FileSpacing,
            };
            minion.InitUnit(room, kind, OBJECT_TYPE.Minion, "Minion", team, Walkable(room, spawn), hp);
            minion.Radius = kind == PLAYER_CHAMPION_TYPE.PlayerTypeMinionSiege     ? 0.65f
                            : kind == PLAYER_CHAMPION_TYPE.PlayerTypeMinionSuper   ? 0.75f
                            : kind == PLAYER_CHAMPION_TYPE.PlayerTypeSruRiftherald ? 1.5f
                                                                                   : 0.48f;
            minion._waypoint = minion.FirstWaypoint();

            return minion;
        }

        private int FirstWaypoint()
        {
            Vec3 position = Info.Position.ToNumericsVector3();
            int nearest = 0;
            float best = float.MaxValue;

            for (int i = 0; i < _path.Count; ++i)
            {
                float distance = FlatDistance(position, _path[i]);

                if (distance < best)
                {
                    best = distance;
                    nearest = i;
                }
            }

            return best <= WaypointReach * 2f ? nearest + 1 : nearest;
        }

        public override void Think(float deltaTime)
        {
            if (!IsAlive || Room == null || Status.IsDisabled)
                return;
            _repathTimer -= deltaTime;
            _target = ChooseTarget();

            if (_target != null)
            {
                _movingTo = -1;
                ChaseAndAttack(_target, RepathSeconds);

                return;
            }

            FollowLane();
        }

        public override (float amount, int type) DamageAgainst(GameObject target)
        {
            if (IsChargingHerald && target is Turret && !_charged)
            {
                _charged = true;
                GameRoom room = Room;
                room?.PushAfter(400, () =>
                                     {
                                         if (IsAlive)
                                             CombatMath.ApplyDamage(room, null, this, Info.Hp + 1, DamageTypes.True);
                                     });

                return (HeraldTurretDamage, DamageTypes.True);
            }

            (float amount, int type) = base.DamageAgainst(target);

            return (EpicBuffs.IsEmpowered(this) ? amount * EpicBuffs.EmpoweredMinionDamage : amount, type);
        }

        private GameObject ChooseTarget()
        {
            if (IsChargingHerald)
                return NearestEnemy(AcquisitionRange, unit => unit is MapUnit mapUnit && mapUnit.IsStructure);
            Player helper = ChampionAttackingAllies(AcquisitionRange);

            if (helper != null)
                return helper;

            if (IsEnemy(_target) && !(_target is JungleMonster) &&
                DistanceXZ(_target) - RadiusOf(_target) <= KeepTargetRange)
                return _target;

            return NearestEnemy(AcquisitionRange, unit => unit is Minion) ??
                   NearestEnemy(AcquisitionRange, unit => unit is MapUnit mapUnit && mapUnit.IsStructure) ??
                   NearestEnemy(AcquisitionRange, unit => Progression.IsChampion(unit) || unit is Tibbers);
        }

        private void FollowLane()
        {
            Vec3 position = Info.Position.ToNumericsVector3();

            while (_waypoint < _path.Count && FlatDistance(position, Goal(_waypoint)) <= WaypointReach)
                ++_waypoint;
            bool moving = MovementState != null && MovementState.IsActive;

            if (moving && _movingTo == _waypoint)
                return;

            if (!moving && _movingTo == _waypoint && _waypoint < _path.Count)
                ++_waypoint;

            if (_repathTimer > 0f || IsInBasicAttackWindup)
                return;
            _repathTimer = RepathSeconds;

            if (AuthoritativeMoveService.TryStartServerMove(Room, this, Goal(_waypoint)))
                _movingTo = _waypoint;
        }

        private Vec3 Goal(int index)
        {
            if (index >= _path.Count)
                return _finalGoal;
            Vec3 point = _path[index];
            Vec3 from = index > 0 ? _path[index - 1] : point;
            Vec3 to = index + 1 < _path.Count ? _path[index + 1] : point;
            Vec3 along = to - from;
            along.Y = 0f;

            if (along.LengthSquared() < 0.0001f)
                return point;
            along = Vec3.Normalize(along);

            return point + new Vec3(-along.Z, 0f, along.X) * _fileOffset;
        }

        private static float FlatDistance(Vec3 a, Vec3 b)
        {
            float dx = a.X - b.X, dz = a.Z - b.Z;

            return MathF.Sqrt(dx * dx + dz * dz);
        }
    }
}
