// PM5HidDiag — Minimal Concept2 PM5 USB HID diagnostic tool
// Sends CSAFE commands over USB HID and parses real PM5 responses.
// Frame: [ REPORT_ID, CSAFE_START, ...commands, XOR-checksum, CSAFE_STOP ] padded to maxOut.
// Mirrors Unity Concept2UsbReader.cs SendFrame/ExtractPublicCmdData/ExtractProprietaryData.

using HidSharp;

const int  PM5_VID      = 0x17A4;
const int  PM5_PID      = 0x0069;
const byte REPORT_ID    = 0x01;
const byte CSAFE_START  = 0xF1;
const byte CSAFE_STOP   = 0xF2;
const byte WRAPPER_CMD  = 0x1A;
const byte STROKESTATS  = 0x6E;
const byte CMD_GETTWORK = 0xA0;
const byte CMD_GETHRCUR = 0xB0;

const int POLL_CYCLES    = 20;   // cycles before exit (0 = infinite)
const int INTER_CMD_MS   = 60;   // gap between commands (matches Unity)
const int INTER_CYCLE_MS = 200;  // gap between full cycles
const int READ_TIMEOUT   = 2000;
const int WRITE_TIMEOUT  = 2000;

Console.WriteLine("=== PM5 CSAFE Diagnostic ===");
Console.WriteLine();
Console.WriteLine("[STAGE 1] Enumerating HID devices...");

HidDevice[] allDevices;
try { allDevices = DeviceList.Local.GetHidDevices().ToArray(); }
catch (Exception ex) { Console.WriteLine($"  [ERROR] {ex.Message}"); return; }

Console.WriteLine($"  Found {allDevices.Length} HID device(s):");
foreach (var d in allDevices)
{
    string name = "(unknown)";
    try { name = d.GetFriendlyName(); } catch { }
    Console.WriteLine($"    VID=0x{d.VendorID:X4}  PID=0x{d.ProductID:X4}  {name}");
}
Console.WriteLine();

Console.WriteLine($"[STAGE 2] Looking for PM5 (VID=0x{PM5_VID:X4} PID=0x{PM5_PID:X4})...");
HidDevice? pm5 = allDevices.FirstOrDefault(d => d.VendorID == PM5_VID && d.ProductID == PM5_PID);
if (pm5 == null) { Console.WriteLine("  [FAIL] PM5 not found. Check USB connection."); return; }

string pm5Name = "(unknown)";
try { pm5Name = pm5.GetFriendlyName(); } catch { }
Console.WriteLine($"  [OK] {pm5Name}");
Console.WriteLine($"       Path: {pm5.DevicePath}");
Console.WriteLine();

Console.WriteLine("[STAGE 3] Opening PM5...");
HidStream? stream;
try
{
    var cfg = new OpenConfiguration();
    cfg.SetOption(OpenOption.Exclusive, false);
    cfg.SetOption(OpenOption.Transient, true);
    if (!pm5.TryOpen(cfg, out stream))
    {
        Console.WriteLine("  [FAIL] TryOpen returned false — device locked by another process?");
        return;
    }
    stream.ReadTimeout  = READ_TIMEOUT;
    stream.WriteTimeout = WRITE_TIMEOUT;
    Console.WriteLine("  [OK] Device opened.");
}
catch (Exception ex)
{
    Console.WriteLine($"  [FAIL] {ex.Message}");
    Console.WriteLine("         Try running as Administrator or close Concept2 Utility.");
    return;
}

int maxOut = pm5.GetMaxOutputReportLength();
int maxIn  = pm5.GetMaxInputReportLength();
Console.WriteLine($"  Report sizes — OUT: {maxOut}  IN: {maxIn}");
Console.WriteLine();

Console.WriteLine($"[STAGE 4] CSAFE poll loop — {(POLL_CYCLES == 0 ? "infinite" : POLL_CYCLES)} cycle(s), Ctrl+C to stop");
Console.WriteLine("          Start a workout on the PM5 to see non-zero values.");
Console.WriteLine();

using (stream)
{
    int cycle = 0;
    while (POLL_CYCLES == 0 || cycle < POLL_CYCLES)
    {
        Console.WriteLine($"-- Cycle {cycle + 1} --------------------------------------------------");

        // Poll STROKESTATS (rep count, drive time, pull distance)
        byte[]? ssResp = SendFrame(stream, maxOut, maxIn,
            new byte[] { WRAPPER_CMD, 0x02, STROKESTATS, 0x00 }, "STROKESTATS");
        if (ssResp != null)
        {
            byte[]? data = ExtractProprietaryData(ssResp, STROKESTATS);
            if (data != null && data.Length >= 7)
            {
                float driveTimeSec = data[2] * 0.01f;
                int   pullDist     = data[5];
                int   repCount     = data[6];
                Console.WriteLine($"  -> RepCount={repCount}  DriveTime={driveTimeSec:F2}s  PullDist={pullDist}");
            }
            else Console.WriteLine("  -> STROKESTATS: no data (start a workout on the PM5)");
        }

        Thread.Sleep(INTER_CMD_MS);

        // Poll GETTWORK (elapsed time)
        byte[]? wtResp = SendFrame(stream, maxOut, maxIn,
            new byte[] { CMD_GETTWORK }, "GETTWORK");
        if (wtResp != null)
        {
            byte[]? data = ExtractPublicCmdData(wtResp, CMD_GETTWORK);
            if (data != null && data.Length >= 3)
            {
                float elapsed = data[0] * 3600f + data[1] * 60f + data[2];
                Console.WriteLine($"  -> Elapsed={elapsed:F0}s  ({data[0]}h {data[1]}m {data[2]}s)");
            }
            else Console.WriteLine("  -> GETTWORK: no data");
        }

        Thread.Sleep(INTER_CMD_MS);

        // Poll GETHRCUR (heart rate)
        byte[]? hrResp = SendFrame(stream, maxOut, maxIn,
            new byte[] { CMD_GETHRCUR }, "GETHRCUR");
        if (hrResp != null)
        {
            byte[]? data = ExtractPublicCmdData(hrResp, CMD_GETHRCUR);
            if (data != null && data.Length >= 1)
                Console.WriteLine($"  -> HeartRate={data[0]} BPM");
            else
                Console.WriteLine("  -> GETHRCUR: no data (HR belt required)");
        }

        Console.WriteLine();
        cycle++;
        if (POLL_CYCLES == 0 || cycle < POLL_CYCLES)
            Thread.Sleep(INTER_CYCLE_MS);
    }
}

Console.WriteLine("=== Done ===");

// ?? Helpers ??????????????????????????????????????????????????????????????

/// <summary>
/// Builds and sends a CSAFE frame, reads the response.
/// Layout: [ REPORT_ID, CSAFE_START, ...commands, XOR-checksum, CSAFE_STOP ] padded to maxOut.
/// Matches Unity Concept2UsbReader.SendFrame exactly.
/// </summary>
static byte[]? SendFrame(HidStream stream, int maxOut, int maxIn,
    byte[] commands, string label)
{
    byte checksum = 0;
    foreach (byte b in commands) checksum ^= b;

    byte[] report = new byte[maxOut];
    report[0] = 0x01;  // REPORT_ID
    report[1] = 0xF1;  // CSAFE_START
    int idx = 2;
    foreach (byte b in commands) report[idx++] = b;
    report[idx++] = checksum;
    report[idx]   = 0xF2;  // CSAFE_STOP

    Console.WriteLine($"  OUT [{label}]: {BitConverter.ToString(report, 0, idx + 1).Replace("-", " ")}");

    try { stream.Write(report); }
    catch (Exception ex) { Console.WriteLine($"  [WRITE ERROR] {label}: {ex.Message}"); return null; }

    try
    {
        byte[] buf = new byte[maxIn];
        int n = stream.Read(buf, 0, buf.Length);
        if (n <= 0) { Console.WriteLine($"  [READ] {label}: 0 bytes"); return null; }
        Console.WriteLine($"  IN  [{label}]: {BitConverter.ToString(buf, 0, n).Replace("-", " ")}");
        return buf;
    }
    catch (TimeoutException) { Console.WriteLine($"  [TIMEOUT] {label}: no response"); return null; }
    catch (Exception ex)     { Console.WriteLine($"  [READ ERROR] {label}: {ex.Message}"); return null; }
}

/// <summary>
/// Finds a public command's data payload inside a CSAFE response frame.
/// Format: [ cmdId, dataLen, data... ] between CSAFE_START and CSAFE_STOP.
/// </summary>
static byte[]? ExtractPublicCmdData(byte[] buf, byte cmd)
{
    int start = -1, stop = -1;
    for (int i = 0; i < buf.Length; i++)
    {
        if (buf[i] == 0xF1 && start < 0) start = i;
        if (buf[i] == 0xF2 && start >= 0) { stop = i; break; }
    }
    if (start < 0 || stop <= start + 2) return null;

    int pos = start + 2;
    while (pos < stop - 1)
    {
        byte cmdId   = buf[pos++];
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
/// Finds a proprietary sub-command's data payload inside a CSAFE response frame.
/// Wrapper format: [ WRAPPER_CMD(0x1A), wrapperLen, subCmd, dataLen, data... ]
/// </summary>
static byte[]? ExtractProprietaryData(byte[] buf, byte subCmd)
{
    int start = -1;
    for (int i = 0; i < buf.Length; i++) { if (buf[i] == 0xF1) { start = i; break; } }
    if (start < 0 || start + 4 >= buf.Length) return null;

    int pos   = start + 2;
    int limit = Math.Min(buf.Length, start + 60);

    while (pos < limit)
    {
        if (buf[pos] == 0x1A)  // WRAPPER_CMD
        {
            pos++;
            if (pos >= limit) break;
            int wrapperLen = buf[pos++];
            if (wrapperLen <= 0 || pos >= limit) break;
            int wrapperEnd = Math.Min(pos + wrapperLen, limit);
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
