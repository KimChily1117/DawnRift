using System;
using System.Numerics;

namespace Server.Game.Navigation
{
    public sealed class NavigationBoundsValidationResult
    {
        private NavigationBoundsValidationResult(bool success, string message)
        {
            Success = success;
            Message = message;
        }

        public bool Success { get; }
        public string Message { get; }

        public static NavigationBoundsValidationResult Match(string message)
        {
            return new NavigationBoundsValidationResult(true, message);
        }

        public static NavigationBoundsValidationResult Mismatch(string message)
        {
            return new NavigationBoundsValidationResult(false, message);
        }
    }

    public static class NavigationBoundsValidator
    {
        private const float Epsilon = 0.0001f;

        public static NavigationBoundsValidationResult ValidateLegacyTilemap(NavGridAsset navigation,
                                                                             Vector2 tilemapSize)
        {
            if (navigation == null)
                return NavigationBoundsValidationResult.Mismatch("Navigation asset is null.");

            if (!IsFinite(tilemapSize.X) || !IsFinite(tilemapSize.Y) || tilemapSize.X <= 0.0f || tilemapSize.Y <= 0.0f)
                return NavigationBoundsValidationResult.Mismatch("Legacy Tilemap size is invalid.");

            float navMinX = navigation.Origin.X;
            float navMinZ = navigation.Origin.Z;
            float navMaxX = navMinX + navigation.Width * navigation.CellSize;
            float navMaxZ = navMinZ + navigation.Height * navigation.CellSize;
            const float tileMinX = 0.0f;
            const float tileMinZ = 0.0f;
            float tileMaxX = tilemapSize.X;
            float tileMaxZ = tilemapSize.Y;

            bool matches = NearlyEqual(navMinX, tileMinX) && NearlyEqual(navMinZ, tileMinZ) &&
                           NearlyEqual(navMaxX, tileMaxX) && NearlyEqual(navMaxZ, tileMaxZ);

            string detail = "Nav XZ=[" + navMinX + "," + navMaxX + ")x[" + navMinZ + "," + navMaxZ + ")" +
                            ", Tilemap XZ=[0," + tileMaxX + ")x[0," + tileMaxZ + ")";

            return matches ? NavigationBoundsValidationResult.Match(detail)
                           : NavigationBoundsValidationResult.Mismatch(detail);
        }

        private static bool NearlyEqual(float left, float right) => Math.Abs(left - right) <= Epsilon;
        private static bool IsFinite(float value) => !float.IsNaN(value) && !float.IsInfinity(value);
    }
}
