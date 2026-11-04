using System.Collections.Generic;
using Newtonsoft.Json;

namespace Server.Game.Navigation
{
    public sealed class NavigationMapCatalogConfig
    {
        [JsonProperty("navigationMaps")]
        public List<NavigationMapConfig> NavigationMaps { get; set; } = new List<NavigationMapConfig>();
    }

    public sealed class NavigationMapConfig
    {
        [JsonProperty("roomId")]
        public int RoomId { get; set; }

        [JsonProperty("sceneId")]
        public int SceneId { get; set; }

        [JsonProperty("navigationMapId")]
        public string NavigationMapId { get; set; }

        [JsonProperty("assetPath")]
        public string AssetPath { get; set; }

        [JsonProperty("formatVersion")]
        public uint FormatVersion { get; set; }

        [JsonProperty("contentHash")]
        public string ContentHashHex { get; set; }

        [JsonProperty("enabled")]
        public bool Enabled { get; set; } = true;

        [JsonProperty("required")]
        public bool Required { get; set; } = true;
    }
}
