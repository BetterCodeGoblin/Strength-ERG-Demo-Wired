// PM5BleBridge — BLE bridge for up to 3 simultaneous Concept2 PM5 devices.
//
// Replaces the wired PM5HidDiag.exe, RowingSimBridge.exe and CyclingSimBridge.exe
// for real hardware sessions.
//
// What it does:
//   1. Reads slot config from pm5_slots.json (auto-creates defaults if missing).
//   2. Starts three TCP listeners (default ports 6789 / 6791 / 6792).
//   3. Scans Bluetooth LE for devices whose advertised name starts with "PM5 ".
//   4. Assigns each found PM5 to the first unoccupied slot (or by pm5Name config).
//   5. Polls each device with CSAFE frames over BLE GATT at ~4 Hz.
//   6. Streams JSON telemetry lines to the Unreal TCP client on each slot's port.
//
// Usage:
//   PM5BleBridge.exe                   — use pm5_slots.json defaults
//   PM5BleBridge.exe --list-devices    — scan and print found PM5s without connecting
//
// Before running:
//   * Enable Bluetooth on your laptop.
//   * Turn on each PM5 device and start a workout (or press the button to wake it).
//   * PM5 BLE becomes discoverable within a few seconds of waking.
//
// Requires Windows 10 build 19041+ for Windows.Devices.Bluetooth APIs.

using Windows.Devices.Bluetooth.Advertisement;
using PM5BleBridge;

Console.WriteLine("????????????????????????????????????????????????????");
Console.WriteLine("?          PM5BleBridge  —  3-Device BLE           ?");
Console.WriteLine("????????????????????????????????????????????????????");
Console.WriteLine();

bool listOnly = args.Contains("--list-devices");

// ?? Load config and validate slot plan ?????????????????????????????????????
var config = BridgeConfig.LoadOrDefault();

if (!config.ValidateAssignmentPlan())
{
    // requirePinnedNames=true and at least one slot is unpinned — refuse to start.
    Console.WriteLine("[Bridge] Aborting. Fix pm5_slots.json and restart.");
    return;
}

// ?? Start TCP servers ????????????????????????????????????????????????????????
var servers = config.Slots
    .Select(s => new ChannelServer(s.Channel, s.Port, s.Pm5Name))
    .ToList();

if (!listOnly)
    foreach (var srv in servers) srv.Start();

// ?? Track which slots are occupied ??????????????????????????????????????????
// Key: bluetooth address (ulong), Value: channel name
var assigned = new Dictionary<ulong, string>();
var assignedLock = new object();
var activeTasks  = new Dictionary<ulong, Task>();
var cts          = new CancellationTokenSource();

Console.CancelKeyPress += (_, e) => { e.Cancel = true; cts.Cancel(); };

// ?? BLE advertisement watcher ????????????????????????????????????????????????
var watcher = new BluetoothLEAdvertisementWatcher
{
    ScanningMode = BluetoothLEScanningMode.Active
};

watcher.Received += (_, args) =>
{
    string name = args.Advertisement.LocalName;
    if (string.IsNullOrEmpty(name)) return;
    if (!name.StartsWith("PM5 ", StringComparison.OrdinalIgnoreCase)) return;

    ulong addr = args.BluetoothAddress;

    lock (assignedLock)
    {
        if (assigned.ContainsKey(addr)) return;  // already assigned

        if (listOnly)
        {
            Console.WriteLine($"  Found PM5: \"{name}\"  RSSI={args.RawSignalStrengthInDBm} dBm  addr={addr:X12}");
            return;
        }

        // Find an unoccupied slot that accepts this device
        ChannelServer? slot = null;
        foreach (var srv in servers)
        {
            bool slotFree = !assigned.Values.Contains(srv.ChannelName,
                StringComparer.OrdinalIgnoreCase);
            if (slotFree && srv.AcceptsDevice(name))
            {
                slot = srv;
                break;
            }
        }

        if (slot == null)
        {
            Console.WriteLine($"[Scan] Found PM5 \"{name}\" but no free slot accepts it — ignoring.");
            return;
        }

        assigned[addr] = slot.ChannelName;
        string assignMode = slot.IsPinned
            ? $"pinned name match (\"{ slot.ConfiguredName}\")"
            : "auto-assigned (discovery order — consider pinning in pm5_slots.json)";
        Console.WriteLine($"[Scan] PM5 \"{name}\"  RSSI={args.RawSignalStrengthInDBm} dBm");
        Console.WriteLine($"       ? [{slot.ChannelName}] port {slot.Port}  [{assignMode}]");

        var device = new PM5BleDevice(name, addr, slot);
        activeTasks[addr] = ConnectWithRetryAsync(device, addr, cts.Token);
    }
};

watcher.Start();
Console.WriteLine("[Scan] Scanning for PM5 devices...");
Console.WriteLine("       Wake each PM5 by pressing its button or starting a workout.");
Console.WriteLine("       Press Ctrl+C to stop.");
Console.WriteLine();

if (listOnly)
{
    // Run for 10 seconds then exit
    await Task.Delay(10_000, cts.Token).ContinueWith(_ => { });
    watcher.Stop();
    Console.WriteLine("[Scan] Done.");
    return;
}

// ?? Keep running until Ctrl+C ????????????????????????????????????????????????
try { await Task.Delay(Timeout.Infinite, cts.Token); }
catch (OperationCanceledException) { }

Console.WriteLine("[Bridge] Stopping...");
watcher.Stop();
cts.Cancel();

foreach (var srv in servers) srv.Stop();
await Task.WhenAll(activeTasks.Values.Append(Task.CompletedTask));
Console.WriteLine("[Bridge] Stopped.");

// ?? Connect with automatic reconnect ????????????????????????????????????????
static async Task ConnectWithRetryAsync(PM5BleDevice device, ulong addr,
    CancellationToken ct)
{
    while (!ct.IsCancellationRequested)
    {
        await device.RunAsync(ct);

        if (ct.IsCancellationRequested) break;

        Console.WriteLine($"[{device.Channel.ChannelName}] Lost connection to {device.DeviceName} — retrying in 5s...");
        device.Channel.Send($"{{\"type\":\"status\",\"connected\":false," +
                            $"\"statusText\":\"BLE reconnecting ({device.DeviceName})\"}}");

        try { await Task.Delay(5_000, ct); }
        catch (OperationCanceledException) { break; }
    }
}
