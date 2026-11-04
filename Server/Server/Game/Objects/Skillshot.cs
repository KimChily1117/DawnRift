using Google.Protobuf.Protocol;
using Server.Game.Objects;
using Server.Game.Room;
using System;
using System.Collections.Generic;
using Vec3 = System.Numerics.Vector3;

public sealed class Skillshot
{
    public ulong Id { get; }
    public GameObject Caster { get; }
    public GameRoom Room { get; set; }
    public int SkillId { get; }
    public int Variant { get; set; }
    public Vec3 Position { get; private set; }
    public Vec3 Direction { get; private set; }
    public float Speed { get; set; }
    public float Acceleration { get; set; }
    public float MaxSpeed { get; set; } = float.MaxValue;
    public float Range { get; set; }
    public float Width { get; set; }
    public bool StopAtFirstHit { get; set; }
    public bool ChampionsOnly { get; set; }

    public bool StopAtWalls { get; set; }
    public bool HitWall { get; private set; }

    public GameObject Homing { get; set; }
    public float Traveled { get; private set; }
    public bool Ended { get; private set; }

    public Action<Skillshot, GameObject> OnHit { get; set; }
    // When it ends for any reason, after the clients were told.
    public Action<Skillshot> OnEnd { get; set; }

    public Func<GameObject, bool> CanHit { get; set; }

    private readonly HashSet<ulong> _hit = new HashSet<ulong>();
    private ulong _lastHitId;

    public Skillshot(ulong id, GameObject caster, int skillId, Vec3 start, Vec3 direction, float speed)
    {
        Id = id;
        Caster = caster ?? throw new ArgumentNullException(nameof(caster));
        SkillId = skillId;
        Position = new Vec3(start.X, start.Y, start.Z);
        direction.Y = 0f;
        Direction = direction.LengthSquared() > 1e-6f ? Vec3.Normalize(direction) : Vec3.UnitX;
        Speed = speed;
    }

    public Vec3 EndPoint => Homing?.Info != null ? Homing.Info.Position.ToNumericsVector3()
                                                 : Position + Direction * Math.Max(0f, Range - Traveled);

    public void Announce()
    {
        Room?.Broadcast(new S_ProjectileSpawn {
            ProjectileId = Id,
            CasterId = Caster.Info.ObjectId,
            TargetId = Homing?.Info?.ObjectId ?? 0,
            StartPos = Position.ToProtoVector3(),
            EndPos = EndPoint.ToProtoVector3(),
            Speed = Homing != null ? Math.Min(MaxSpeed, Math.Max(Speed, (Speed + MaxSpeed) * 0.5f)) : Speed,
            SkillId = SkillId,
            Variant = Variant,
        });
    }

    public void Update(float deltaTime)
    {
        if (Ended || Room == null || Caster.Info == null || deltaTime <= 0f || float.IsNaN(deltaTime))
            return;
        Speed = Math.Min(MaxSpeed, Speed + Acceleration * deltaTime);
        float step = Speed * deltaTime;
        Vec3 from = Position, to;
        bool arrived = false;

        if (Homing != null)
        {
            if (Homing.Info == null)
            {
                End();

                return;
            }

            Vec3 toTarget = Homing.Info.Position.ToNumericsVector3() - Position;
            toTarget.Y = 0f;
            float distance = toTarget.Length();

            if (distance <= step || distance < 0.35f)
            {
                to = Position + toTarget;
                arrived = true;
            }
            else
            {
                Direction = toTarget / distance;
                to = Position + Direction * step;
            }
        }
        else
        {
            step = Math.Min(step, Range - Traveled);
            to = Position + Direction * step;
            arrived = Traveled + step >= Range - 1e-4f;
        }

        if (StopAtWalls && Room.Navigation != null)
        {
            float length = Vec3.Distance(from, to);

            for (float d = 0.25f; d <= length + 1e-3f; d += 0.25f)
            {
                Vec3 point = from + (to - from) * Math.Min(1f, d / Math.Max(1e-4f, length));

                if (!Room.Navigation.TryWorldToCell(point, out var cell) || !Room.Navigation.IsWalkable(cell.X, cell.Z))
                {
                    to = from + (to - from) * Math.Max(0f, (d - 0.25f) / Math.Max(1e-4f, length));
                    HitWall = true;
                    arrived = true;
                    break;
                }
            }
        }

        Traveled += Vec3.Distance(from, to);
        Position = to;

        if (CheckHits(from, to))
            return;

        if (arrived)
            End();
    }

    private bool CheckHits(Vec3 from, Vec3 to)
    {
        Vec3 middle = (from + to) * 0.5f;
        float reach = Vec3.Distance(from, to) * 0.5f + Width + 3f;
        var candidates = new List<(GameObject Unit, float Along)>();

        foreach (GameObject unit in Room.GetObjectsInRange(middle, reach))
        {
            if (unit?.Info == null || _hit.Contains(unit.Info.ObjectId) || !(CanHit ?? DefaultCanHit)(unit))
                continue;
            Vec3 point = unit.Info.Position.ToNumericsVector3();
            float along = ClosestAlong(from, to, point, out float distance);

            if (distance <= Width + RadiusOf(unit))
                candidates.Add((unit, along));
        }

        candidates.Sort((a, b) => a.Along.CompareTo(b.Along));

        foreach (var (unit, _) in candidates)
        {
            _hit.Add(unit.Info.ObjectId);
            _lastHitId = unit.Info.ObjectId;
            OnHit?.Invoke(this, unit);

            if (StopAtFirstHit)
            {
                Position = unit.Info.Position.ToNumericsVector3();
                End();

                return true;
            }
        }

        return false;
    }

    private bool DefaultCanHit(GameObject unit) => unit.Info.Hp > 0 && unit.Info.TeamId != Caster.Info.TeamId &&
                                                   !(unit is Ward) &&
                                                   (!ChampionsOnly || Progression.IsChampion(unit)) &&
                                                   !(unit is MapUnit structure && structure.IsStructure);

    public static float RadiusOf(GameObject unit) => unit is MapUnit mapUnit ? mapUnit.Radius : 0.65f;

    private static float ClosestAlong(Vec3 a, Vec3 b, Vec3 p, out float distance)
    {
        Vec3 ab = b - a, ap = p - a;
        ab.Y = ap.Y = 0f;
        float length = ab.LengthSquared();
        float t = length > 1e-8f ? Math.Clamp(Vec3.Dot(ap, ab) / length, 0f, 1f) : 0f;
        Vec3 closest = a + ab * t;
        Vec3 d = p - closest;
        d.Y = 0f;
        distance = d.Length();

        return t;
    }

    public void End()
    {
        if (Ended)
            return;
        Ended = true;
        Room?.Broadcast(new S_ProjectileHit { ProjectileId = Id, TargetId = _lastHitId });
        OnEnd?.Invoke(this);
        Room?.RemoveSkillshot(this);
    }
}
