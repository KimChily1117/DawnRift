using Google.Protobuf.Protocol;
using Server.Game.Objects;
using Server.Game.Room;
using System;
using Vec3 = System.Numerics.Vector3;

public class Projectile
{
    private readonly GameObject _caster;
    private readonly GameObject _target;
    private readonly float _speed;
    private readonly float _damage;
    private readonly int _damageType;
    private readonly int _skillId;
    private Vec3 _position;
    private bool _arrived;

    public ulong Id { get; private set; }

    public Action<GameObject> OnHit { get; set; }
    public bool Crit { get; set; }
    public int Dealt { get; private set; }
    public GameRoom Room { get; set; }
    public Vec3 Position => _position;
    public ulong CasterTeamId => (ulong)_caster.Info.TeamId;

    public Projectile(ulong id, GameObject caster, GameObject target, float speed, float damage, int damageType,
                      int skillId)
    {
        Id = id;
        _caster = caster ?? throw new ArgumentNullException(nameof(caster));
        _target = target ?? throw new ArgumentNullException(nameof(target));
        _speed = speed;
        _damage = damage;
        _damageType = damageType;
        _skillId = skillId;
        _position = caster.Info.Position.ToNumericsVector3();
    }

    public void Update(float deltaTime)
    {
        if (_arrived || _target.Info == null || _caster.Info == null || Room == null)
            return;

        if (deltaTime <= 0.0f || float.IsNaN(deltaTime) || float.IsInfinity(deltaTime) || _speed <= 0.0f)
            return;

        Vec3 targetPosition = _target.Info.Position.ToNumericsVector3();
        Vec3 toTarget = targetPosition - _position;
        float distance = toTarget.Length();

        if (float.IsNaN(distance) || float.IsInfinity(distance))
            return;

        float moveDistance = _speed * deltaTime;

        if (distance > 0.35f && moveDistance < distance)
        {
            _position += Vec3.Normalize(toTarget) * moveDistance;

            return;
        }

        _position = targetPosition;
        _arrived = true;
        Room.Push(ResolveHit);
    }

    private void ResolveHit()
    {
        if (_target.Info == null || _caster.Info == null || _target.Info.TeamId == _caster.Info.TeamId)
        {
            Room.RemoveProjectile(this);

            return;
        }

        Room.Broadcast(new S_ProjectileHit { ProjectileId = Id, TargetId = _target.Info.ObjectId });

        Dealt = CombatMath.ApplyDamage(Room, _caster, _target, _damage, _damageType, _skillId, crit: Crit);
        OnHit?.Invoke(_target);
        Room.RemoveProjectile(this);
    }
}
