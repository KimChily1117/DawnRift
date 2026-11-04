using Google.Protobuf.Protocol;
using Server.Game.Room;
using System;
using Vec3 = System.Numerics.Vector3;

namespace Server.Game.Objects
{
    public sealed class Ward : MapUnit
    {
        public const float SightRange = 9.0f;
        public const int Health = 3;

        public Player Owner { get; private set; }
        public long ExpiresAt { get; private set; }

        public static Ward Create(GameRoom room, Player owner, Vec3 position, double lifetimeSeconds)
        {
            var ward = new Ward { Owner = owner };
            ward.InitUnit(room, PLAYER_CHAMPION_TYPE.PlayerTypeWard, OBJECT_TYPE.Structure, "Ward", owner.Info.TeamId,
                          position, Health);
            ward.Info.OwnerId = owner.Info.ObjectId;
            ward.Radius = 0.35f;
            ward.ExpiresAt = StatusEffects.After(lifetimeSeconds);

            return ward;
        }

        public override void OnDamaged(GameObject source) {}

        public override int OnKilled(GameObject source)
        {
            Room?.PushAfter(600, Despawn);

            return 0;
        }

        public override void Think(float deltaTime)
        {
            if (IsAlive && StatusEffects.Now >= ExpiresAt)
                Expire();
        }

        public void ExpireAtForTests() => ExpiresAt = StatusEffects.Now - 1;

        public void Expire()
        {
            if (Info == null || Room == null)
                return;
            Info.Hp = 0;
            Room.Broadcast(new S_Dead { TargetId = Info.ObjectId });
            Despawn();
        }
    }
}
