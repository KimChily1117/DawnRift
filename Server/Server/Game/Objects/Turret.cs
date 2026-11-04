using Google.Protobuf.Protocol;
using Server.Game.Map;
using Server.Game.Room;
using System;
using System.Linq;
using Vec3 = System.Numerics.Vector3;

namespace Server.Game.Objects
{
    public sealed class Turret : MapUnit
    {
        private const float HeatPerShot = 0.4f;
        private const float MaxHeat = 1.2f;

        private static readonly int[] HealthByTier = { 5000, 3600, 3300, 2700 };

        public const float MeleeMinionShare = 0.45f, RangedMinionShare = 0.70f, SiegeMinionShare = 0.14f;

        public int Tier { get; private set; }
        public string Lane { get; private set; }

        public const int PlateCount = 5, PlateHealth = 800, PlateGold = 160;
        public const float PlateGoldRange = 10f;
        public int Plates { get; private set; }
        private float _heat;
        private ulong _heatTargetId;

        public static Turret Create(GameRoom room, MapLayout.Structure spot)
        {
            var turret = new Turret { Tier = spot.Tier, Lane = spot.Lane };
            turret.InitUnit(room, PLAYER_CHAMPION_TYPE.PlayerTypeTurret, OBJECT_TYPE.Tower, spot.Name ?? "Turret",
                            spot.Team, spot.Position, HealthByTier[Math.Clamp(spot.Tier, 1, 4) - 1]);
            turret.Radius = 0.884f;
            turret.Plates = spot.Tier == 1 ? PlateCount : 0;

            return turret;
        }

        public override bool IsStructure => true;

        public override void OnDamaged(GameObject source)
        {
            if (Plates <= 0 || Room == null || !(Room.Map?.PlatingActive ?? false))
                return;
            int baseHealth = Info.MaxHp - PlateCount * PlateHealth;
            int left = Math.Clamp((int)Math.Ceiling((Info.Hp - baseHealth) / (double)PlateHealth), 0, PlateCount);

            if (left >= Plates)
                return;
            int lost = Plates - left;
            Plates = left;

            foreach (Player champion in Room.GetObjectsInRange(Info.Position.ToNumericsVector3(), PlateGoldRange)
                         .OfType<Player>()
                         .Where(p => Progression.IsChampion(p) && p.Info.Hp > 0 && p.Info.TeamId != Info.TeamId))
                Progression.GrantGold(champion, PlateGold * lost);
            Buffs.State(Room, this, BUFF_TYPE.BuffTurretPlates, Buffs.PassiveSkillId, Plates, 3600.0);
        }

        public void DropPlates()
        {
            if (Plates <= 0)
                return;
            Plates = 0;

            if (Room != null && IsAlive)
                Buffs.State(Room, this, BUFF_TYPE.BuffTurretPlates, Buffs.PassiveSkillId, 0f, 0.0);
        }

        public static float MinionShare(GameObject minion) => minion.Info.ChampType switch {
            PLAYER_CHAMPION_TYPE.PlayerTypeMinionMelee => MeleeMinionShare,
            PLAYER_CHAMPION_TYPE.PlayerTypeMinionRanged => RangedMinionShare,
            _ => SiegeMinionShare,
        };

        public override (float amount, int type) DamageAgainst(GameObject target)
        {
            if (target is Minion)
                return (target.Info.MaxHp * MinionShare(target), DamageTypes.True);

            if (target.Info.ObjectId != _heatTargetId)
            {
                _heatTargetId = target.Info.ObjectId;
                _heat = 0f;
            }

            float amount = AttackDamageValue * (1f + _heat);
            _heat = Math.Min(MaxHeat, _heat + HeatPerShot);

            return (amount, DamageTypes.Physical);
        }

        public override void Think(float deltaTime)
        {
            if (!IsAlive || Room == null)
                return;
            _target = ChooseTarget();

            if (_target == null)
            {
                _heatTargetId = 0;

                return;
            }

            if (!IsPerformingCombatAction)
                TryAttack(_target);
        }

        private GameObject ChooseTarget()
        {
            Player helper = ChampionAttackingAllies(AttackRange);

            if (helper != null && InAttackRange(helper))
                return helper;

            if (IsEnemy(_target) && InAttackRange(_target))
                return _target;

            return NearestEnemy(AttackRange, unit => unit is Minion) ??
                   NearestEnemy(AttackRange, Progression.IsChampion) ??
                   NearestEnemy(AttackRange, unit => unit is Tibbers);
        }
    }
}
