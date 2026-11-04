using Google.Protobuf.Protocol;
using Server.Game.Map;
using Server.Game.Movement;
using Server.Game.Room;
using System;
using Vec3 = System.Numerics.Vector3;

namespace Server.Game.Objects
{
    public abstract class MapUnit : Player
    {
        public const float ChampionRadius = 0.65f;
        protected const double CallForHelpSeconds = 1.0;

        public float AttackRange { get; protected set; }
        public float AttackSpeed { get; protected set; } = 1f;
        public float WindupRatio { get; protected set; } = 0.3f;
        public float AttackDamageValue { get; protected set; }
        public float MissileSpeed { get; protected set; }

        public float Radius { get; protected set; } = 0.5f;
        public virtual bool IsStructure => false;

        public Func<bool> Protection { get; set; }
        public bool IsTargetable => Protection == null || !Protection();
        public MapDirector Director { get; set; }

        protected GameObject _target;
        protected float _repathTimer;

        public GameObject Target => _target;
        public bool IsAlive => Info != null && Info.Hp > 0;

        public static float RadiusOf(GameObject unit) => unit is MapUnit mapUnit ? mapUnit.Radius : ChampionRadius;

        protected void InitUnit(GameRoom room, PLAYER_CHAMPION_TYPE type, OBJECT_TYPE objectType, string name, int team,
                                Vec3 position, int maxHp)
        {
            ChampionData data = ChampionDatabase.Get(type);
            Info = new ObjectInfo {
                ObjectId = room.GenerateUnitId(),
                ChampType = type,
                State = OBJECT_STATE_TYPE.Idle,
                ObjType = objectType,
                Name = name,
                MaxHp = maxHp,
                Hp = maxHp,
                Position = position.ToProtoVector3(),
                TeamId = team,
                Level = 1,
            };
            Room = room;

            if (data != null)
            {
                AttackRange = data.AttackRangeUnits;
                AttackSpeed = data.AttackSpeed > 0f ? data.AttackSpeed : 1f;
                WindupRatio = data.AttackWindupPercent;
                AttackDamageValue = data.AttackDamage.AtLevel(1);
                MissileSpeed = data.Ranged ? data.BasicAttackMissileSpeed * ChampionData.UnitScale : 0f;

                if (data.MoveSpeedUnits > 0f)
                    TrySetMovementSpeed(data.MoveSpeedUnits);
                Info.Attack = (int)Math.Round(AttackDamageValue);
                Info.Defence = (int)Math.Round(data.Armor.AtLevel(1));
            }

            if (room.Navigation.TryWorldToCell(position, out var cell))
            {
                TileX = cell.X;
                TileZ = cell.Z;
            }
        }

        public abstract void Think(float deltaTime);

        // Damage of one basic attack against the target, and its type.
        public virtual (float amount, int type)
            DamageAgainst(GameObject target) => (AttackDamageValue, DamageTypes.Physical);

        // CombatMath: something damaged this unit.
        public virtual void OnDamaged(GameObject source) {}

        public virtual int OnKilled(GameObject source)
        {
            Room?.CancelPlayerMovement(this);

            return Director?.OnUnitKilled(this, source) ?? 0;
        }

        public float DistanceXZ(GameObject unit)
        {
            Vec3 delta = unit.Info.Position.ToNumericsVector3() - Info.Position.ToNumericsVector3();
            delta.Y = 0f;

            return delta.Length();
        }

        public bool InAttackRange(GameObject target,
                                  float slack = 0f) => DistanceXZ(target) <= AttackRange + RadiusOf(target) + slack;

        public bool IsEnemy(GameObject unit) => unit?.Info != null
                                                && unit.Info.Hp > 0 && unit.Info.TeamId != Info.TeamId &&
                                                !ReferenceEquals(unit, this) &&
                                                !(unit is MapUnit mapUnit && !mapUnit.IsTargetable) &&
                                                (Room?.Vision == null ||
                                                 !Room.Vision.IsHiddenFrom(Info.TeamId, unit.Info.ObjectId));

        public static Player ChampionOf(GameRoom room, GameObject source)
        {
            Player player = source as Player;

            if (player is Tibbers && player.Info != null)
                player = room.FindObject(player.Info.OwnerId) as Player;

            return player != null && Progression.IsChampion(player) ? player : null;
        }

        protected Player ChampionAttackingAllies(float range)
        {
            Player best = null;
            float bestDistance = range;

            foreach (GameObject unit in Room.GetObjectsInRange(Info.Position.ToNumericsVector3(),
                                                               range + ChampionRadius))
            {
                if (!(unit is Player champion) || !Progression.IsChampion(champion) || !IsEnemy(champion) ||
                    champion.LastHitTargetId == 0 ||
                    StatusEffects.Now - champion.LastHitAt >
                        (long)(CallForHelpSeconds * System.Diagnostics.Stopwatch.Frequency))
                    continue;
                GameObject victim = Room.FindObject(champion.LastHitTargetId);

                if (victim?.Info == null || victim.Info.TeamId != Info.TeamId || !Progression.IsChampion(victim))
                    continue;
                float distance = DistanceXZ(champion);

                if (distance <= bestDistance)
                {
                    best = champion;
                    bestDistance = distance;
                }
            }

            return best;
        }

        protected GameObject NearestEnemy(float range, Func<GameObject, bool> filter)
        {
            GameObject best = null;
            float bestDistance = float.MaxValue;

            foreach (GameObject unit in Room.GetObjectsInRange(Info.Position.ToNumericsVector3(), range + 3.5f))
            {
                if (!IsEnemy(unit) || !filter(unit))
                    continue;
                float distance = DistanceXZ(unit) - RadiusOf(unit);

                if (distance <= range && distance < bestDistance)
                {
                    best = unit;
                    bestDistance = distance;
                }
            }

            return best;
        }

        protected bool TryAttack(GameObject target)
        {
            double cycle = 1.0 / AttackSpeed;

            double windup = cycle * WindupRatio;

            if (!TryBeginBasicAttack(windup, cycle, 0.0))
                return false;
            Room.CancelPlayerMovement(this);
            Room.Broadcast(new S_SkillResult {
                CasterId = Info.ObjectId,
                SkillId = (int)SkillType.Gerneral,
                HitObjects = { target.Info.ObjectId },
            });
            int sequence = BasicAttackSequence;
            Room.PushAfter((int)Math.Round(windup * 1000.0), () => LandAttack(target, sequence));

            return true;
        }

        private void LandAttack(GameObject target, int sequence)
        {
            if (!IsAlive || BasicAttackSequence != sequence || Room == null || !IsEnemy(target) ||
                !InAttackRange(target, 1.0f))
                return;
            (float amount, int type) = DamageAgainst(target);

            if (MissileSpeed <= 0f)
            {
                CombatMath.ApplyDamage(Room, this, target, amount, type, (int)SkillType.Gerneral);

                return;
            }

            ulong projectileId = Room.GenerateProjectileId();
            Room.AddProjectile(
                new Projectile(projectileId, this, target, MissileSpeed, amount, type, (int)SkillType.Gerneral));
            Room.Broadcast(new S_ProjectileSpawn {
                ProjectileId = projectileId,
                CasterId = Info.ObjectId,
                TargetId = target.Info.ObjectId,
                StartPos = Info.Position,
                EndPos = target.Info.Position,
                Speed = MissileSpeed,
                SkillId = (int)SkillType.Gerneral,
            });
        }

        // Chase the target until in range, then attack it.
        protected void ChaseAndAttack(GameObject target, float repathSeconds)
        {
            if (InAttackRange(target))
            {
                if (MovementState != null && MovementState.IsActive)
                    Room.CancelPlayerMovement(this);

                if (!IsPerformingCombatAction)
                    TryAttack(target);

                return;
            }

            if (MovementSpeed <= 0f || IsInBasicAttackWindup || _repathTimer > 0f)
                return;
            _repathTimer = repathSeconds;
            AuthoritativeMoveService.TryStartServerMove(Room, this, ApproachPoint(target));
        }

        private const float SlotDegrees = 22f;

        public Vec3 ApproachPoint(GameObject target)
        {
            Vec3 center = target.Info.Position.ToNumericsVector3();
            Vec3 away = Info.Position.ToNumericsVector3() - center;
            away.Y = 0f;
            away = away.LengthSquared() > 0.0001f ? Vec3.Normalize(away) : Vec3.UnitX;
            float angle = ((int)(Info.ObjectId % 5) - 2) * SlotDegrees * MathF.PI / 180f;
            float cos = MathF.Cos(angle), sin = MathF.Sin(angle);
            Vec3 direction = new Vec3(away.X * cos - away.Z * sin, 0f, away.X * sin + away.Z * cos);
            float distance = RadiusOf(target) + AttackRange * 0.8f;

            return new Vec3(center.X + direction.X * distance, center.Y, center.Z + direction.Z * distance);
        }

        public static Vec3 Walkable(GameRoom room, Vec3 position)
        {
            var navigation = room.Navigation;

            if (navigation.TryWorldToCell(position.X, navigation.Origin.Y, position.Z, out var requested) &&
                AuthoritativeMoveService.TryResolveDestination(navigation, requested, out var cell, out _) &&
                navigation.TryGetCellWorldCenter(cell.X, cell.Z, out Vec3 center))
                return new Vec3(center.X, position.Y, center.Z);

            return position;
        }

        public void Spawn()
        {
            Room.AddObject(this);
            Room.Broadcast(new S_AddObject { Objects = { Info } });
        }

        public void Despawn()
        {
            Room?.CancelPlayerMovement(this);
            Room?.RemoveObject(Info.ObjectId);
        }
    }
}
