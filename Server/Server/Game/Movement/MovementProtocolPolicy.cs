using System;

namespace Server.Game.Movement
{
    public static class MovementProtocolPolicy
    {
        public static bool AuthoritativeMoveRequestsEnabled => true;

        public static bool LegacyMoveEnabled =>
            string.Equals(Environment.GetEnvironmentVariable("D3D_ALLOW_LEGACY_MOVE"), "1", StringComparison.Ordinal);
    }
}
