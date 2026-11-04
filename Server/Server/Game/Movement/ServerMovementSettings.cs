using System;

namespace Server.Game.Movement
{
    public enum MovementDeltaTimeStatus
    {
        Valid = 0,
        Invalid = 1,
        Clamped = 2,
    }

    public static class ServerMovementSettings
    {
        public const float DefaultMovementSpeed = 2.0f;
        public const float MaximumMovementSpeed = 50.0f;
        public const float SlowCellSpeedMultiplier = 0.5f;
        public const float MaximumDeltaTimeSeconds = 0.25f;
        public const float WaypointEpsilon = 0.0001f;

        public static float SanitizeDeltaTime(float deltaTime, out MovementDeltaTimeStatus status)
        {
            if (float.IsNaN(deltaTime) || float.IsInfinity(deltaTime) || deltaTime < 0.0f)
            {
                status = MovementDeltaTimeStatus.Invalid;

                return 0.0f;
            }

            if (deltaTime > MaximumDeltaTimeSeconds)
            {
                status = MovementDeltaTimeStatus.Clamped;

                return MaximumDeltaTimeSeconds;
            }

            status = MovementDeltaTimeStatus.Valid;

            return deltaTime;
        }

        public static bool IsValidSpeed(float speed)
        {
            return !float.IsNaN(speed) && !float.IsInfinity(speed) && speed >= 0.0f && speed <= MaximumMovementSpeed;
        }
    }
}
