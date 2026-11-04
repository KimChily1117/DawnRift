using Google.Protobuf.Protocol;
using Server.Game.Room;
using System;
using Vec3 = System.Numerics.Vector3;

namespace Server.Game.Objects
{
    public sealed class TrainingDummy : MapUnit
    {
        public const int Health = 1000;
        public const double RegenDelaySeconds = 6.0;
        public const float RegenPerSecond = 0.25f;
        private long _lastDamagedAt;
        private float _regenCarry;
        private Vec3 _home;

        public static TrainingDummy Create(GameRoom room, int team, Vec3 position)
        {
            var dummy = new TrainingDummy();
            Vec3 spot = Walkable(room, position);
            dummy._home = spot;
            dummy.InitUnit(room, PLAYER_CHAMPION_TYPE.PlayerTypeMinionMelee, OBJECT_TYPE.Minion, "TrainingDummy", team,
                           spot, Health);
            dummy.Radius = 0.48f;

            return dummy;
        }

        public override void OnDamaged(GameObject source) => _lastDamagedAt = StatusEffects.Now;

        public override void Think(float deltaTime)
        {
            if (!IsAlive || Room == null || Info.Hp >= Info.MaxHp)
                return;

            if (StatusEffects.Now - _lastDamagedAt < (long)(RegenDelaySeconds * System.Diagnostics.Stopwatch.Frequency))
                return;
            _regenCarry += Info.MaxHp * RegenPerSecond * deltaTime;

            if (_regenCarry >= 1f)
            {
                int heal = (int)_regenCarry;
                _regenCarry -= heal;
                Info.Hp = Math.Min(Info.MaxHp, Info.Hp + heal);
                Buffs.SendHp(Room, this);
            }
        }

        public override int OnKilled(GameObject source)
        {
            GameRoom room = Room;
            Vec3 home = _home;
            int team = Info.TeamId;
            room.CancelPlayerMovement(this);
            room.PushAfter(1500, Despawn);
            room.PushAfter(4000, () =>
                                 {
                                     if (room.Practice && room.Dummies.Remove(this))
                                     {
                                         TrainingDummy again = Create(room, team, home);
                                         again.Spawn();
                                         room.Dummies.Add(again);
                                     }
                                 });

            return 0;
        }
    }
}
