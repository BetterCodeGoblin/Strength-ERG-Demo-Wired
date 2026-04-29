// BridgeConfig.cs
// Loads pm5_slots.json from the directory containing PM5BleBridge.exe.
//
// pm5_slots.json fields:
//   requirePinnedNames  bool   (default false)
//     When true, startup fails if any slot has an empty pm5Name.
//     Set this once you have run --list-devices and filled in all names.
//   slots[].pm5Name     string (default "")
//     Partial or full advertised BLE device name, e.g. "PM5 112233" or "112233".
//     Empty = auto-assign by BLE discovery order (unreliable across sessions).

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
    [JsonPropertyName("slots")]
    public List<SlotEntry> Slots { get; set; } = [];

    /// <summary>
    /// When true, startup aborts if any slot has an empty pm5Name.
    /// Prevents silent role-swaps in a live demo session.
    /// Set to true once all device names are known and filled in pm5_slots.json.
    /// </summary>
    [JsonPropertyName("requirePinnedNames")]
    public bool RequirePinnedNames { get; set; } = false;

    public static BridgeConfig LoadOrDefault()
    {
        string configPath = Path.Combine(AppContext.BaseDirectory, "pm5_slots.json");

        if (File.Exists(configPath))
        {
            try
            {
                string json = File.ReadAllText(configPath);
                var cfg = JsonSerializer.Deserialize<BridgeConfig>(json);
                if (cfg?.Slots?.Count > 0)
                {
                    Console.WriteLine($"[Config] Loaded {configPath}");
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
            Console.WriteLine("[Config] pm5_slots.json not found — using defaults (all slots auto-assigned).");
        }

        return new BridgeConfig
        {
            RequirePinnedNames = false,
            Slots =
            [
                new SlotEntry { Channel = "Strength", Port = 6789, Pm5Name = "" },
                new SlotEntry { Channel = "Rowing",   Port = 6791, Pm5Name = "" },
                new SlotEntry { Channel = "Cycling",  Port = 6792, Pm5Name = "" },
            ]
        };
    }

    /// <summary>
    /// Prints the slot assignment plan and warns about unpinned slots.
    /// Returns false if RequirePinnedNames is true and any slot is unpinned —
    /// caller must exit before starting servers.
    /// </summary>
    public bool ValidateAssignmentPlan()
    {
        int unpinnedCount = Slots.Count(s => string.IsNullOrEmpty(s.Pm5Name));

        Console.WriteLine("[Config] Slot assignment plan:");
        foreach (var s in Slots)
        {
            bool   pinned = !string.IsNullOrEmpty(s.Pm5Name);
            string badge  = pinned ? "[PINNED]" : "[AUTO]  ";
            string detail = pinned
                ? $"pm5Name=\"{s.Pm5Name}\""
                : "no pm5Name — will accept first available PM5";
            Console.WriteLine($"         {badge}  {s.Channel,-10}  port {s.Port}  {detail}");
        }
        Console.WriteLine();

        if (unpinnedCount == 0)
        {
            Console.WriteLine("[Config] All slots are pinned. Assignment is deterministic.");
            Console.WriteLine();
            return true;
        }

        // At least one unpinned slot — warn clearly
        Console.WriteLine($"[WARN]  {unpinnedCount} slot(s) have no pm5Name set.");
        Console.WriteLine("[WARN]  These slots will be filled by BLE discovery order,");
        Console.WriteLine("[WARN]  which is NOT stable across power cycles or reboots.");

        if (unpinnedCount > 1)
        {
            Console.WriteLine();
            Console.WriteLine("[WARN]  ????????????????????????????????????????????????????????");
            Console.WriteLine("[WARN]  ?  MULTIPLE UNPINNED SLOTS — ROLE SWAPS ARE POSSIBLE  ?");
            Console.WriteLine("[WARN]  ?  Rowing may become Cycling. Cycling may become       ?");
            Console.WriteLine("[WARN]  ?  Strength. Run --list-devices to get device names,   ?");
            Console.WriteLine("[WARN]  ?  then set pm5Name in pm5_slots.json.                 ?");
            Console.WriteLine("[WARN]  ????????????????????????????????????????????????????????");
        }

        Console.WriteLine();
        Console.WriteLine("[WARN]  To silence these warnings: set pm5Name for each slot.");
        Console.WriteLine("[WARN]  To make this a hard error:  set requirePinnedNames=true.");
        Console.WriteLine();

        if (RequirePinnedNames)
        {
            Console.WriteLine("[ERROR] requirePinnedNames=true but the following slots are unpinned:");
            foreach (var s in Slots.Where(s => string.IsNullOrEmpty(s.Pm5Name)))
                Console.WriteLine($"         {s.Channel}  port {s.Port}");
            Console.WriteLine("[ERROR] Set pm5Name for every slot, or set requirePinnedNames=false.");
            return false;  // caller must exit
        }

        return true;
    }
}
