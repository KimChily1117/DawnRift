using Google.Protobuf.Protocol;
using Server.Game.Map;
using Server.Game.Room;

namespace Server.Game.Objects
{
    public sealed class Structure : MapUnit
    {
        public const int InhibitorHealth = 4000;
        public const int NexusHealth = 5500;
        public const float InhibitorRespawnSeconds = 300f;

        public string Lane { get; private set; }
        public bool IsNexus => Info.ChampType == PLAYER_CHAMPION_TYPE.PlayerTypeNexus;
        public override bool IsStructure => true;

        public static Structure CreateInhibitor(GameRoom room, MapLayout.Structure spot)
        {
            var inhibitor = new Structure { Lane = spot.Lane };
            inhibitor.InitUnit(room, PLAYER_CHAMPION_TYPE.PlayerTypeInhibitor, OBJECT_TYPE.Structure, "Inhibitor",
                               spot.Team, spot.Position, InhibitorHealth);
            inhibitor.Radius = 2.14f;

            return inhibitor;
        }

        public static Structure CreateNexus(GameRoom room, MapLayout.Structure spot)
        {
            var nexus = new Structure { Lane = "mid" };
            nexus.InitUnit(room, PLAYER_CHAMPION_TYPE.PlayerTypeNexus, OBJECT_TYPE.Structure, "Nexus", spot.Team,
                           spot.Position, NexusHealth);
            nexus.Radius = 3.04f;

            return nexus;
        }

        public override void Think(float deltaTime) {}

        // The inhibitor stands again with full health.
        public void Restore()
        {
            Info.Hp = Info.MaxHp;
            Room.Broadcast(new S_Respawn {
                TargetId = Info.ObjectId,
                Position = Info.Position,
                Hp = Info.Hp,
                MaxHp = Info.MaxHp,
            });
        }
    }
}
