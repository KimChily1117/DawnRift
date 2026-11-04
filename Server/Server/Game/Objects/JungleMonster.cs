using Google.Protobuf.Protocol;
using Server.Game.Map;
using Server.Game.Room;
using System;
using Vec3 = System.Numerics.Vector3;

namespace Server.Game.Objects
{
    public sealed class JungleMonster : MapUnit
    {
        public const int NeutralTeam = 0;
        public const float LeashRange = 8.0f;
        private const float HomeReach = 0.75f;
        private const float RepathSeconds = 0.4f;
        private const float DefaultMeleeRange = 1.5f;

        public const float EpicLeashRange = 10.0f;

        public MapLayout.Camp Camp { get; private set; }
        public bool IsResetting { get; private set; }
        public bool IsDragon => IsDragonType(Info.ChampType);
        public bool IsBaron => Info.ChampType == PLAYER_CHAMPION_TYPE.PlayerTypeSruBaron;
        public bool IsElder => Info.ChampType == PLAYER_CHAMPION_TYPE.PlayerTypeSruDragonElder;
        public bool IsHerald => Info.ChampType == PLAYER_CHAMPION_TYPE.PlayerTypeSruRiftherald;
        public bool IsEpic => IsDragon || IsBaron || IsElder || IsHerald;

        public bool IsCompanion => Info.ChampType >= PLAYER_CHAMPION_TYPE.PlayerTypeSruKrugMini &&
                                   Info.ChampType <= PLAYER_CHAMPION_TYPE.PlayerTypeSruRazorbeakMini;
        private float Leash => IsEpic ? EpicLeashRange : LeashRange;

        public static bool IsDragonType(PLAYER_CHAMPION_TYPE type) =>
            type >= PLAYER_CHAMPION_TYPE.PlayerTypeSruDragonFire && type <= PLAYER_CHAMPION_TYPE.PlayerTypeSruDragonAir;
        private Vec3 _home;

        public static JungleMonster Create(GameRoom room, MapDirector director, MapLayout.Camp camp,
                                           PLAYER_CHAMPION_TYPE? type = null, Vec3 offset = default)
        {
            PLAYER_CHAMPION_TYPE kind = type ?? camp.Monster;
            ChampionData data = ChampionDatabase.Get(kind);
            int hp = (int)Math.Round(data?.Hp.AtLevel(1) ?? 1500f);
            var monster = new JungleMonster { Camp = camp, Director = director };
            Vec3 home = Walkable(room, camp.Position + offset);
            monster.InitUnit(room, kind, OBJECT_TYPE.Monster, type == null ? camp.Name : camp.Name + " (small)",
                             NeutralTeam, home, hp);
            monster._home = home;
            monster.Radius = MonsterRadius(camp.Monster);

            if (monster.AttackRange <= 0f)
                monster.AttackRange = DefaultMeleeRange;

            if (monster.IsEpic)
                monster.MissileSpeed = 0f;

            return monster;
        }

        private static float MonsterRadius(PLAYER_CHAMPION_TYPE monster) => monster switch {
            PLAYER_CHAMPION_TYPE.PlayerTypeSruBlue => 1.2f,
            PLAYER_CHAMPION_TYPE.PlayerTypeSruRed => 1.2f,
            PLAYER_CHAMPION_TYPE.PlayerTypeSruGromp => 1.0f,
            PLAYER_CHAMPION_TYPE.PlayerTypeSruKrug => 0.95f,
            PLAYER_CHAMPION_TYPE.PlayerTypeSruBaron => 2.0f,
            PLAYER_CHAMPION_TYPE.PlayerTypeSruDragonElder => 1.6f,
            PLAYER_CHAMPION_TYPE.PlayerTypeSruRiftherald => 1.5f,
            PLAYER_CHAMPION_TYPE.PlayerTypeSruKrugMini => 0.6f,
            PLAYER_CHAMPION_TYPE.PlayerTypeSruKrugMinimini => 0.4f,
            PLAYER_CHAMPION_TYPE.PlayerTypeSruMurkwolfMini => 0.55f,
            PLAYER_CHAMPION_TYPE.PlayerTypeSruRazorbeakMini => 0.4f,
            _ when IsDragonType(monster) => 1.4f,
            _ => 0.8f,
        };

        public float DistanceFromHome => Vec3.Distance(Flat(Info.Position.ToNumericsVector3()), Flat(_home));

        public bool IsResettingOrFighting => IsResetting || _target != null;

        public override void OnDamaged(GameObject source)
        {
            if (IsResetting || source?.Info == null || source.Info.TeamId == NeutralTeam)
                return;

            if (!IsEnemy(_target))
                _target = source;
        }

        public override void Think(float deltaTime)
        {
            if (!IsAlive || Room == null || Status.IsDisabled)
                return;
            _repathTimer -= deltaTime;

            if (IsResetting)
            {
                ReturnHome();

                return;
            }

            if (_target == null)
                return;

            if (IsBaron)
            {
                if (!IsEnemy(_target) || !InAttackRange(_target))
                    _target = NearestEnemy(AttackRange, unit => !(unit is MapUnit mapUnit) || mapUnit is Minion);

                if (_target == null)
                    BeginReset();
                else
                    ChaseAndAttack(_target, RepathSeconds);

                return;
            }

            bool targetGone = !IsEnemy(_target);
            bool leashed = DistanceFromHome > Leash || Vec3.Distance(Flat(_target.Info.Position.ToNumericsVector3()),
                                                                     Flat(_home)) > Leash + AttackRange + 1f;

            if (targetGone || leashed)
            {
                BeginReset();

                return;
            }

            ChaseAndAttack(_target, RepathSeconds);
        }

        private void BeginReset()
        {
            _target = null;
            IsResetting = true;
            CancelBasicAttackWindup();
            Info.Hp = Info.MaxHp;
            Room.Broadcast(new S_HpUpdate { TargetId = Info.ObjectId, Hp = Info.Hp, MaxHp = Info.MaxHp });
            _repathTimer = 0f;
            ReturnHome();
        }

        private void ReturnHome()
        {
            if (DistanceFromHome <= HomeReach)
            {
                IsResetting = false;

                return;
            }

            bool moving = MovementState != null && MovementState.IsActive;

            if (moving || _repathTimer > 0f)
                return;
            _repathTimer = RepathSeconds;

            if (!Movement.AuthoritativeMoveService.TryStartServerMove(Room, this, _home))
            {
                // No path home: stay here and take this spot as home.
                _home = Info.Position.ToNumericsVector3();
                IsResetting = false;
            }
        }

        private static Vec3 Flat(Vec3 v) => new Vec3(v.X, 0f, v.Z);
    }
}
