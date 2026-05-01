// PM5BleDevice.cs — BLE GATT connection and data streaming for one Concept2 PM5.
//
// ?? Service architecture (from working ERGBridgeBLE, commit 68cb4cf) ?????????
//
// CE060000  DiscoveryService — advertisement UUID only, NOT a data path.
//
// RowErg / BikeErg / SkiErg  (CE060030 base):
//   CE060030  RowingService    main data service (post-connect only)
//   CE060080  Multiplexed      preferred notify; byte[0]=selector 0x20=GenStatus 0x35=StrokeData
//   CE060035  StrokeData       per-stroke notify, fires once per completed rep
//   CE060031  GeneralStatus    1 Hz: elapsed, pace, SPM, heart rate
//
// StrengthErg  (advertised name contains "STR"):
//   CE060060  StrengthService  passive ERG Link data — silent to external apps
//   CE060020  CtrlService      CSAFE request/response control service
//   CE060021  CtrlTx           write CSAFE frames here
//   CE060022  CtrlRx           subscribe for CSAFE responses
//
// ?? CE060035 StrokeData byte layout (20 bytes) ???????????????????????????????
//   [0]     stroke state  0=idle 1=accel 2=drive 3=dwell 4=recovery
//   [1-3]   elapsed       uint24-LE * 0.01 s
//   [4]     drive length  uint8 * 0.01 m (cm) — pullDistance equivalent
//   [5]     drive time    uint8 * 0.01 s
//   [12-15] work/stroke   uint32-LE * 0.001 J
//   [16-17] stroke count  uint16-LE
//   [18]    stroke rate   uint8 SPM
//
// ?? CE060031 GeneralStatus byte layout (20 bytes) ????????????????????????????
//   [0-2]   elapsed       uint24-LE * 0.01 s
//   [6-7]   pace          uint16-LE s/500 m  (0 when idle)
//   [8]     stroke rate   uint8 SPM
//   [9]     heart rate    uint8 BPM  (0 = no HR belt)

using System.Collections.Concurrent;
using Windows.Devices.Bluetooth;
using Windows.Devices.Bluetooth.GenericAttributeProfile;
using Windows.Security.Cryptography;
using Windows.Storage.Streams;

namespace PM5BleBridge;

internal class PM5BleDevice
{
    // ?? UUIDs ?????????????????????????????????????????????????????????????????
    // CE060000 = discovery/advertisement only — NOT a data service.

    // RowErg / BikeErg / SkiErg (CE060030 base)
    static readonly Guid UUID_RowingSvc  = new("CE060030-43E5-11E4-916C-0800200C9A66");
    static readonly Guid UUID_Mux        = new("CE060080-43E5-11E4-916C-0800200C9A66");
    static readonly Guid UUID_StrokeData = new("CE060035-43E5-11E4-916C-0800200C9A66");
    static readonly Guid UUID_GenStatus  = new("CE060031-43E5-11E4-916C-0800200C9A66");

    // StrengthErg passive data (CE060060) — for detection/enumeration only
    static readonly Guid UUID_StrSvc     = new("CE060060-43E5-11E4-916C-0800200C9A66");
    static readonly Guid UUID_StrGenStat = new("CE060061-43E5-11E4-916C-0800200C9A66");
    static readonly Guid UUID_StrRepData = new("CE060065-43E5-11E4-916C-0800200C9A66");

    // StrengthErg CSAFE control (CE060020) — active request/response
    static readonly Guid UUID_CtrlSvc = new("CE060020-43E5-11E4-916C-0800200C9A66");
    static readonly Guid UUID_CtrlTx  = new("CE060021-43E5-11E4-916C-0800200C9A66");
    static readonly Guid UUID_CtrlRx  = new("CE060022-43E5-11E4-916C-0800200C9A66");

    // ?? Identity ??????????????????????????????????????????????????????????????
    public  string        DeviceName { get; }
    public  ChannelServer Channel    { get; }
    private readonly ulong _address;

    // ?? Shared state (volatile — BLE thread pool writes, send path reads) ?????
    private volatile int   _hr      = 0;
    private volatile float _elapsed = 0f;
    private volatile float _pace    = 0f;
    private volatile float _spm     = 0f;
    private volatile float _power   = 0f;
    private volatile int   _lastRep = -1;

    private readonly ConcurrentQueue<byte[]> _csafeRx = new();
    private bool _stop;

    public PM5BleDevice(string deviceName, ulong address, ChannelServer channel)
    {
        DeviceName = deviceName;
        _address   = address;
        Channel    = channel;
    }

    // ?? Entry point ???????????????????????????????????????????????????????????
    public async Task RunAsync(CancellationToken ct)
    {
        Console.WriteLine(
            $"[{Channel.ChannelName}] Connecting to \"{DeviceName}\" (0x{_address:X12})...");
        BluetoothLEDevice? dev = null;
        try
        {
            dev = await BluetoothLEDevice.FromBluetoothAddressAsync(_address);
            if (dev == null)
            {
                Console.WriteLine($"[{Channel.ChannelName}] FromBluetoothAddress returned null.");
                return;
            }
            Console.WriteLine($"[{Channel.ChannelName}] Device: \"{dev.Name}\"");
            await Task.Delay(100, ct);

            bool isStr = DeviceName.IndexOf("STR", StringComparison.OrdinalIgnoreCase) >= 0;
            Console.WriteLine($"[{Channel.ChannelName}] Path: " +
                (isStr ? "StrengthErg => CE060020 CSAFE"
                       : "RowErg/BikeErg/SkiErg => CE060030 notifications"));

            if (isStr) await RunStrengthAsync(dev, ct);
            else       await RunRowingAsync(dev, ct);
        }
        catch (OperationCanceledException) { }
        catch (Exception ex) when (!_stop)
        {
            Console.WriteLine($"[{Channel.ChannelName}] Session error: {ex.Message}");
        }
        finally
        {
            dev?.Dispose();
            Console.WriteLine($"[{Channel.ChannelName}] Session ended for \"{DeviceName}\".");
        }
    }

    public void Stop() => _stop = true;

    // ?? StrengthErg: CE060020 CSAFE request / response ???????????????????????
    private async Task RunStrengthAsync(BluetoothLEDevice dev, CancellationToken ct)
    {
        Console.WriteLine($"[{Channel.ChannelName}] Looking up CE060020 (CSAFE control)...");
        GattDeviceService ctrl;
        try { ctrl = await SvcAsync(dev, UUID_CtrlSvc, "CE060020"); }
        catch (Exception ex)
        {
            Console.WriteLine($"[{Channel.ChannelName}] CE060020 not found: {ex.Message}");
            await DumpAsync(dev);
            return;
        }

        using (ctrl)
        {
            GattCharacteristic tx, rx;
            try
            {
                tx = await CharAsync(ctrl, UUID_CtrlTx, "CE060021 TX");
                rx = await CharAsync(ctrl, UUID_CtrlRx, "CE060022 RX");
            }
            catch (Exception ex)
            {
                Console.WriteLine($"[{Channel.ChannelName}] CSAFE chars missing: {ex.Message}");
                await DumpAsync(dev);
                return;
            }

            rx.ValueChanged += (_, a) =>
            {
                CryptographicBuffer.CopyToByteArray(a.CharacteristicValue, out byte[] raw);
                if (raw?.Length > 0)
                {
                    Console.WriteLine(
                        $"[{Channel.ChannelName}] CSAFE RX [{raw.Length}]: " +
                        BitConverter.ToString(raw));
                    _csafeRx.Enqueue(raw);
                }
            };
            await NotifyAsync(rx, "CE060022");
            Console.WriteLine($"[{Channel.ChannelName}] CE060022 subscribed. Polling...");
            EmitStatus(true, $"CSAFE connected — {DeviceName}");
            await StrPollLoop(dev, tx, ct);
        }
    }

    private async Task StrPollLoop(
        BluetoothLEDevice dev, GattCharacteristic tx, CancellationToken ct)
    {
        int lastRep = -1;
        while (!ct.IsCancellationRequested && !_stop
               && dev.ConnectionStatus == BluetoothConnectionStatus.Connected)
        {
            try
            {
                await WriteCsafe(tx, CsafeHelper.BuildFrame(0x1A, 0x02, 0x6E, 0x00));
                await Task.Delay(200, ct);
                DrainCsafe(ref lastRep);

                await WriteCsafe(tx, CsafeHelper.BuildFrame(0xA0, 0xB0));
                await Task.Delay(200, ct);
                DrainCsafe(ref lastRep);

                EmitStatus(true, $"CSAFE Active — {DeviceName}",
                    repCount: Math.Max(0, lastRep));
                await Task.Delay(100, ct);
            }
            catch (OperationCanceledException) { break; }
            catch (Exception ex)
            {
                Console.WriteLine($"[{Channel.ChannelName}] CSAFE poll error: {ex.Message}");
                await Task.Delay(500, ct);
            }
        }
    }

    private void DrainCsafe(ref int lastRep)
    {
        while (_csafeRx.TryDequeue(out byte[]? r))
        {
            var wt = CsafeHelper.ExtractPublicCmd(r, CsafeHelper.CMD_WORK);
            if (wt?.Length >= 3)
            {
                _elapsed = wt[0] * 3600f + wt[1] * 60f + wt[2];
                Console.WriteLine(
                    $"[{Channel.ChannelName}] CSAFE GETTWORK ? {wt[0]}h {wt[1]}m {wt[2]}s " +
                    $"= {_elapsed:F0}s elapsed");
            }

            var hr = CsafeHelper.ExtractPublicCmd(r, CsafeHelper.CMD_HR);
            if (hr?.Length >= 1)
            {
                _hr = hr[0];
                Console.WriteLine($"[{Channel.ChannelName}] CSAFE GETHRCUR ? {_hr} BPM");
            }

            var ss = CsafeHelper.ExtractProprietarySubCmd(r, CsafeHelper.SUB_STROKE);
            if (ss != null)
            {
                Console.WriteLine(
                    $"[{Channel.ChannelName}] CSAFE STROKESTATS raw [{ss.Length}]: " +
                    BitConverter.ToString(ss));
            }
            if (ss?.Length >= 7)
            {
                float dt = ss[2] * 0.01f;
                int   pd = ss[5];
                int   rc = ss[6];
                Console.WriteLine(
                    $"[{Channel.ChannelName}] CSAFE STROKESTATS ? dt={dt:F2}s pd={pd}cm rc={rc} " +
                    $"(lastRep={lastRep})");
                if (rc > 0 && rc > lastRep)
                {
                    lastRep = rc;
                    _lastRep = rc;
                    string json =
                        $"{{\"type\":\"rep\",\"repCount\":{rc}," +
                        $"\"driveTimeSec\":{dt:F3},\"pullDistance\":{pd}," +
                        $"\"heartRate\":{_hr},\"elapsedSec\":{_elapsed:F2}," +
                        $"\"connected\":true}}";
                    Console.WriteLine(
                        $"[{Channel.ChannelName}] EMIT rep#{rc} ? {json}");
                    Channel.Send(json);
                }
            }
        }
    }

    // ?? RowErg / BikeErg / SkiErg: CE060030 notification path ???????????????
    private async Task RunRowingAsync(BluetoothLEDevice dev, CancellationToken ct)
    {
        Console.WriteLine($"[{Channel.ChannelName}] Looking up CE060030...");
        GattDeviceService? svc = null;
        try { svc = await SvcAsync(dev, UUID_RowingSvc, "CE060030"); }
        catch (Exception ex)
        {
            Console.WriteLine($"[{Channel.ChannelName}] CE060030 direct lookup failed: {ex.Message}");
        }

        if (svc == null)
        {
            Console.WriteLine($"[{Channel.ChannelName}] Falling back to service enumeration...");
            svc = await FindRowingSvcAsync(dev);
        }

        if (svc == null)
        {
            Console.WriteLine(
                $"[{Channel.ChannelName}] No rowing service found. " +
                "Start a workout on the PM5 screen, then retry.");
            await DumpAsync(dev);
            return;
        }

        using (svc)
        {
            Console.WriteLine($"[{Channel.ChannelName}] Using service: {svc.Uuid}");
            bool ok = false;

            // 1. CE060080 Multiplexed (preferred)
            try
            {
                await Task.Delay(200, ct);
                var mux = await CharAsync(svc, UUID_Mux, "CE060080 Multiplexed");
                mux.ValueChanged += (_, a) =>
                {
                    CryptographicBuffer.CopyToByteArray(a.CharacteristicValue, out byte[] d);
                    if (d != null) OnMux(d);
                };
                await NotifyAsync(mux, "CE060080");
                Console.WriteLine($"[{Channel.ChannelName}] Subscribed CE060080 Multiplexed (primary).");
                ok = true;
            }
            catch (Exception ex)
            {
                Console.WriteLine(
                    $"[{Channel.ChannelName}] CE060080 unavailable: {ex.Message} — trying direct chars.");
            }

            // 2. CE060035 StrokeData (per-rep fallback)
            try
            {
                await Task.Delay(200, ct);
                var sc = await CharAsync(svc, UUID_StrokeData, "CE060035 StrokeData");
                sc.ValueChanged += (_, a) =>
                {
                    CryptographicBuffer.CopyToByteArray(a.CharacteristicValue, out byte[] d);
                    if (d != null) OnStroke(d);
                };
                await NotifyAsync(sc, "CE060035");
                Console.WriteLine($"[{Channel.ChannelName}] Subscribed CE060035 StrokeData.");
                ok = true;
            }
            catch (Exception ex)
            {
                Console.WriteLine($"[{Channel.ChannelName}] CE060035 unavailable: {ex.Message}");
            }

            // 3. CE060031 GeneralStatus (1 Hz)
            try
            {
                await Task.Delay(200, ct);
                var gs = await CharAsync(svc, UUID_GenStatus, "CE060031 GeneralStatus");
                gs.ValueChanged += (_, a) =>
                {
                    CryptographicBuffer.CopyToByteArray(a.CharacteristicValue, out byte[] d);
                    if (d != null) OnGenStatus(d);
                };
                await NotifyAsync(gs, "CE060031");
                Console.WriteLine($"[{Channel.ChannelName}] Subscribed CE060031 GeneralStatus (1 Hz).");
                ok = true;
            }
            catch (Exception ex)
            {
                Console.WriteLine($"[{Channel.ChannelName}] CE060031 unavailable: {ex.Message}");
            }

            if (!ok)
            {
                Console.WriteLine($"[{Channel.ChannelName}] No characteristics subscribed.");
                await DumpAsync(dev);
                return;
            }

            Console.WriteLine($"[{Channel.ChannelName}] Ready — waiting for stroke notifications.");
            EmitStatus(true, $"BLE notifications active — {DeviceName}");
            await DisconnectWait(dev, ct);
        }
    }

    // ?? Notification handlers ?????????????????????????????????????????????????

    private void OnMux(byte[] d)
    {
        if (d.Length < 2) return;
        byte   sel   = d[0];
        byte[] inner = d[1..];
        string label = CsafeHelper.MuxSelectorLabel(sel);

        Console.WriteLine(
            $"[{Channel.ChannelName}] CE060080 mux sel=0x{sel:X2} ({label}) len={d.Length}: " +
            BitConverter.ToString(d, 0, Math.Min(d.Length, 6)));

        // Route selectors whose byte layouts we know well enough to improve live telemetry.
        // Direct CE060031/CE060035 subscriptions remain authoritative when present, but some
        // PM5 firmwares expose more useful live cadence/power hints through 0x32/0x33.
        if      (sel == 0x31 && inner.Length >= 10) OnGenStatus(inner);
        else if (sel == 0x32) OnAdditionalStatus1(inner);
        else if (sel == 0x33) OnAdditionalStatus2(inner);
        else if (sel == 0x35 && inner.Length >= 17) OnStroke(inner);
    }

    private void OnStroke(byte[] d)
    {
        if (d.Length < 18) return;
        if (d[0] == 0 || d[0] == 1) return;    // idle / accel only — not a completed rep

        float elapsed = (d[1] | (d[2] << 8) | (d[3] << 16)) * 0.01f;
        int   len     = d[4];                   // cm; pullDistance equivalent
        float dt      = d[5] * 0.01f;
        int   cnt     = d[16] | (d[17] << 8);
        int   spm     = d.Length >= 19 ? d[18] : 0;

        float pwr = 0f;
        if (d.Length >= 16 && dt > 0f)
        {
            uint w = (uint)(d[12] | (d[13] << 8) | (d[14] << 16) | (d[15] << 24));
            pwr = w * 0.001f / dt;
        }

        if (cnt <= 0 || dt <= 0f)
        {
            Console.WriteLine($"[{Channel.ChannelName}] OnStroke ignored invalid cnt={cnt} dt={dt:F3}");
            return;
        }

        _elapsed = elapsed;
        if (spm > 0) _spm = spm;
        if (pwr > 0f) { _power = pwr; _pace = CsafeHelper.ComputeRowingPace(pwr); }

        if (cnt > _lastRep)
        {
            _lastRep = cnt;
            string json =
                $"{{\"type\":\"rep\",\"repCount\":{cnt}," +
                $"\"driveTimeSec\":{dt:F3},\"pullDistance\":{len}," +
                $"\"heartRate\":{_hr},\"elapsedSec\":{elapsed:F2}," +
                $"\"connected\":true}}";
            Console.WriteLine(
                $"[{Channel.ChannelName}] EMIT stroke#{cnt}: " +
                $"dt={dt:F2}s len={len}cm spm={spm} pwr={pwr:F0}W ? {json}");
            Channel.Send(json);
        }
    }

    private void OnAdditionalStatus1(byte[] d)
    {
        if (d.Length < 5) return;

        // Empirical decoding from live logs:
        // bytes[0..2] change rapidly during real activity and appear to encode pace/workload.
        // bytes[3..4] track a larger little-endian metric that is stable at idle and shifts
        // under effort. We use this as a fallback signal only when GeneralStatus is stuck.
        int metricA = d[0] | (d[1] << 8) | (d[2] << 16);
        int metricB = d[3] | (d[4] << 8);

        // Ignore the known idle baseline frames that repeat forever at rest.
        bool bLooksIdleBaseline = (_spm <= 1f && _power <= 21.5f && metricA > 0 && metricB > 0);
        if (!bLooksIdleBaseline)
        {
            Console.WriteLine($"[{Channel.ChannelName}] AdditionalStatus1 metricA={metricA} metricB={metricB}");
        }
    }

    private void OnAdditionalStatus2(byte[] d)
    {
        if (d.Length < 2) return;

        // Selector 0x33 appears to carry another live movement metric on PM5 row/bike units.
        // When it rises above the idle 0/2/3-ish floor, treat it as evidence of real activity
        // and keep the stream from collapsing back to the sticky 1 spm / 21 W baseline.
        int liveHint = d[0] | (d[1] << 8);
        if (liveHint > 3 && _spm < 5f)
        {
            _spm = 5f;
            if (_power < 30f) _power = 30f;
            Console.WriteLine($"[{Channel.ChannelName}] AdditionalStatus2 promoted live activity hint={liveHint}");
            EmitStatus(true, $"BLE Active hint {liveHint} - {DeviceName}");
        }
    }

    private void OnGenStatus(byte[] d)
    {
        if (d.Length < 10)
        {
            Console.WriteLine($"[{Channel.ChannelName}] OnGenStatus ignored short payload len={d.Length}");
            return;
        }
        float elapsed = (d[0] | (d[1] << 8) | (d[2] << 16)) * 0.01f;
        int   paceSec = d[6] | (d[7] << 8);
        int   spm     = d[8];
        int   hr      = d[9];

        _elapsed = elapsed;
        _hr      = hr;

        // Always assign spm so that a device returning to idle reports 0, not its last active rate.
        _spm = spm;

        if (paceSec > 0)
        {
            _pace  = paceSec;
            // Derive power from pace whenever we have a valid pace reading.
            _power = (float)(2.8 * Math.Pow(500.0 / paceSec, 3.0));
        }
        else if (spm == 0)
        {
            // Truly idle (no pace, no stroke rate): clear streaming fields so stale
            // values from a previous workout do not drive lane movement in Unreal.
            _pace  = 0f;
            _power = 0f;
        }
        // spm > 0 but paceSec == 0: mid-stroke burst with no split yet — keep last power.

        EmitStatus(true, $"BLE Active — {DeviceName}");
    }

    // ?? JSON helpers ??????????????????????????????????????????????????????????
    private void EmitStatus(bool connected, string text = "", int repCount = -1)
    {
        int rc = repCount >= 0 ? repCount : Math.Max(0, _lastRep);
        string json =
            $"{{\"type\":\"status\"," +
            $"\"connected\":{(connected ? "true" : "false")}," +
            $"\"strokeRate\":{_spm:F1}," +
            $"\"powerWatts\":{_power:F1}," +
            $"\"paceSec500m\":{_pace:F1}," +
            $"\"heartRate\":{_hr}," +
            $"\"elapsedSec\":{_elapsed:F1}," +
            $"\"repCount\":{rc}," +
            $"\"driveTimeSec\":0," +
            $"\"pullDistance\":0," +
            $"\"statusText\":\"{Esc(text)}\"}}";
        Console.WriteLine(
            $"[{Channel.ChannelName}] EMIT status spm={_spm:F0} pwr={_power:F0}W " +
            $"hr={_hr} el={_elapsed:F0}s rc={rc}");
        Channel.Send(json);
    }

    // ?? CSAFE write ???????????????????????????????????????????????????????????
    private static async Task WriteCsafe(GattCharacteristic tx, byte[] frame)
    {
        using var w = new DataWriter();
        w.WriteBytes(frame);
        var s = await tx.WriteValueAsync(w.DetachBuffer());
        if (s != GattCommunicationStatus.Success)
            Console.WriteLine($"[CSAFE] Write failed ({s}): {BitConverter.ToString(frame)}");
    }

    // ?? GATT helpers ??????????????????????????????????????????????????????????
    private static async Task<GattDeviceService> SvcAsync(
        BluetoothLEDevice dev, Guid uuid, string label)
    {
        var r = await dev.GetGattServicesForUuidAsync(uuid, BluetoothCacheMode.Uncached);
        if (r.Status != GattCommunicationStatus.Success || r.Services.Count == 0)
            throw new InvalidOperationException(
                $"{label} not found (status={r.Status} count={r.Services.Count})");
        Console.WriteLine($"  [GATT] Service: {label} ({uuid})");
        return r.Services[0];
    }

    private static async Task<GattCharacteristic> CharAsync(
        GattDeviceService svc, Guid uuid, string label)
    {
        var r = await svc.GetCharacteristicsForUuidAsync(uuid, BluetoothCacheMode.Uncached);
        if (r.Status != GattCommunicationStatus.Success || r.Characteristics.Count == 0)
            throw new InvalidOperationException($"{label} not found in {svc.Uuid}");
        Console.WriteLine($"  [GATT] Char: {label} ({uuid})");
        return r.Characteristics[0];
    }

    private static async Task NotifyAsync(GattCharacteristic ch, string label)
    {
        var s = await ch.WriteClientCharacteristicConfigurationDescriptorAsync(
            GattClientCharacteristicConfigurationDescriptorValue.Notify);
        if (s != GattCommunicationStatus.Success)
            throw new InvalidOperationException(
                $"Notify subscribe failed {label} ({ch.Uuid}): {s}");
    }

    // ?? Fallback: find rowing service by enumeration ??????????????????????????
    private async Task<GattDeviceService?> FindRowingSvcAsync(BluetoothLEDevice dev)
    {
        Console.WriteLine($"[{Channel.ChannelName}] Enumerating all services (fallback)...");
        GattDeviceService? found = null;
        try
        {
            var all = await dev.GetGattServicesAsync(BluetoothCacheMode.Uncached);
            Console.WriteLine($"[{Channel.ChannelName}] {all.Services.Count} service(s):");
            foreach (var svc in all.Services)
            {
                Console.WriteLine($"  [GATT] Service: {svc.Uuid}");
                if (found != null) { svc.Dispose(); continue; }

                if (svc.Uuid == UUID_RowingSvc || svc.Uuid == UUID_StrSvc)
                {
                    Console.WriteLine($"  [GATT] *** Matched C2 service: {svc.Uuid}");
                    found = svc;
                    continue;
                }

                var chars = await svc.GetCharacteristicsAsync(BluetoothCacheMode.Uncached);
                bool hit  = false;
                foreach (var c in chars.Characteristics)
                {
                    Console.WriteLine($"         Char: {c.Uuid}");
                    if (c.Uuid == UUID_StrokeData || c.Uuid == UUID_Mux    ||
                        c.Uuid == UUID_GenStatus  || c.Uuid == UUID_StrRepData ||
                        c.Uuid == UUID_StrGenStat)
                    {
                        Console.WriteLine($"  [GATT] *** C2 char {c.Uuid} in svc {svc.Uuid}");
                        found = svc; hit = true; break;
                    }
                }
                if (!hit) svc.Dispose();
            }
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[{Channel.ChannelName}] Enumeration error: {ex.Message}");
        }
        return found;
    }

    // ?? Full diagnostic GATT dump ?????????????????????????????????????????????
    private async Task DumpAsync(BluetoothLEDevice dev)
    {
        Console.WriteLine($"[{Channel.ChannelName}] --- Full GATT dump ---");
        try
        {
            var all = await dev.GetGattServicesAsync(BluetoothCacheMode.Uncached);
            Console.WriteLine($"[{Channel.ChannelName}] Services: {all.Services.Count}");
            foreach (var svc in all.Services)
            {
                Console.WriteLine($"  Svc: {svc.Uuid}");
                try
                {
                    var ch = await svc.GetCharacteristicsAsync(BluetoothCacheMode.Uncached);
                    foreach (var c in ch.Characteristics)
                        Console.WriteLine(
                            $"    Char: {c.Uuid}  props={c.CharacteristicProperties}");
                }
                catch (Exception ex) { Console.WriteLine($"    (failed: {ex.Message})"); }
                svc.Dispose();
            }
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[{Channel.ChannelName}] Dump failed: {ex.Message}");
        }
        Console.WriteLine($"[{Channel.ChannelName}] --- End GATT dump ---");
    }

    // ?? Connection monitor ????????????????????????????????????????????????????
    private static Task DisconnectWait(BluetoothLEDevice dev, CancellationToken ct)
    {
        var tcs = new TaskCompletionSource<bool>();
        dev.ConnectionStatusChanged += (d, _) =>
        {
            if (d.ConnectionStatus == BluetoothConnectionStatus.Disconnected)
                tcs.TrySetResult(true);
        };
        ct.Register(() => tcs.TrySetResult(false));
        return tcs.Task;
    }

    private static string Esc(string s) =>
        s.Replace("\\", "\\\\").Replace("\"", "\\\"");
}
