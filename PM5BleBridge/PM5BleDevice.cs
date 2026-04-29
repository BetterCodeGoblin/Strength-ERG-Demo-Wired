// PM5BleDevice.cs
// Manages a single BLE GATT connection to a Concept2 PM5 device and
// streams CSAFE telemetry to a ChannelServer TCP slot.
//
// Protocol notes:
//   All Concept2 PM5 devices (RowErg, BikeErg, StrengthErg) share identical
//   GATT service/characteristic UUIDs and CSAFE framing.  What differs is
//   which CSAFE fields carry meaningful data for each workout type:
//
//     RowErg/BikeErg  ?  GETCADENCE (stroke rate / cadence)
//                         GETPOWER   (watts)
//                         GETHRCUR   (HR)
//                         GETTWORK   (elapsed)
//
//     StrengthErg     ?  WRAPPER + STROKESTATS (rep count, drive time, pull dist)
//                         GETHRCUR   (HR)
//                         GETTWORK   (elapsed)
//
//   We poll ALL commands from every device and include all fields in the JSON
//   output.  The UE5 side selects fields by DeviceChannel.  This means if you
//   mis-assign a device to a slot, the debug view will show zeros rather than crash.
//
// BLE GATT UUIDs (Concept2 PM Control Service):
//   Service  : CE060000-43E5-11E4-916C-0800200C9A66
//   RX (write):CE060001-43E5-11E4-916C-0800200C9A66   ? we write CSAFE frames here
//   TX (notify):CE060002-43E5-11E4-916C-0800200C9A66  ? responses arrive here
//
// Request-response synchronisation:
//   We write a CSAFE frame to CE060001 then wait for a notification on CE060002.
//   A SemaphoreSlim prevents concurrent in-flight requests.
//   Partial notifications are accumulated until a complete frame (F1...F2) arrives.

using Windows.Devices.Bluetooth;
using Windows.Devices.Bluetooth.GenericAttributeProfile;
using Windows.Security.Cryptography;

namespace PM5BleBridge;

internal class PM5BleDevice
{
    // ?? PM5 GATT UUIDs ??????????????????????????????????????????????????????
    private static readonly Guid C2_SERVICE  = Guid.Parse("CE060000-43E5-11E4-916C-0800200C9A66");
    private static readonly Guid C2_RX       = Guid.Parse("CE060001-43E5-11E4-916C-0800200C9A66");
    private static readonly Guid C2_TX       = Guid.Parse("CE060002-43E5-11E4-916C-0800200C9A66");

    // ?? Identity ????????????????????????????????????????????????????????????
    public  string DeviceName { get; }
    private readonly ulong _address;

    // ?? Assigned channel ????????????????????????????????????????????????????
    public ChannelServer Channel { get; }

    // ?? BLE handles ?????????????????????????????????????????????????????????
    private BluetoothLEDevice?         _device;
    private GattCharacteristic?        _rx;
    private GattCharacteristic?        _tx;

    // ?? Request-response sync ???????????????????????????????????????????????
    private readonly SemaphoreSlim                    _requestLock = new(1, 1);
    private          TaskCompletionSource<byte[]>?    _pendingResponse;
    private          byte[]                           _responseBuffer = [];

    // ?? State ???????????????????????????????????????????????????????????????
    private int   _lastRepCount = -1;
    private bool  _stopRequested;

    public PM5BleDevice(string deviceName, ulong address, ChannelServer channel)
    {
        DeviceName = deviceName;
        _address   = address;
        Channel    = channel;
    }

    // ?? Connection ??????????????????????????????????????????????????????????

    /// <summary>
    /// Connects to the PM5, subscribes to GATT notifications, and starts the
    /// polling loop.  Returns when the connection is lost; caller should retry.
    /// </summary>
    public async Task RunAsync(CancellationToken ct)
    {
        Console.WriteLine($"[{Channel.ChannelName}] Connecting to {DeviceName}...");

        try
        {
            _device = await BluetoothLEDevice.FromBluetoothAddressAsync(_address);
            if (_device == null)
            {
                Console.WriteLine($"[{Channel.ChannelName}] FromBluetoothAddress returned null for {DeviceName}");
                return;
            }

            // ?? Get C2 service ??????????????????????????????????????????????
            var svcResult = await _device.GetGattServicesForUuidAsync(C2_SERVICE,
                BluetoothCacheMode.Uncached);
            if (svcResult.Status != GattCommunicationStatus.Success ||
                svcResult.Services.Count == 0)
            {
                Console.WriteLine($"[{Channel.ChannelName}] C2 service not found on {DeviceName} " +
                                  $"(status={svcResult.Status})");
                return;
            }
            var service = svcResult.Services[0];

            // ?? Get characteristics ?????????????????????????????????????????
            var rxResult = await service.GetCharacteristicsForUuidAsync(C2_RX,
                BluetoothCacheMode.Uncached);
            var txResult = await service.GetCharacteristicsForUuidAsync(C2_TX,
                BluetoothCacheMode.Uncached);

            if (rxResult.Status != GattCommunicationStatus.Success ||
                rxResult.Characteristics.Count == 0 ||
                txResult.Status != GattCommunicationStatus.Success ||
                txResult.Characteristics.Count == 0)
            {
                Console.WriteLine($"[{Channel.ChannelName}] RX/TX characteristics not found on {DeviceName}");
                return;
            }

            _rx = rxResult.Characteristics[0];
            _tx = txResult.Characteristics[0];

            // ?? Subscribe to TX notifications ???????????????????????????????
            var notifyStatus = await _tx.WriteClientCharacteristicConfigurationDescriptorAsync(
                GattClientCharacteristicConfigurationDescriptorValue.Notify);
            if (notifyStatus != GattCommunicationStatus.Success)
            {
                Console.WriteLine($"[{Channel.ChannelName}] Failed to subscribe to notifications on {DeviceName}");
                return;
            }

            _tx.ValueChanged += OnNotification;

            Console.WriteLine($"[{Channel.ChannelName}] Connected to {DeviceName} — starting poll loop");

            Channel.Send($"{{\"type\":\"status\",\"connected\":false,\"statusText\":\"BLE connected to {DeviceName}, waiting for workout\"}}");

            await PollLoopAsync(ct);
        }
        catch (Exception ex) when (!_stopRequested)
        {
            Console.WriteLine($"[{Channel.ChannelName}] Connection error for {DeviceName}: {ex.Message}");
        }
        finally
        {
            if (_tx != null) _tx.ValueChanged -= OnNotification;
            _device?.Dispose();
            _device = null;
            _rx     = null;
            _tx     = null;
        }
    }

    public void Stop() => _stopRequested = true;

    // ?? BLE notification handler (runs on a BLE thread pool thread) ?????????

    private void OnNotification(GattCharacteristic _, GattValueChangedEventArgs args)
    {
        CryptographicBuffer.CopyToByteArray(args.CharacteristicValue, out byte[] chunk);
        if (chunk == null || chunk.Length == 0) return;

        // Accumulate chunks until we have a complete CSAFE frame (F1 ... F2)
        _responseBuffer = [.. _responseBuffer, .. chunk];

        // Discard garbage that doesn't start with CSAFE_START
        int startIdx = Array.IndexOf(_responseBuffer, CsafeHelper.START);
        if (startIdx < 0) { _responseBuffer = []; return; }
        if (startIdx > 0) _responseBuffer = _responseBuffer[startIdx..];

        // Check for CSAFE_STOP
        int stopIdx = Array.IndexOf(_responseBuffer, CsafeHelper.STOP, 1);
        if (stopIdx <= 0) return;  // frame not complete yet

        byte[] frame = _responseBuffer[..(stopIdx + 1)];
        _responseBuffer = _responseBuffer[(stopIdx + 1)..];  // keep any overflow

        _pendingResponse?.TrySetResult(frame);
    }

    // ?? CSAFE request-response ??????????????????????????????????????????????

    private async Task<byte[]?> SendCsafeAsync(byte[] frame, TimeSpan timeout)
    {
        if (_rx == null || _tx == null) return null;

        await _requestLock.WaitAsync();
        try
        {
            _pendingResponse = new TaskCompletionSource<byte[]>(
                TaskCreationOptions.RunContinuationsAsynchronously);

            var buffer = CryptographicBuffer.CreateFromByteArray(frame);
            var writeStatus = await _rx.WriteValueAsync(buffer);
            if (writeStatus != GattCommunicationStatus.Success)
            {
                Console.WriteLine($"[{Channel.ChannelName}] CSAFE write failed: {writeStatus}");
                return null;
            }

            using var cts = new CancellationTokenSource(timeout);
            cts.Token.Register(() => _pendingResponse.TrySetCanceled());

            try   { return await _pendingResponse.Task; }
            catch (TaskCanceledException) { return null; }
        }
        finally
        {
            _pendingResponse = null;
            _requestLock.Release();
        }
    }

    // ?? Poll loop ???????????????????????????????????????????????????????????

    private async Task PollLoopAsync(CancellationToken ct)
    {
        var responseTimeout = TimeSpan.FromSeconds(2);

        while (!ct.IsCancellationRequested && !_stopRequested)
        {
            // ?? Rowing/Cycling frame ????????????????????????????????????????
            // Query: cadence, power, HR, elapsed
            float strokeRate = 0f, powerWatts = 0f, paceSec = 0f;
            int   hr = 0;
            float elapsed = 0f;
            bool  rowOk = false;

            var rowResp = await SendCsafeAsync(CsafeHelper.RowCycFrame, responseTimeout);
            if (rowResp != null)
            {
                rowOk = true;
                var cadData = CsafeHelper.ExtractPublicCmd(rowResp, CsafeHelper.CMD_CADENCE);
                var pwrData = CsafeHelper.ExtractPublicCmd(rowResp, CsafeHelper.CMD_POWER);
                var hrData  = CsafeHelper.ExtractPublicCmd(rowResp, CsafeHelper.CMD_HR);
                var wkData  = CsafeHelper.ExtractPublicCmd(rowResp, CsafeHelper.CMD_WORK);

                if (cadData != null) strokeRate = CsafeHelper.ParseCadence(cadData);
                if (pwrData != null) powerWatts = CsafeHelper.ParsePower(pwrData);
                if (hrData  != null) hr         = CsafeHelper.ParseHR(hrData);
                if (wkData  != null) elapsed    = CsafeHelper.ParseElapsed(wkData);

                if (powerWatts > 0f) paceSec = CsafeHelper.ComputeRowingPace(powerWatts);
            }

            // 60 ms inter-command gap (matches PM5HidDiag INTER_CMD_MS)
            await Task.Delay(60, ct);

            // ?? Strength frame ??????????????????????????????????????????????
            // Query: stroke stats (rep count / drive time / pull dist), HR, elapsed
            int   repCount    = 0;
            float driveTimeSec = 0f;
            int   pullDist    = 0;
            bool  strOk = false;

            var strResp = await SendCsafeAsync(CsafeHelper.StrengthFrame, responseTimeout);
            if (strResp != null)
            {
                strOk = true;
                var ssData = CsafeHelper.ExtractProprietarySubCmd(strResp, CsafeHelper.SUB_STROKE);
                if (ssData != null && ssData.Length >= 7)
                {
                    driveTimeSec = ssData[2] * 0.01f;
                    pullDist     = ssData[5];
                    repCount     = ssData[6];
                }

                // Prefer HR / elapsed from this frame if not yet set
                var hrData2 = CsafeHelper.ExtractPublicCmd(strResp, CsafeHelper.CMD_HR);
                var wkData2 = CsafeHelper.ExtractPublicCmd(strResp, CsafeHelper.CMD_WORK);
                if (hr == 0 && hrData2 != null) hr      = CsafeHelper.ParseHR(hrData2);
                if (elapsed == 0f && wkData2 != null) elapsed = CsafeHelper.ParseElapsed(wkData2);
            }

            // ?? Build unified status JSON ???????????????????????????????????
            bool connected  = rowOk || strOk;
            string status   = connected ? $"BLE Active ({DeviceName})" : "BLE No Data";

            string statusLine =
                $"{{\"type\":\"status\"," +
                $"\"connected\":{(connected ? "true" : "false")}," +
                $"\"strokeRate\":{strokeRate:F1}," +
                $"\"powerWatts\":{powerWatts:F1}," +
                $"\"paceSec500m\":{paceSec:F1}," +
                $"\"heartRate\":{hr}," +
                $"\"elapsedSec\":{elapsed:F1}," +
                $"\"repCount\":{repCount}," +
                $"\"driveTimeSec\":{driveTimeSec:F2}," +
                $"\"pullDistance\":{pullDist}," +
                $"\"statusText\":\"{status}\"}}";

            Channel.Send(statusLine);

            // ?? Rep event (strength) ????????????????????????????????????????
            // Fire a separate "rep" line when repCount advances so UE5's
            // existing OnNewRep delegate path still triggers correctly.
            if (repCount > _lastRepCount && driveTimeSec > 0f && pullDist > 0)
            {
                _lastRepCount = repCount;
                string repLine =
                    $"{{\"type\":\"rep\"," +
                    $"\"repCount\":{repCount}," +
                    $"\"driveTimeSec\":{driveTimeSec:F2}," +
                    $"\"pullDistance\":{pullDist}," +
                    $"\"heartRate\":{hr}," +
                    $"\"elapsedSec\":{elapsed:F1}," +
                    $"\"connected\":true}}";
                Channel.Send(repLine);
                Console.WriteLine($"[{Channel.ChannelName}] Rep #{repCount}: {driveTimeSec:F2}s  dist={pullDist}");
            }

            // ?? Debug console output (reduced frequency: every ~1 s) ????????
            if ((int)(elapsed) % 5 == 0)
            {
                Console.WriteLine($"[{Channel.ChannelName}] " +
                    $"SPM={strokeRate:F0} W={powerWatts:F0} HR={hr} " +
                    $"Rep={repCount} Elapsed={elapsed:F0}s");
            }

            // Poll at ~4 Hz (250 ms minus the two inter-command gaps above)
            await Task.Delay(Math.Max(0, 250 - 60 - 20), ct);
        }
    }
}
