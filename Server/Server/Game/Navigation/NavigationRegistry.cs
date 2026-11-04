using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using Newtonsoft.Json;

namespace Server.Game.Navigation
{
    public sealed class NavigationRegistration
    {
        internal NavigationRegistration(NavigationMapConfig config, NavGridAsset asset)
        {
            RoomId = config.RoomId;
            SceneId = config.SceneId;
            NavigationMapId = config.NavigationMapId;
            AssetPath = config.AssetPath;
            Required = config.Required;
            Asset = asset;
        }

        public int RoomId { get; }
        public int SceneId { get; }
        public string NavigationMapId { get; }
        public string AssetPath { get; }
        public bool Required { get; }
        public NavGridAsset Asset { get; }
    }

    public sealed class NavigationRegistryLoadResult
    {
        private NavigationRegistryLoadResult(NavigationRegistry registry, string error)
        {
            Registry = registry;
            Error = error;
        }

        public NavigationRegistry Registry { get; }
        public string Error { get; }
        public bool Success => Registry != null;

        internal static NavigationRegistryLoadResult Ok(NavigationRegistry registry)
        {
            return new NavigationRegistryLoadResult(registry, null);
        }

        internal static NavigationRegistryLoadResult Fail(string error)
        {
            return new NavigationRegistryLoadResult(null, error);
        }
    }

    public sealed class NavigationRegistry
    {
        private readonly Dictionary<string, NavigationRegistration> _byMapId;
        private readonly Dictionary<int, NavigationRegistration> _byRoomId;
        private readonly IReadOnlyList<NavigationRegistration> _registrations;

        private NavigationRegistry(string contentRoot, Dictionary<string, NavigationRegistration> byMapId,
                                   Dictionary<int, NavigationRegistration> byRoomId,
                                   List<NavigationRegistration> registrations)
        {
            ContentRoot = contentRoot;
            _byMapId = byMapId;
            _byRoomId = byRoomId;
            _registrations = registrations.AsReadOnly();
        }

        public string ContentRoot { get; }
        public IReadOnlyList<NavigationRegistration> Registrations => _registrations;

        public bool TryGetByMapId(string navigationMapId, out NavGridAsset asset)
        {
            asset = null;

            if (navigationMapId == null ||
                !_byMapId.TryGetValue(navigationMapId, out NavigationRegistration registration))
                return false;
            asset = registration.Asset;

            return true;
        }

        public bool TryGetByRoomId(int roomId, out NavGridAsset asset)
        {
            asset = null;

            if (!_byRoomId.TryGetValue(roomId, out NavigationRegistration registration))
                return false;
            asset = registration.Asset;

            return true;
        }

        public bool TryGetRegistrationByRoomId(int roomId, out NavigationRegistration registration)
        {
            return _byRoomId.TryGetValue(roomId, out registration);
        }

        public NavGridAsset GetRequiredForRoom(int roomId)
        {
            if (!TryGetByRoomId(roomId, out NavGridAsset asset))
                throw new KeyNotFoundException("No Navigation Grid is registered for Room " + roomId + ".");

            return asset;
        }

        public static NavigationRegistryLoadResult LoadFromContent(
            string contentRoot, string configurationPath = NavigationContentPath.DefaultConfigurationPath,
            Action<string> log = null)
        {
            if (!NavigationContentPath.TryResolveAssetPath(contentRoot, configurationPath, out string fullConfigPath,
                                                           out string pathError))
                return NavigationRegistryLoadResult.Fail("Navigation configuration path is invalid: " + pathError);

            NavigationMapCatalogConfig catalog;
            try
            {
                if (!File.Exists(fullConfigPath))
                    return NavigationRegistryLoadResult.Fail("Navigation configuration file does not exist: " +
                                                             configurationPath);

                string json = File.ReadAllText(fullConfigPath);
                catalog = JsonConvert.DeserializeObject<NavigationMapCatalogConfig>(json);
            }
            catch (Exception exception) when (exception is IOException || exception is UnauthorizedAccessException ||
                                              exception is JsonException)
            {
                return NavigationRegistryLoadResult.Fail("Cannot load Navigation configuration: " + exception.Message);
            }

            if (catalog == null || catalog.NavigationMaps == null)
                return NavigationRegistryLoadResult.Fail(
                    "Navigation configuration must contain a navigationMaps array.");

            return Build(contentRoot, catalog.NavigationMaps, log);
        }

        public static NavigationRegistryLoadResult Build(string contentRoot,
                                                         IEnumerable<NavigationMapConfig> configurations,
                                                         Action<string> log = null)
        {
            if (configurations == null)
                return NavigationRegistryLoadResult.Fail("Navigation configuration collection is null.");

            if (!NavigationContentPath.TryResolveContentRoot(contentRoot, out string normalizedRoot,
                                                             out string rootError))
                return NavigationRegistryLoadResult.Fail(rootError);

            List<NavigationMapConfig> enabled =
                configurations.Where(config => config != null && config.Enabled).ToList();
            var mapIds = new HashSet<string>(StringComparer.Ordinal);
            var roomIds = new HashSet<int>();

            foreach (NavigationMapConfig config in enabled)
            {
                string validationError = ValidateConfiguration(config);

                if (validationError != null)
                    return NavigationRegistryLoadResult.Fail(validationError);

                if (!mapIds.Add(config.NavigationMapId))
                    return NavigationRegistryLoadResult.Fail("Duplicate NavigationMapId: " + config.NavigationMapId);

                if (!roomIds.Add(config.RoomId))
                    return NavigationRegistryLoadResult.Fail("Duplicate RoomId Navigation mapping: " + config.RoomId);
            }

            var byMapId = new Dictionary<string, NavigationRegistration>(StringComparer.Ordinal);
            var byRoomId = new Dictionary<int, NavigationRegistration>();
            var registrations = new List<NavigationRegistration>();

            foreach (NavigationMapConfig config in enabled)
            {
                if (!NavigationContentPath.TryResolveAssetPath(normalizedRoot, config.AssetPath,
                                                               out string fullAssetPath, out string pathError))
                {
                    string error = "Invalid assetPath for Room " + config.RoomId + ": " + pathError;

                    if (config.Required)
                        return NavigationRegistryLoadResult.Fail(error);
                    log?.Invoke("[Navigation] Optional mapping skipped: " + error);
                    continue;
                }

                NavGridLoadResult load = NavGridAssetLoader.Load(fullAssetPath);

                if (!load.Success)
                {
                    string error = "NavGrid load failed for Room " + config.RoomId + " (" + config.AssetPath +
                                   "): " + load.Error + " - " + load.Message;

                    if (config.Required)
                        return NavigationRegistryLoadResult.Fail(error);
                    log?.Invoke("[Navigation] Optional mapping skipped: " + error);
                    continue;
                }

                NavGridAsset asset = load.Asset;

                if (!string.Equals(asset.MapId, config.NavigationMapId, StringComparison.Ordinal))
                    return NavigationRegistryLoadResult.Fail("NavigationMapId mismatch for Room " + config.RoomId +
                                                             ": config=" + config.NavigationMapId +
                                                             ", file=" + asset.MapId);

                if (asset.FormatVersion != config.FormatVersion)
                    return NavigationRegistryLoadResult.Fail("FormatVersion mismatch for Room " + config.RoomId + ".");

                if (!string.Equals(asset.ContentHashHex, config.ContentHashHex, StringComparison.OrdinalIgnoreCase))
                    return NavigationRegistryLoadResult.Fail("ContentHash mismatch for Room " + config.RoomId +
                                                             ": expected=" + config.ContentHashHex +
                                                             ", actual=" + asset.ContentHashHex + ".");

                var registration = new NavigationRegistration(config, asset);
                byMapId.Add(registration.NavigationMapId, registration);
                byRoomId.Add(registration.RoomId, registration);
                registrations.Add(registration);
            }

            return NavigationRegistryLoadResult.Ok(
                new NavigationRegistry(normalizedRoot, byMapId, byRoomId, registrations));
        }

        private static string ValidateConfiguration(NavigationMapConfig config)
        {
            if (config.RoomId < 0)
                return "Navigation RoomId must be non-negative.";

            if (config.SceneId < 0)
                return "Navigation SceneId must be non-negative for Room " + config.RoomId + ".";

            if (string.IsNullOrWhiteSpace(config.NavigationMapId))
                return "NavigationMapId is required for Room " + config.RoomId + ".";

            if (string.IsNullOrWhiteSpace(config.AssetPath))
                return "assetPath is required for Room " + config.RoomId + ".";

            if (config.FormatVersion != NavGridAssetLoader.FormatVersion)
                return "Unsupported configured FormatVersion for Room " + config.RoomId + ".";

            if (!IsSha256Hex(config.ContentHashHex))
                return "contentHash must contain exactly 64 hexadecimal characters for Room " + config.RoomId + ".";

            return null;
        }

        private static bool IsSha256Hex(string value)
        {
            if (value == null || value.Length != 64)
                return false;

            foreach (char character in value)
            {
                bool digit = character >= '0' && character <= '9';
                bool lower = character >= 'a' && character <= 'f';
                bool upper = character >= 'A' && character <= 'F';

                if (!digit && !lower && !upper)
                    return false;
            }

            return true;
        }
    }
}
