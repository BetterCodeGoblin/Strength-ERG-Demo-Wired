// BridgeConfig.cs
// Loads pm5_slots.json from the directory containing PM5BleBridge.exe.
//
// Example pm5_slots.json (edit with your PM5 device names or leave name empty for auto-assign):
// {
//   "slots": [
//     { "channel": "Strength", "port": 6789, "pm5Name": "" },
//     { "channel": "Rowing",   "port": 6791, "pm5Name": "" },
//     { "channel": "Cycling",  "port": 6792, "pm5Name": "" }
//   ]
// }
//
// pm5Name can be a full advertised name ("PM5 123456") or a partial match ("123456").
// Leave empty to auto-assign devices in BLE discovery order.

using System.Text.Json;
using System.Text.Json.Serialization;

namespace PM5BleBridge;

internal class SlotEntry
{
    [JsonPropertyName("channel")]  public string Channel { get; set; } = "";
    [JsonPropertyName("port")]     public int    Port    { get; set; }
    [JsonPropertyName("pm5Name")]  public string Pm5Name { get; set; } = "";
}

internal class BridgeConfig
{
    [JsonPropertyName("slots")] public List<SlotEntry> Slots { get; set; } = [];

    public static BridgeConfig LoadOrDefault()
    {
        string configPath = Path.Combine(
            AppContext.BaseDirectory, "pm5_slots.json");

        if (File.Exists(configPath))
        {
            try
            {
                string json = File.ReadAllText(configPath);
                var cfg = JsonSerializer.Deserialize<BridgeConfig>(json);
                if (cfg?.Slots?.Count > 0)
                {
                    Console.WriteLine($"[Config] Loaded {configPath}");
                    foreach (var s in cfg.Slots)
                        Console.WriteLine($"         {s.Channel} -> port {s.Port}  pm5Name=\"{s.Pm5Name}\"");
                    return cfg;
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"[Config] Failed to parse pm5_slots.json: {ex.Message}. Using defaults.");
            }
        }
        else
        {
            Console.WriteLine($"[Config] {configPath} not found — using defaults.");
        }

        return new BridgeConfig
        {
            Slots =
            [
                new SlotEntry { Channel = "Strength", Port = 6789, Pm5Name = "" },
                new SlotEntry { Channel = "Rowing",   Port = 6791, Pm5Name = "" },
                new SlotEntry { Channel = "Cycling",  Port = 6792, Pm5Name = "" },
            ]
        };
    }
}
