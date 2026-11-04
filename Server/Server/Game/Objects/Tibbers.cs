using Google.Protobuf.Protocol;
using Server.Game.Movement;
using Server.Game.Room;
using System;
using System.Diagnostics;
using Vec3 = System.Numerics.Vector3;

namespace Server.Game.Objects
{
    public sealed class Tibbers : Player
    {
        private const float AuraRadius = 3.5f;
        private const float AggroRange = 6.0f;
        private const float FollowDistance = 2.5f;
        private const float RepathSeconds = 0.4f;
        private const float RangeSlack = 0.25f;
        private const double OwnerTargetMemorySeconds = 3.0;

        private Player _owner;
        private long _expiresAt;
        private float _auraDamage, _attackDamage;
        private float _auraCarry, _repathTimer;
        private GameObject _target;
        private bool _dead;

        public static Tibbers Spawn(GameRoom room, Player owner, Vec3 position, int rank)
        {
            ChampionData data = ChampionDatabase.Get(PLAYER_CHAMPION_TYPE.PlayerTypeTibbers);
            SpellData r = CombatMath.DataOf(owner)?.Spell(SkillType.RSpell) ?? SpellData.Empty;
            NavGridCoordinateCenter(room, ref position);
            int hp = (int)Math.Round(data?.Hp.AtLevel(rank) ?? 850f);
            var tibbers = new Tibbers {
                Info =
                    new ObjectInfo {
                        ObjectId = room.GenerateUnitId(),
                        ChampType = PLAYER_CHAMPION_TYPE.PlayerTypeTibbers,
                        State = OBJECT_STATE_TYPE.Idle,
                        ObjType = OBJECT_TYPE.Minion,
                        Name = "Tibbers",
                        MaxHp = hp,
                        Hp = hp,
                        Position = position.ToProtoVector3(),
                        TeamId = owner.Info.TeamId,
                        OwnerId = owner.Info.ObjectId,
                    },
                Room = room,
                _owner = owner,
                _expiresAt = StatusEffects.After(r.Value("TibbersLifetime", rank, 45f)),
                _auraDamage = r.Value("AuraDamage", rank, 8f),
                _attackDamage = r.Value("TibbersAttackDamage", rank, 30f),
            };
            tibbers.Info.Attack = (int)tibbers._attackDamage;
            tibbers.TrySetMovementSpeed(data?.MoveSpeedUnits ?? 3.5f);

            if (room.Navigation.TryWorldToCell(position, out var cell))
            {
                tibbers.TileX = cell.X;
                tibbers.TileZ = cell.Z;
            }

            room.AddObject(tibbers);
            room.Broadcast(new S_AddObject { Objects = { tibbers.Info } });

            return tibbers;
        }

        // Summons land on the nearest walkable cell.
        private static void NavGridCoordinateCenter(GameRoom room, ref Vec3 position)
        {
            var navigation = room.Navigation;

            if (navigation.TryWorldToCell(position.X, navigation.Origin.Y, position.Z, out var requested) &&
                AuthoritativeMoveService.TryResolveDestination(navigation, requested, out var cell, out _) &&
                navigation.TryGetCellWorldCenter(cell.X, cell.Z, out Vec3 center))
            {
                position.X = center.X;
                position.Z = center.Z;
            }
        }

        public bool IsAlive => !_dead;

        public void Think(float deltaTime)
        {
            if (_dead || Room == null)
                return;

            if (Info.Hp <= 0 || StatusEffects.Now >= _expiresAt || _owner.Info == null ||
                !ReferenceEquals(Room.FindObject(_owner.Info.ObjectId), _owner))
            {
                Despawn();

                return;
            }

            _auraCarry += deltaTime;

            while (_auraCarry >= 1f)
            {
                _auraCarry -= 1f;

                foreach (GameObject unit in Room.GetObjectsInRange(Info.Position.ToNumericsVector3(), AuraRadius))
                    if (IsEnemy(unit))
                        CombatMath.ApplyDamage(Room, this, unit, _auraDamage, DamageTypes.Magic);
            }

            if (Status.IsDisabled)
                return;
            _target = ChooseTarget();
            _repathTimer -= deltaTime;

            if (_target == null)
            {
                FollowOwner();

                return;
            }

            ChampionCombatStats stats = ChampionCombatStats.For(Info.ChampType);
            float distance = DistanceXZ(_target);

            if (distance > stats.AttackRange + RangeSlack)
            {
                if (_repathTimer <= 0f && !IsInBasicAttackWindup)
                {
                    _repathTimer = RepathSeconds;
                    AuthoritativeMoveService.TryStartServerMove(Room, this, _target.Info.Position.ToNumericsVector3());
                }

                return;
            }

            Attack(stats);
        }

        private void Attack(ChampionCombatStats stats)
        {
            if (!TryBeginBasicAttack(stats.WindupSeconds, stats.AttackCycleSeconds, 0.0))
                return;
            Room.CancelPlayerMovement(this);
            GameObject target = _target;
            Room.Broadcast(new S_SkillResult { CasterId = Info.ObjectId, SkillId = (int)SkillType.Gerneral,
                                               HitObjects = { target.Info.ObjectId } });
            int sequence = BasicAttackSequence;
            Room.PushAfter((int)Math.Round(stats.WindupSeconds * 1000.0),
                           () =>
                           {
                               if (_dead || BasicAttackSequence != sequence || !IsEnemy(target) ||
                                   DistanceXZ(target) > stats.AttackRange + RangeSlack + 0.75f)
                                   return;
                               CombatMath.ApplyDamage(Room, this, target, _attackDamage, DamageTypes.Magic,
                                                      (int)SkillType.Gerneral);
                           });
        }

        private GameObject ChooseTarget()
        {
            // Annie's most recent target, while she keeps fighting it.
            if (_owner.LastHitTargetId != 0 &&
                StatusEffects.Now - _owner.LastHitAt < (long)(OwnerTargetMemorySeconds * Stopwatch.Frequency))
            {
                GameObject ownerTarget = Room.FindObject(_owner.LastHitTargetId);

                if (IsEnemy(ownerTarget) && DistanceXZ(ownerTarget) <= AggroRange * 2f)
                    return ownerTarget;
            }

            if (IsEnemy(_target) && DistanceXZ(_target) <= AggroRange * 1.5f)
                return _target;
            GameObject nearest = null;
            float best = AggroRange;

            foreach (GameObject unit in Room.GetObjectsInRange(Info.Position.ToNumericsVector3(), AggroRange))
            {
                float distance = DistanceXZ(unit);

                if (IsEnemy(unit) && distance <= best)
                {
                    nearest = unit;
                    best = distance;
                }
            }

            return nearest;
        }

        private void FollowOwner()
        {
            if (DistanceXZ(_owner) <= FollowDistance || _repathTimer > 0f)
                return;
            _repathTimer = RepathSeconds;
            AuthoritativeMoveService.TryStartServerMove(Room, this, _owner.Info.Position.ToNumericsVector3());
        }

        private void Despawn()
        {
            _dead = true;
            Room.CancelPlayerMovement(this);
            Room.RemoveObject(Info.ObjectId);
        }

        private bool IsEnemy(GameObject unit) => unit?.Info != null
                                                 && unit.Info.Hp > 0 && unit.Info.TeamId != Info.TeamId &&
                                                 !ReferenceEquals(unit, this) &&
                                                 !(unit is MapUnit structure && !structure.IsTargetable) &&
                                                 !Room.Vision.IsHiddenFrom(Info.TeamId, unit.Info.ObjectId);

        private float DistanceXZ(GameObject unit)
        {
            Vec3 delta = unit.Info.Position.ToNumericsVector3() - Info.Position.ToNumericsVector3();
            delta.Y = 0f;

            return delta.Length();
        }
    }
}
