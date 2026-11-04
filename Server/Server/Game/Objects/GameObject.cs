using System;
using System.Collections.Generic;
using System.Text;
using Google.Protobuf.Protocol;
using Server.Game.Room;

namespace Server.Game.Objects
{
    public class GameObject
    {
        public ObjectInfo Info { get; set; }
        public GameRoom Room { get; set; }

        public int TileX { get; set; }
        public int TileZ { get; set; }

        private Dictionary<int, float> _skillCooldowns = new Dictionary<int, float>();
        private float _globalCooldown = 0.5f;

        public bool CanUseSkill(int skillId)
        {
            float currentTime = DateTime.UtcNow.Ticks / TimeSpan.TicksPerMillisecond / 1000f;

            if (_skillCooldowns.TryGetValue(skillId, out float lastUsedTime))
            {
                if (currentTime - lastUsedTime < _globalCooldown)
                {
                    Console.WriteLine($"[Server] ❌ Skill {skillId} is on cooldown!");

                    return false;
                }
            }

            _skillCooldowns[skillId] = currentTime;

            return true;
        }
    }
}
