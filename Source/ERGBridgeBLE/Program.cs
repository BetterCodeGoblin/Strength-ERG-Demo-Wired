// ErgBridgeBLE — Concept2 PM5 Bluetooth LE → TCP bridge for Unity
//
// Scans Windows BLE for a PM5 device (identified by "PM5" name prefix or
// Concept2 Rowing Service UUID), subscribes to per-rep stroke notifications,
// and streams newline-delimited JSON to a TCP client on 127.0.0.1:6790.
//
// Unity side: Concept2UsbReader.cs (useBleWireless = true) launches this
// process and connects to the TCP server to receive rep events.
//
// Build:  dotnet build -c Release
// Run:    ErgBridgeBLE.exe   (auto-launched by Unity, or run standalone)

using System;
using System.Collections.Concurrent;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using Windows.Devices.Bluetooth;
using Windows.Devices.Enumeration;
using Windows.Devices.Bluetooth.Advertisement;
using Windows.Devices.Bluetooth.GenericAttributeProfile;
using Windows.Storage.Streams;

// ── Concept2 PM5 BLE GUIDs ─────────────────────────────────────────────────

static class C2Uuids
{
    // Root discovery UUID — in the PM5 advertisement packet.
    public static readonly Guid DiscoveryService = new("CE060000-43E5-11E4-916C-0800200C9A66");

    // ── RowErg / SkiErg / BikeErg services (CE060030 base) ─────────────────
    // Rowing data service — only on row/ski/bike PM5, NOT on StrengthERG
    public static readonly Guid RowingService   = new("CE060030-43E5-11E4-916C-0800200C9A66");
    public static readonly Guid StrokeData      = new("CE060035-43E5-11E4-916C-0800200C9A66");
    public static readonly Guid GeneralStatus   = new("CE060031-43E5-11E4-916C-0800200C9A66");
    // Multiplexed channel (byte[0]=selector): used by pm5-base / ErgometerJS
    public static readonly Guid Multiplexed     = new("CE060080-43E5-11E4-916C-0800200C9A66");

    // ── StrengthERG services (CE060060 base) ────────────────────────────────
    // The StrengthERG exposes CE060060 instead of CE060030.
    // Characteristic offsets mirror the rowing service:
    //   CE060061 ≈ CE060031 (GeneralStatus / 1 Hz status)
    //   CE060065 ≈ CE060035 (RepData / per-rep notification)
    public static readonly Guid StrengthService   = new("CE060060-43E5-11E4-916C-0800200C9A66");
    public static readonly Guid StrengthGenStatus = new("CE060061-43E5-11E4-916C-0800200C9A66");
    public static readonly Guid StrengthRepData   = new("CE060065-43E5-11E4-916C-0800200C9A66");
}

// ── Shared state passed between BLE callbacks and TCP sender ────────────────

sealed class BridgeState
{
    public bool   IsConnected  { get; set; }
    public string StatusText   { get; set; } = "Disconnected";
    public int    HeartRate    { get; set; }
    public float  ElapsedSec   { get; set; }

    // Per-rep events queued for dispatch to Unity
    public readonly ConcurrentQueue<string> OutboundMessages = new();
}

// ═══════════════════════════════════════════════════════════════════════════
class Program
{
    private const int    TCP_PORT  = 6790;   // 6789 is used by the existing USB ErgBridge
    private const string TCP_HOST = "127.0.0.1";

    static async Task Main(string[] args)
    {
        Console.WriteLine("[ErgBridgeBLE] Starting — Concept2 PM5 BLE Bridge");
        Console.WriteLine($"[ErgBridgeBLE] TCP server on {TCP_HOST}:{TCP_PORT}");

        using var cts = new CancellationTokenSource();

        // Clean shutdown on Ctrl+C or app close
        Console.CancelKeyPress += (_, e) => { e.Cancel = true; cts.Cancel(); };
        AppDomain.CurrentDomain.ProcessExit += (_, _) => cts.Cancel();

        var state = new BridgeState();

        // Run TCP server and BLE scanner concurrently
        var tcpTask = RunTcpServerAsync(state, cts.Token);
        var bleTask = RunBleScanLoopAsync(state, cts.Token);

        try { await Task.WhenAll(tcpTask, bleTask); }
        catch (OperationCanceledException) { }
        catch (Exception ex) { Console.WriteLine($"[ErgBridgeBLE] Fatal: {ex.Message}"); }

        Console.WriteLine("[ErgBridgeBLE] Shutdown complete.");
    }

    // ── TCP Server ──────────────────────────────────────────────────────────

    static async Task RunTcpServerAsync(BridgeState state, CancellationToken ct)
    {
        var listener = new TcpListener(IPAddress.Parse(TCP_HOST), TCP_PORT);
        listener.Start();
        Console.WriteLine($"[TCP] Listening on {TCP_HOST}:{TCP_PORT}");

        while (!ct.IsCancellationRequested)
        {
            TcpClient? client = null;
            try
            {
                // AcceptTcpClientAsync doesn't accept CancellationToken in older APIs;
                // wrap with a task that watches ct
                client = await AcceptWithCancelAsync(listener, ct);
                Console.WriteLine("[TCP] Unity connected.");
                await ServeClientAsync(client, state, ct);
            }
            catch (OperationCanceledException) { break; }
            catch (Exception ex) { Console.WriteLine($"[TCP] Client error: {ex.Message}"); }
            finally
            {
                client?.Close();
                Console.WriteLine("[TCP] Unity disconnected. Waiting for reconnect...");
            }
        }

        listener.Stop();
    }

    static async Task<TcpClient> AcceptWithCancelAsync(TcpListener listener, CancellationToken ct)
    {
        var acceptTask = listener.AcceptTcpClientAsync();
        var cancelTask = Task.Delay(Timeout.Infinite, ct);
        var winner     = await Task.WhenAny(acceptTask, cancelTask);
        if (winner == cancelTask) { ct.ThrowIfCancellationRequested(); }
        return await acceptTask;
    }

    static async Task ServeClientAsync(TcpClient client, BridgeState state, CancellationToken ct)
    {
        await using var stream = client.GetStream();

        while (!ct.IsCancellationRequested && client.Connected)
        {
            // Drain all queued messages
            while (state.OutboundMessages.TryDequeue(out string? msg))
            {
                byte[] bytes = Encoding.UTF8.GetBytes(msg + "\n");
                await stream.WriteAsync(bytes, ct);
            }

            await Task.Delay(50, ct); // 20 Hz drain cycle
        }
    }

    // ── BLE Scanner Loop ────────────────────────────────────────────────────

    static async Task RunBleScanLoopAsync(BridgeState state, CancellationToken ct)
    {
        while (!ct.IsCancellationRequested)
        {
            state.IsConnected = false;

            // ── Step 0: Try the cached address from a previous run ─────────
            // This lets us reconnect instantly without waiting for an advertisement,
            // which is critical when the PM5 is awake but not actively broadcasting.
            if (TryLoadKnownAddress(out ulong cached, out BluetoothAddressType cachedType))
            {
                Console.WriteLine($"[BLE] Trying cached PM5 address 0x{cached:X12} (type={cachedType})...");
                state.StatusText = "Connecting to known PM5...";
                try
                {
                    await ConnectAndRunAsync(cached, cachedType, state, ct);
                    if (!ct.IsCancellationRequested)
                    {
                        Console.WriteLine("[BLE] Reconnecting in 3s...");
                        await Task.Delay(3000, ct);
                    }
                    continue;   // success — loop back and use cache again
                }
                catch (OperationCanceledException) { break; }
                catch (Exception ex)
                {
                    // If the error is Unreachable, the PM5 exists but is not yet accepting
                    // connections (e.g. it is still in ERG Link scan / Central mode on startup).
                    // Keep hammering through the cache at short intervals rather than
                    // waiting 60s in the advertisement watcher — we want to connect the
                    // instant the PM5 becomes connectable.
                    bool isUnreachable = ex.Message.IndexOf("Unreachable",
                                             StringComparison.OrdinalIgnoreCase) >= 0
                                      || ex.Message.IndexOf("service not found",
                                             StringComparison.OrdinalIgnoreCase) >= 0;
                    if (isUnreachable)
                    {
                        Console.WriteLine("[BLE] PM5 found but not yet connectable " +
                            "(ERG Link scan in progress?). Retrying in 1.5s...");
                        await Task.Delay(1500, ct);
                        continue;   // retry cached address, skip advertisement scan
                    }

                    Console.WriteLine($"[BLE] Cached address failed ({ex.Message}). Falling back to scan.");
                }
            }

            // ── Step 1: Advertisement watcher (60s) ────────────────────────
            Console.WriteLine("[BLE] Scanning for PM5 via advertisement watcher...");
            state.StatusText = "Scanning for PM5...";
            var scanResult = await ScanForPm5Async(ct);

            // ── Step 2: Windows BLE device cache ──────────────────────────
            if (scanResult == null && !ct.IsCancellationRequested)
            {
                Console.WriteLine("[BLE] No PM5 in advertisements — checking Windows BLE cache...");
                scanResult = await FindPm5InCacheAsync();
            }

            if (scanResult == null)
            {
                Console.WriteLine("[BLE] PM5 not found. Wake the PM5 and start a workout, then retry.");
                Console.WriteLine("[BLE] Retrying in 5s...");
                await Task.Delay(5000, ct);
                continue;
            }

            var (foundAddr, foundType) = scanResult.Value;
            // Save for future runs before connecting
            SaveKnownAddress(foundAddr, foundType);
            Console.WriteLine($"[BLE] PM5 found at 0x{foundAddr:X12} (type={foundType}). Connecting...");

            try
            {
                await ConnectAndRunAsync(foundAddr, foundType, state, ct);
            }
            catch (OperationCanceledException) { break; }
            catch (Exception ex)
            {
                Console.WriteLine($"[BLE] Session error: {ex.Message}");
                state.IsConnected = false;
                state.StatusText  = "BLE error — reconnecting";
                EnqueueStatus(state);
            }

            if (!ct.IsCancellationRequested)
            {
                Console.WriteLine("[BLE] Reconnecting in 3s...");
                await Task.Delay(3000, ct);
            }
        }
    }

    // ── BLE Advertisement Scanner ───────────────────────────────────────────
    //
    // Strategy:
    //   1. Run BluetoothLEAdvertisementWatcher in Active mode for up to 60s.
    //      Log EVERY device seen (so Unity console shows the full BLE neighborhood).
    //      Accept the device if its name contains "PM5"/"pm5"/"Concept2" anywhere,
    //      OR if its advertisement includes the Concept2 Rowing Service UUID.
    //   2. If the watcher expires with no match, fall back to
    //      DeviceInformation.FindAllAsync which queries Windows' BLE cache
    //      (covers devices that were previously seen / paired).
    //
    // On Windows, the device "LocalName" can arrive in a *separate* advertisement
    // event (scan response) from the one with service UUIDs. We collect both by
    // keeping a dictionary of (address → best candidate) and resolving names from
    // multiple events.

    private static readonly HashSet<ulong> _loggedAddresses = new();

    static Task<(ulong addr, BluetoothAddressType addrType)?> ScanForPm5Async(CancellationToken ct)
    {
        var tcs     = new TaskCompletionSource<(ulong, BluetoothAddressType)?>();
        var watcher = new BluetoothLEAdvertisementWatcher();
        // Collect names from both advertisement and scan-response packets
        var seenNames = new System.Collections.Concurrent.ConcurrentDictionary<ulong, string>();

        watcher.ScanningMode = BluetoothLEScanningMode.Active;

        watcher.Received += (_, args) =>
        {
            if (tcs.Task.IsCompleted) return;

            ulong addr = args.BluetoothAddress;
            string name = args.Advertisement.LocalName ?? "";

            // Some devices only put the name in the scan response (a separate event)
            // Keep the best (non-empty) name we've seen for each address
            if (!string.IsNullOrEmpty(name))
                seenNames[addr] = name;
            else if (seenNames.TryGetValue(addr, out string? cached))
                name = cached ?? "";   // use previously seen name

            // Log every device found (once per address) so Unity console shows the neighbourhood
            if (_loggedAddresses.Add(addr))
                Console.WriteLine($"[BLE] Nearby: addr=0x{addr:X12} name=\"{name ?? ""}\" type={args.BluetoothAddressType}");

            // PM5 detection — accept any of these criteria:
            bool isPm5ByName    = !string.IsNullOrEmpty(name) &&
                                  (name.IndexOf("PM5",      StringComparison.OrdinalIgnoreCase) >= 0 ||
                                   name.IndexOf("PM3",      StringComparison.OrdinalIgnoreCase) >= 0 ||
                                   name.IndexOf("STR",      StringComparison.OrdinalIgnoreCase) >= 0 ||   // StrengthErg
                                   name.IndexOf("Concept2", StringComparison.OrdinalIgnoreCase) >= 0);

            // The PM5 advertises CE060000 (discovery service) — NOT CE060030 (rowing data).
            // CE060030 is only discoverable AFTER connecting; checking it here always misses the PM5.
            bool isPm5ByService = false;
            foreach (var uuid in args.Advertisement.ServiceUuids)
                if (uuid == C2Uuids.DiscoveryService || uuid == C2Uuids.RowingService)
                { isPm5ByService = true; break; }

            if (isPm5ByName || isPm5ByService)
            {
                Console.WriteLine($"[BLE] >>> PM5 identified: \"{name}\" (0x{addr:X12}) " +
                                  $"addrType={args.BluetoothAddressType} byName={isPm5ByName} byService={isPm5ByService}");
                watcher.Stop();
                tcs.TrySetResult((addr, args.BluetoothAddressType));
            }
        };

        watcher.Stopped += (_, _) => tcs.TrySetResult(null);

        ct.Register(() => { try { watcher.Stop(); } catch { } tcs.TrySetCanceled(); });

        watcher.Start();
        Console.WriteLine("[BLE] Advertisement watcher started (60s window). Logging all nearby BLE devices...");

        // Auto-stop after 60s — RunBleScanLoopAsync retries automatically so no hurry
        _ = Task.Delay(60_000, ct).ContinueWith(_ =>
        {
            try { watcher.Stop(); } catch { }
        }, TaskContinuationOptions.NotOnCanceled);

        return tcs.Task;
    }

    // Fallback: query Windows BLE cache for any previously seen/paired devices
    static async Task<(ulong addr, BluetoothAddressType addrType)?> FindPm5InCacheAsync()
    {
        try
        {
            // Build AQS filter for BLE devices that advertise the Concept2 discovery UUID
            // (CE060000 is what the PM5 puts in its advertisement — CE060030 is post-connect only)
            string aqsFilter = GattDeviceService.GetDeviceSelectorFromUuid(C2Uuids.DiscoveryService);
            var devices = await DeviceInformation.FindAllAsync(aqsFilter);

            if (devices.Count > 0)
            {
                var d = devices[0];
                Console.WriteLine($"[BLE] Cache hit: \"{d.Name}\" (id={d.Id})");
                // Extract BT address from device ID string (format ends with _<12-hex-digits>)
                string id = d.Id;
                int last = id.LastIndexOf('_');
                if (last >= 0 && ulong.TryParse(id.Substring(last + 1),
                        System.Globalization.NumberStyles.HexNumber, null, out ulong addr))
                    return (addr, BluetoothAddressType.Random);
            }

            // Broader search: all paired/cached BLE devices
            string broad = BluetoothLEDevice.GetDeviceSelectorFromPairingState(false);
            var all = await DeviceInformation.FindAllAsync(broad);
            Console.WriteLine($"[BLE] Cache: {all.Count} total BLE device(s) known to Windows:");
            foreach (var d in all)
            {
                Console.WriteLine($"[BLE]   \"{d.Name}\" id={d.Id}");
                if (d.Name.IndexOf("PM5",      StringComparison.OrdinalIgnoreCase) >= 0 ||
                    d.Name.IndexOf("STR",      StringComparison.OrdinalIgnoreCase) >= 0 ||   // StrengthErg
                    d.Name.IndexOf("Concept2", StringComparison.OrdinalIgnoreCase) >= 0)
                {
                    string id = d.Id;
                    int last = id.LastIndexOf('_');
                    if (last >= 0 && ulong.TryParse(id.Substring(last + 1),
                            System.Globalization.NumberStyles.HexNumber, null, out ulong addr))
                    {
                        Console.WriteLine($"[BLE] Cache match: \"{d.Name}\"");
                        return (addr, BluetoothAddressType.Random);
                    }
                }
            }
        }
        catch (Exception ex) { Console.WriteLine($"[BLE] Cache search failed: {ex.Message}"); }
        return null;
    }

    // ── GATT Connection + Characteristic Subscription ──────────────────────

    static async Task ConnectAndRunAsync(ulong address, BluetoothAddressType addrType, BridgeState state, CancellationToken ct)
    {
        Console.WriteLine($"[BLE] Connecting to 0x{address:X12} (addrType={addrType})...");
        using BluetoothLEDevice device = await BluetoothLEDevice.FromBluetoothAddressAsync(address, addrType);
        if (device == null) throw new InvalidOperationException("Failed to create BLE device object.");

        string deviceName = device.Name ?? "PM5";
        Console.WriteLine($"[BLE] Connected to \"{deviceName}\"");

        // Brief pause to let the Windows BLE stack register before GATT query
        await Task.Delay(50, ct);

        // ── Get data service ────────────────────────────────────────────────
        // StrengthERG advertises as "STR <serial>" so we can skip the CE060030
        // (rowing service) UUID lookup entirely for strength machines.
        bool isKnownStrengthErg = deviceName.IndexOf("STR", StringComparison.OrdinalIgnoreCase) >= 0;

        GattDeviceService? rowingServiceRaw = null;
        bool gattWasUnreachable = false;

        if (!isKnownStrengthErg)
        {
            // RowErg / SkiErg / BikeErg — try targeted CE060030 UUID lookup first
            var svcResult = await device.GetGattServicesForUuidAsync(C2Uuids.RowingService,
                                                                      BluetoothCacheMode.Uncached);
            Console.WriteLine($"[BLE] GetGattServicesForUuid(RowingService) → status={svcResult.Status} count={svcResult.Services.Count}");

            if (svcResult.Status == GattCommunicationStatus.Unreachable)
                gattWasUnreachable = true;

            if (svcResult.Status == GattCommunicationStatus.Success && svcResult.Services.Count > 0)
                rowingServiceRaw = svcResult.Services[0];
        }
        else
        {
            Console.WriteLine("[BLE] StrengthERG detected — skipping CE060030 lookup, enumerating all services directly.");
        }

        if (rowingServiceRaw == null && !gattWasUnreachable)
        {
            // Full service enumeration: StrengthERG always takes this path;
            // RowErg takes it only when the targeted lookup returns 0 services.
            Console.WriteLine("[BLE] Enumerating all services...");
            var allSvcs = await device.GetGattServicesAsync(BluetoothCacheMode.Uncached);
            Console.WriteLine($"[BLE] All services ({allSvcs.Status}): {allSvcs.Services.Count} found");
            if (allSvcs.Status == GattCommunicationStatus.Unreachable)
                gattWasUnreachable = true;
            foreach (var svc in allSvcs.Services)
            {
                Console.WriteLine($"[BLE]   Service UUID: {svc.Uuid}");
                // Check if this service contains any of our target characteristics
                var chars = await svc.GetCharacteristicsAsync(BluetoothCacheMode.Uncached);
                foreach (var ch in chars.Characteristics)
                {
                    Console.WriteLine($"[BLE]     Char UUID: {ch.Uuid}");
                    if (ch.Uuid == C2Uuids.StrokeData || ch.Uuid == C2Uuids.Multiplexed ||
                        ch.Uuid == C2Uuids.GeneralStatus ||
                        ch.Uuid == C2Uuids.StrengthRepData || ch.Uuid == C2Uuids.StrengthGenStatus)
                    {
                        Console.WriteLine($"[BLE]   *** Found PM5 data char {ch.Uuid} in service {svc.Uuid}");
                        rowingServiceRaw = svc;
                        break;
                    }
                }
                if (rowingServiceRaw != null) break;
                else svc.Dispose();
            }
        }

        if (rowingServiceRaw == null)
        {
            // Preserve "Unreachable" in the message so RunBleScanLoopAsync
            // retries the cached address instead of dropping to a 60s scan.
            // The PM5 is Unreachable while it is still in BLE Central mode
            // completing the ERG Link connection to the machine body.
            if (gattWasUnreachable)
                throw new InvalidOperationException(
                    "PM5 GATT Unreachable — ERG Link setup still in progress. Will retry.");
            throw new InvalidOperationException(
                "No PM5 rowing service found. " +
                "PM5 BLE is single-connection — close ErgData app and disable Bluetooth on your phone, " +
                "then start an active workout on the PM5 before launching Unity.");
        }

        using GattDeviceService rowingService = rowingServiceRaw;
        bool anySubscribed = false;

        bool isStrengthErg = rowingService.Uuid == C2Uuids.StrengthService;
        Console.WriteLine($"[BLE] Service mode: {(isStrengthErg ? "StrengthERG (CE060060)" : "RowErg/SkiErg/BikeErg (CE060030)")}");

        if (isStrengthErg)
        {
            // ── StrengthERG: CSAFE polling via CE060020 service ──────────────
            // Passive notifications on CE060060 are silent on the StrengthERG
            // (they're internal ERG Link data, not for external apps).
            // Instead we poll via CSAFE commands, same protocol as USB HID:
            //   CE060021 = write (TX) — we send CSAFE frames here
            //   CE060022 = notify (RX) — PM5 sends CSAFE responses here
            //
            // Commands (from the working USB CSAFE code):
            //   STROKESTATS (0x6E via wrapper 0x1A) → rep count, drive time, pull dist
            //   GETTWORK    (0xA0)                  → elapsed time
            //   GETHRCUR    (0xB0)                  → heart rate

            Console.WriteLine("[BLE] StrengthERG: setting up CSAFE polling via CE060020...");

            // Get CE060020 control service (fresh, Uncached)
            var ctrlSvcResult = await device.GetGattServicesForUuidAsync(
                new Guid("CE060020-43E5-11E4-916C-0800200C9A66"), BluetoothCacheMode.Uncached);
            if (ctrlSvcResult.Status != GattCommunicationStatus.Success || ctrlSvcResult.Services.Count == 0)
                throw new InvalidOperationException($"CE060020 control service not available (status={ctrlSvcResult.Status}).");

            using var ctrlSvc = ctrlSvcResult.Services[0];

            // Get CE060021 (TX) and CE060022 (RX) characteristics
            var txResult = await ctrlSvc.GetCharacteristicsForUuidAsync(
                new Guid("CE060021-43E5-11E4-916C-0800200C9A66"), BluetoothCacheMode.Uncached);
            var rxResult = await ctrlSvc.GetCharacteristicsForUuidAsync(
                new Guid("CE060022-43E5-11E4-916C-0800200C9A66"), BluetoothCacheMode.Uncached);

            if (txResult.Characteristics.Count == 0)
                throw new InvalidOperationException("CE060021 (TX) characteristic not found.");
            if (rxResult.Characteristics.Count == 0)
                throw new InvalidOperationException("CE060022 (RX) characteristic not found.");

            var txChar = txResult.Characteristics[0];
            var rxChar = rxResult.Characteristics[0];

            // Subscribe to CE060022 for CSAFE responses
            var csafeResponseQueue = new ConcurrentQueue<byte[]>();
            rxChar.ValueChanged += (_, args) =>
            {
                byte[] raw = ReadBuffer(args.CharacteristicValue);
                Console.WriteLine($"[CSAFE] RX [{raw.Length}]: {BitConverter.ToString(raw)}");
                csafeResponseQueue.Enqueue(raw);
            };
            await EnableNotifyAsync(rxChar);
            Console.WriteLine("[BLE] Subscribed to CE060022 (CSAFE RX).");
            anySubscribed = true;

            // ── Mark connected ──────────────────────────────────────────
            state.IsConnected = true;
            state.StatusText  = $"PM5 Connected (BLE CSAFE) — {deviceName}";
            EnqueueStatus(state);
            Console.WriteLine("[BLE] Ready — starting CSAFE poll loop.");

            // ── CSAFE polling loop ───────────────────────────────────────
            // Same commands and framing as USB: F1 <cmds> <checksum> F2
            int lastRepCount = -1;
            while (!ct.IsCancellationRequested && device.ConnectionStatus == BluetoothConnectionStatus.Connected)
            {
                try
                {
                    // 1. STROKESTATS (proprietary: 0x1A wrapper + 0x6E sub-command)
                    //    USB frame: F1 1A 02 6E 00 <chk> F2
                    await WriteCsafeAsync(txChar, new byte[] { 0x1A, 0x02, 0x6E, 0x00 });
                    await Task.Delay(200, ct);

                    // Check for response
                    while (csafeResponseQueue.TryDequeue(out byte[] resp))
                    {
                        // Try to parse STROKESTATS data from the CSAFE response
                        byte[] ssData = ExtractProprietaryData(resp, 0x6E);
                        if (ssData != null && ssData.Length >= 7)
                        {
                            int driveTimeRaw = ssData[2];
                            int pullDist     = ssData[5];
                            int repCounter   = ssData[6];

                            float driveTimeSec = driveTimeRaw * 0.01f;
                            state.ElapsedSec = state.ElapsedSec; // keep last known

                            if (repCounter > 0 && repCounter > lastRepCount)
                            {
                                lastRepCount = repCounter;
                                string json = $"{{\"type\":\"rep\",\"repCount\":{repCounter}," +
                                              $"\"driveTimeSec\":{driveTimeSec:F3}," +
                                              $"\"pullDistance\":{pullDist}," +
                                              $"\"heartRate\":{state.HeartRate}," +
                                              $"\"elapsedSec\":{state.ElapsedSec:F2}}}";
                                state.OutboundMessages.Enqueue(json);
                                Console.WriteLine($"[CSAFE] Rep #{repCounter}: time={driveTimeSec:F2}s dist={pullDist}");
                            }
                        }

                        // Also try standard command responses (GETTWORK, GETHRCUR)
                        byte[] wtData = ExtractPublicCmdData(resp, 0xA0);
                        if (wtData != null && wtData.Length >= 3)
                        {
                            state.ElapsedSec = wtData[0] * 3600f + wtData[1] * 60f + wtData[2];
                        }
                        byte[] hrData = ExtractPublicCmdData(resp, 0xB0);
                        if (hrData != null && hrData.Length >= 1)
                        {
                            state.HeartRate = hrData[0];
                        }
                    }

                    // 2. GETTWORK (elapsed time)
                    await WriteCsafeAsync(txChar, new byte[] { 0xA0 });
                    await Task.Delay(150, ct);

                    // 3. GETHRCUR (heart rate)
                    await WriteCsafeAsync(txChar, new byte[] { 0xB0 });
                    await Task.Delay(150, ct);

                    // Drain any remaining responses
                    while (csafeResponseQueue.TryDequeue(out byte[] resp2))
                    {
                        byte[] wtData = ExtractPublicCmdData(resp2, 0xA0);
                        if (wtData != null && wtData.Length >= 3)
                            state.ElapsedSec = wtData[0] * 3600f + wtData[1] * 60f + wtData[2];
                        byte[] hrData = ExtractPublicCmdData(resp2, 0xB0);
                        if (hrData != null && hrData.Length >= 1)
                            state.HeartRate = hrData[0];
                        byte[] ssData = ExtractProprietaryData(resp2, 0x6E);
                        if (ssData != null && ssData.Length >= 7)
                        {
                            int repCounter = ssData[6];
                            if (repCounter > 0 && repCounter > lastRepCount)
                            {
                                lastRepCount = repCounter;
                                float dt = ssData[2] * 0.01f;
                                int pd = ssData[5];
                                string json = $"{{\"type\":\"rep\",\"repCount\":{repCounter}," +
                                              $"\"driveTimeSec\":{dt:F3}," +
                                              $"\"pullDistance\":{pd}," +
                                              $"\"heartRate\":{state.HeartRate}," +
                                              $"\"elapsedSec\":{state.ElapsedSec:F2}}}";
                                state.OutboundMessages.Enqueue(json);
                                Console.WriteLine($"[CSAFE] Rep #{repCounter}: time={dt:F2}s dist={pd}");
                            }
                        }
                    }

                    EnqueueStatus(state);
                }
                catch (OperationCanceledException) { break; }
                catch (Exception ex)
                {
                    Console.WriteLine($"[CSAFE] Poll error: {ex.Message}");
                    await Task.Delay(500, ct);
                }
            }
            return; // skip the generic monitor loop below
        }
        else
        {
            // ── RowErg / SkiErg / BikeErg: CE060030 service ─────────────────

            // ── 1. Multiplexed (CE060080) — preferred; used by pm5-base and ErgometerJS ──
            try
            {
                await Task.Delay(200, ct);
                GattCharacteristic muxChar = await GetCharacteristicAsync(rowingService, C2Uuids.Multiplexed);
                muxChar.ValueChanged += (_, args) => OnMultiplexedData(args.CharacteristicValue, state);
                await EnableNotifyAsync(muxChar);
                Console.WriteLine("[BLE] Subscribed to Multiplexed (CE060080) — primary channel.");
                anySubscribed = true;
            }
            catch (Exception ex)
            {
                Console.WriteLine($"[BLE] CE060080 unavailable: {ex.Message}. Will try direct characteristics.");
            }

            // ── 2. Stroke Data (CE060035) — direct fallback ───────────────────
            try
            {
                await Task.Delay(200, ct);
                GattCharacteristic strokeChar = await GetCharacteristicAsync(rowingService, C2Uuids.StrokeData);
                strokeChar.ValueChanged += (_, args) => OnStrokeData(args.CharacteristicValue, state);
                await EnableNotifyAsync(strokeChar);
                Console.WriteLine("[BLE] Subscribed to StrokeData (CE060035).");
                anySubscribed = true;
            }
            catch (Exception ex)
            {
                Console.WriteLine($"[BLE] CE060035 unavailable: {ex.Message}");
            }

            // ── 3. General Status (CE060031) — 1 Hz elapsed + heart rate ─────
            try
            {
                await Task.Delay(200, ct);
                GattCharacteristic statusChar = await GetCharacteristicAsync(rowingService, C2Uuids.GeneralStatus);
                statusChar.ValueChanged += (_, args) => OnGeneralStatus(args.CharacteristicValue, state);
                await EnableNotifyAsync(statusChar);
                Console.WriteLine("[BLE] Subscribed to GeneralStatus (CE060031).");
                anySubscribed = true;
            }
            catch (Exception ex)
            {
                Console.WriteLine($"[BLE] CE060031 unavailable: {ex.Message}");
            }
        }

        if (!anySubscribed)
            throw new InvalidOperationException("No PM5 characteristics could be subscribed.");

        // ── Mark connected ──────────────────────────────────────────────────
        state.IsConnected = true;
        state.StatusText  = $"PM5 Connected (BLE) — {deviceName}";
        EnqueueStatus(state);

        Console.WriteLine("[BLE] Ready — waiting for rep notifications. Press Ctrl+C to quit.");

        // ── Monitor until device disconnects or ct cancelled ────────────────
        var disconnected = new TaskCompletionSource<bool>();
        device.ConnectionStatusChanged += (d, _) =>
        {
            if (d.ConnectionStatus == BluetoothConnectionStatus.Disconnected)
                disconnected.TrySetResult(true);
        };
        ct.Register(() => disconnected.TrySetResult(false));

        await disconnected.Task;

        state.IsConnected = false;
        state.StatusText  = "PM5 Disconnected";
        EnqueueStatus(state);
        Console.WriteLine("[BLE] PM5 disconnected.");
    }

    static async Task<GattCharacteristic> GetCharacteristicAsync(GattDeviceService service, Guid uuid)
    {
        var result = await service.GetCharacteristicsForUuidAsync(uuid, BluetoothCacheMode.Uncached);
        if (result.Status != GattCommunicationStatus.Success || result.Characteristics.Count == 0)
            throw new InvalidOperationException($"Characteristic {uuid} not found (status={result.Status}).");
        return result.Characteristics[0];
    }

    static async Task EnableNotifyAsync(GattCharacteristic ch)
    {
        var status = await ch.WriteClientCharacteristicConfigurationDescriptorAsync(
                         GattClientCharacteristicConfigurationDescriptorValue.Notify);
        if (status != GattCommunicationStatus.Success)
            throw new InvalidOperationException($"Failed to enable notify on {ch.Uuid} (status={status}).");
    }

    // ── BLE Data Parsers ────────────────────────────────────────────────────
    //
    // CE060035 — Rowing Stroke Data (20 bytes, fires once per completed rep)
    //
    //  Byte  Size  Description
    //  0     1     Stroke State (0=Idle 1=Acceleration 2=Driving 3=Dwelling 4=Recovery)
    //  1-3   3     Elapsed Time   (uint24 LE, ×0.01 s)
    //  4     1     Drive Length   (uint8,     ×0.01 m = cm) ← analogous to USB pullDistance
    //  5     1     Drive Time     (uint8,     ×0.01 s)      ← analogous to USB driveTime
    //  6-7   2     Stroke Distance(uint16 LE, ×0.01 m)
    //  8-9   2     Peak Drive Force(int16 LE,  N)
    // 10-11  2     Avg Drive Force (int16 LE,  N)
    // 12-15  4     Work Per Stroke (uint32 LE, ×0.001 J)
    // 16-17  2     Stroke Count    (uint16 LE)
    // 18     1     Stroke Rate     (uint8,     SPM)
    // 19     1     (reserved / padding)

    static void OnStrokeData(IBuffer buffer, BridgeState state)
        => OnStrokeDataBytes(ReadBuffer(buffer), state);

    static void OnStrokeDataBytes(byte[] data, BridgeState state)
    {
        if (data.Length < 18) return;   // need at least through StrokeCount

        // Skip strokes in idle state (state == 0) — only process completed drive reps
        // State 3 = DwellingAfterDrive, State 4 = Recovery both indicate a completed rep
        int strokeState = data[0];
        if (strokeState == 0 || strokeState == 1) return;

        float elapsedSec  = ((data[1]) | (data[2] << 8) | (data[3] << 16)) * 0.01f;
        int   driveLength = data[4];                              // cm (0.01m units)
        float driveTimeSec = data[5] * 0.01f;                    // seconds
        int   strokeCount  = data[16] | (data[17] << 8);         // uint16 LE

        if (strokeCount <= 0 || driveTimeSec <= 0f) return;

        // Use driveLength as pullDistance (same role as USB data[5])
        // Multiply by 10 to bring into comparable range with USB raw units
        // (USB emits 50–120, BLE drive_length in cm is typically 50–120 cm for StrengthErg)
        int pullDistance = driveLength;

        state.ElapsedSec = elapsedSec;

        string json = $"{{\"type\":\"rep\",\"repCount\":{strokeCount}," +
                      $"\"driveTimeSec\":{driveTimeSec:F3}," +
                      $"\"pullDistance\":{pullDistance}," +
                      $"\"heartRate\":{state.HeartRate}," +
                      $"\"elapsedSec\":{elapsedSec:F2}}}";

        state.OutboundMessages.Enqueue(json);
        Console.WriteLine($"[BLE] Rep #{strokeCount}: time={driveTimeSec:F2}s dist={pullDistance}cm elapsed={elapsedSec:F1}s");
    }

    // CE060031 — General Row Status (20 bytes, ~1 Hz)
    //
    //  Byte  Size  Description
    //  0-2   3     Elapsed Time   (uint24 LE, ×0.01 s)
    //  3-5   3     Distance       (uint24 LE, ×0.01 m)
    //  6-7   2     Pace           (uint16 LE, s/500m)
    //  8     1     Stroke Rate    (uint8,    SPM)
    //  9     1     Heart Rate     (uint8,    BPM) — 0 if no HR belt
    // 10-11  2     Current Pace   (uint16 LE)
    // 12-13  2     Average Pace   (uint16 LE)
    // 14-15  2     Rest Distance  (uint16 LE)
    // 16-17  2     Rest Time      (uint16 LE)
    // 18     1     Workout State  (see Concept2 spec)
    // 19     1     Interval Type  (see Concept2 spec)

    static void OnGeneralStatus(IBuffer buffer, BridgeState state)
        => OnGeneralStatusBytes(ReadBuffer(buffer), state);

    static void OnGeneralStatusBytes(byte[] data, BridgeState state)
    {
        if (data.Length < 10) return;

        float elapsedSec = ((data[0]) | (data[1] << 8) | (data[2] << 16)) * 0.01f;
        int   heartRate  = data[9];  // BPM; 0 when no HR belt

        state.ElapsedSec = elapsedSec;
        state.HeartRate  = heartRate;

        EnqueueStatus(state);
    }

    // CE060080 — Multiplexed (primary channel used by pm5-base / ErgometerJS)
    //   byte[0]    = characteristic selector (0x31=GeneralStatus, 0x35=StrokeData, etc.)
    //   bytes[1..] = payload identical to the named characteristic
    static void OnMultiplexedData(IBuffer buffer, BridgeState state)
    {
        byte[] data = ReadBuffer(buffer);
        if (data.Length < 2) return;

        byte selector = data[0];
        byte[] payload = new byte[data.Length - 1];
        Array.Copy(data, 1, payload, 0, payload.Length);

        switch (selector)
        {
            case 0x35: OnStrokeDataBytes(payload, state);    break;  // CE060035
            case 0x31: OnGeneralStatusBytes(payload, state); break;  // CE060031
            // 0x32=AdditionalStatus1, 0x33=AdditionalStatus2, etc. — ignored for now
        }
    }

    // ── StrengthERG Parsers ──────────────────────────────────────────────────
    //
    // CE060065 — StrengthERG Rep Data (analogous to CE060035 on RowErg)
    // Byte layout mirrors CE060035; we log raw hex first to verify.
    //
    //  Byte  Size  Description (best-guess based on CE060035 offset pattern)
    //  0     1     Stroke State (0=Idle, 1=Accel, 2=Drive, 3=Dwell, 4=Recovery)
    //  1-3   3     Elapsed Time   (uint24 LE, ×0.01 s)
    //  4     1     Drive Length   (uint8,     ×0.01 m = cm)
    //  5     1     Drive Time     (uint8,     ×0.01 s)
    //  6-7   2     Stroke Distance(uint16 LE, ×0.01 m)
    //  8-9   2     Peak Force     (int16  LE, N)
    // 10-11  2     Avg Force      (int16  LE, N)
    // 12-15  4     Work Per Rep   (uint32 LE, ×0.001 J)
    // 16-17  2     Rep Count      (uint16 LE)
    // 18     1     Rep Rate       (uint8,    reps/min)
    // 19     1     (reserved)

    static void OnStrengthRepData(IBuffer buffer, BridgeState state)
    {
        byte[] data = ReadBuffer(buffer);
        // Log raw bytes every time so we can verify the byte layout
        Console.WriteLine($"[STRENGTH] CE060065 raw[{data.Length}]: {BitConverter.ToString(data)}");
        OnStrengthRepDataBytes(data, state);
    }

    static void OnStrengthRepDataBytes(byte[] data, BridgeState state)
    {
        if (data.Length < 18) return;

        int strokeState = data[0];
        // Only process completed-rep states (Dwell=3, Recovery=4)
        if (strokeState == 0 || strokeState == 1) return;

        float elapsedSec   = ((data[1]) | (data[2] << 8) | (data[3] << 16)) * 0.01f;
        int   driveLength  = data[4];           // cm
        float driveTimeSec = data[5] * 0.01f;   // seconds
        int   repCount     = data[16] | (data[17] << 8);

        if (repCount <= 0 || driveTimeSec <= 0f) return;

        state.ElapsedSec = elapsedSec;

        string json = $"{{\"type\":\"rep\",\"repCount\":{repCount}," +
                      $"\"driveTimeSec\":{driveTimeSec:F3}," +
                      $"\"pullDistance\":{driveLength}," +
                      $"\"heartRate\":{state.HeartRate}," +
                      $"\"elapsedSec\":{elapsedSec:F2}}}";

        state.OutboundMessages.Enqueue(json);
        Console.WriteLine($"[STRENGTH] Rep #{repCount}: time={driveTimeSec:F2}s dist={driveLength}cm elapsed={elapsedSec:F1}s");
    }

    // CE060061 — StrengthERG General Status (~1 Hz, analogous to CE060031)
    static void OnStrengthGenStatus(IBuffer buffer, BridgeState state)
        => OnStrengthGenStatusBytes(ReadBuffer(buffer), state);

    static void OnStrengthGenStatusBytes(byte[] data, BridgeState state)
    {
        Console.WriteLine($"[STRENGTH] CE060061 raw[{data.Length}]: {BitConverter.ToString(data)}");
        if (data.Length < 10) return;

        float elapsedSec = ((data[0]) | (data[1] << 8) | (data[2] << 16)) * 0.01f;
        int   heartRate  = data[9];

        state.ElapsedSec = elapsedSec;
        state.HeartRate  = heartRate;
        EnqueueStatus(state);
    }

    // ── CSAFE over BLE Helpers ──────────────────────────────────────────────

    /// <summary>
    /// Write a CSAFE frame to the TX characteristic.
    /// Automatically wraps commands in F1 ... checksum F2.
    /// </summary>
    static async Task WriteCsafeAsync(GattCharacteristic txChar, byte[] commands)
    {
        byte checksum = 0;
        foreach (byte b in commands) checksum ^= b;

        // Build frame: F1 <commands> <checksum> F2
        byte[] frame = new byte[commands.Length + 3];
        frame[0] = 0xF1;
        Array.Copy(commands, 0, frame, 1, commands.Length);
        frame[frame.Length - 2] = checksum;
        frame[frame.Length - 1] = 0xF2;

        var writer = new DataWriter();
        writer.WriteBytes(frame);
        var status = await txChar.WriteValueAsync(writer.DetachBuffer());
        if (status != GattCommunicationStatus.Success)
            Console.WriteLine($"[CSAFE] Write failed ({status}): {BitConverter.ToString(frame)}");
    }

    /// <summary>
    /// Extract a standard CSAFE public command response (like 0xA0, 0xB0).
    /// Looks for the command ID in the response frame and returns its data bytes.
    /// </summary>
    static byte[] ExtractPublicCmdData(byte[] buf, byte cmd)
    {
        int start = -1, stop = -1;
        for (int i = 0; i < buf.Length; i++)
        {
            if (buf[i] == 0xF1 && start < 0) start = i;
            if (buf[i] == 0xF2 && start >= 0) { stop = i; break; }
        }
        if (start < 0 || stop <= start + 2) return null;

        // Skip start flag + status byte
        int pos = start + 2;
        while (pos < stop - 1)
        {
            byte cmdId = buf[pos++];
            if (pos >= stop) break;
            byte dataLen = buf[pos++];
            if (cmdId == cmd && dataLen > 0 && pos + dataLen <= stop)
            {
                byte[] data = new byte[dataLen];
                Array.Copy(buf, pos, data, 0, dataLen);
                return data;
            }
            pos += dataLen;
        }
        return null;
    }

    /// <summary>
    /// Extract a proprietary CSAFE sub-command response (wrapped in 0x1A).
    /// Used for PM5-specific commands like STROKESTATS (0x6E).
    /// </summary>
    static byte[] ExtractProprietaryData(byte[] buf, byte subCmd)
    {
        int start = -1;
        for (int i = 0; i < buf.Length; i++) { if (buf[i] == 0xF1) { start = i; break; } }
        if (start < 0 || start + 4 >= buf.Length) return null;

        int pos = start + 2;
        int limit = Math.Min(buf.Length, start + 60);
        while (pos < limit)
        {
            if (buf[pos] == 0x1A)   // wrapper command
            {
                pos++;
                if (pos >= limit) break;
                int wrapperLen = buf[pos++];
                if (wrapperLen <= 0 || pos >= limit) break;
                int wrapperEnd = pos + wrapperLen;
                if (wrapperEnd > limit) wrapperEnd = limit;
                while (pos < wrapperEnd)
                {
                    byte sc = buf[pos++];
                    if (pos >= wrapperEnd) break;
                    byte dl = buf[pos++];
                    if (sc == subCmd && dl > 0 && pos + dl <= buf.Length)
                    {
                        byte[] data = new byte[dl];
                        Array.Copy(buf, pos, data, 0, dl);
                        return data;
                    }
                    pos += dl;
                }
                break;
            }
            pos++;
        }
        return null;
    }

    static void EnqueueStatus(BridgeState state)
    {
        string connStr = state.IsConnected ? "true" : "false";
        string json    = $"{{\"type\":\"status\",\"connected\":{connStr}," +
                         $"\"heartRate\":{state.HeartRate}," +
                         $"\"elapsedSec\":{state.ElapsedSec:F2}," +
                         $"\"statusText\":\"{EscapeJson(state.StatusText)}\"}}";
        state.OutboundMessages.Enqueue(json);
    }

    // ── Helpers ─────────────────────────────────────────────────────────────

    static byte[] ReadBuffer(IBuffer buffer)
    {
        using var reader = DataReader.FromBuffer(buffer);
        byte[] data = new byte[buffer.Length];
        reader.ReadBytes(data);
        return data;
    }

    static string AddressFilePath =>
        System.IO.Path.Combine(AppContext.BaseDirectory, "pm5_address.txt");

    static void SaveKnownAddress(ulong addr, BluetoothAddressType addrType)
    {
        try
        {
            // Format: "AABBCCDDEEFF,1"  where the second field is (int)BluetoothAddressType
            string text = $"{addr:X12},{(int)addrType}";
            System.IO.File.WriteAllText(AddressFilePath, text);
            Console.WriteLine($"[BLE] Saved PM5 address to pm5_address.txt (0x{addr:X12} type={addrType})");
        }
        catch (Exception ex) { Console.WriteLine($"[BLE] Could not save address: {ex.Message}"); }
    }

    static bool TryLoadKnownAddress(out ulong addr, out BluetoothAddressType addrType)
    {
        addr = 0;
        addrType = BluetoothAddressType.Random;   // safe default for PM5
        try
        {
            if (!System.IO.File.Exists(AddressFilePath)) return false;
            string text = System.IO.File.ReadAllText(AddressFilePath).Trim();

            // Support old format (just hex address) and new format ("hex,type")
            string[] parts = text.Split(',');
            if (!ulong.TryParse(parts[0], System.Globalization.NumberStyles.HexNumber, null, out addr))
                return false;

            if (parts.Length >= 2 && int.TryParse(parts[1], out int typeInt))
                addrType = (BluetoothAddressType)typeInt;

            return true;
        }
        catch { }
        return false;
    }

    static string EscapeJson(string s) =>
        s.Replace("\\", "\\\\").Replace("\"", "\\\"");
}
