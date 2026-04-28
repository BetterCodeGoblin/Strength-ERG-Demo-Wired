// PM5HidDiag — Minimal Concept2 PM5 USB HID diagnostic tool
// Stages:
//   1. List all HID devices
//   2. Identify PM5 by VID/PID
//   3. Attempt to open the device
//   4. Start a read loop and print raw packet bytes

using HidSharp;

// ?? Constants ????????????????????????????????????????????????????????????????
// Concept2 PM5 USB HID identifiers (confirmed in Concept2 SDK documentation)
const int PM5_VID = 0x17A4;
const int PM5_PID = 0x0069;

// Maximum time to wait for a single HID read before timing out (ms)
const int READ_TIMEOUT_MS = 2000;

// How many packets to read before stopping the demo loop (0 = run forever)
const int MAX_PACKETS = 50;

Console.WriteLine("=== PM5 USB HID Diagnostic ===");
Console.WriteLine();

// ?? Stage 1: List all HID devices ????????????????????????????????????????????
Console.WriteLine("[STAGE 1] Enumerating HID devices...");

HidDevice[] allDevices;
try
{
    var loader = DeviceList.Local;
    allDevices = loader.GetHidDevices().ToArray();
}
catch (Exception ex)
{
    Console.WriteLine($"  [ERROR] Failed to enumerate HID devices: {ex.Message}");
    return;
}

if (allDevices.Length == 0)
{
    Console.WriteLine("  [WARN] No HID devices found at all. Check USB connections.");
}
else
{
    Console.WriteLine($"  Found {allDevices.Length} HID device(s):");
    foreach (var d in allDevices)
    {
        string name = "(unknown)";
        try { name = d.GetFriendlyName(); } catch { }
        Console.WriteLine($"    VID=0x{d.VendorID:X4}  PID=0x{d.ProductID:X4}  {name}");
    }
}

Console.WriteLine();

// ?? Stage 2: Identify PM5 ?????????????????????????????????????????????????????
Console.WriteLine($"[STAGE 2] Looking for PM5 (VID=0x{PM5_VID:X4} PID=0x{PM5_PID:X4})...");

HidDevice? pm5 = allDevices.FirstOrDefault(
    d => d.VendorID == PM5_VID && d.ProductID == PM5_PID);

if (pm5 == null)
{
    Console.WriteLine("  [FAIL] PM5 NOT found. Possible reasons:");
    Console.WriteLine("         - Device is not connected via USB.");
    Console.WriteLine("         - A different PID is in use (check the VID/PID list above).");
    Console.WriteLine("         - Windows has the device claimed by another driver.");
    Console.WriteLine();
    Console.WriteLine("  Tip: If you see a Concept2 VID (0x17A4) above with a different PID,");
    Console.WriteLine("       update PM5_PID in this source file to match.");
    return;
}

string pm5Name = "(unknown)";
try { pm5Name = pm5.GetFriendlyName(); } catch { }
Console.WriteLine($"  [OK] PM5 found: {pm5Name}");
Console.WriteLine($"       DevicePath: {pm5.DevicePath}");
Console.WriteLine();

// ?? Stage 3: Open the device ??????????????????????????????????????????????????
Console.WriteLine("[STAGE 3] Attempting to open PM5...");

HidStream? stream = null;
try
{
    var openConfig = new OpenConfiguration();
    openConfig.SetOption(OpenOption.Exclusive, false);
    openConfig.SetOption(OpenOption.Transient, true);

    if (!pm5.TryOpen(openConfig, out stream))
    {
        Console.WriteLine("  [FAIL] TryOpen returned false. Device may be locked by another process.");
        return;
    }

    stream.ReadTimeout  = READ_TIMEOUT_MS;
    stream.WriteTimeout = READ_TIMEOUT_MS;
    Console.WriteLine("  [OK] Device opened successfully.");
}
catch (Exception ex)
{
    Console.WriteLine($"  [FAIL] Exception while opening device: {ex.Message}");
    Console.WriteLine("         Try running as Administrator, or check if Concept2 Utility is open.");
    return;
}

Console.WriteLine();

// ?? Stage 4: Read loop ????????????????????????????????????????????????????????
Console.WriteLine($"[STAGE 4] Starting read loop (up to {MAX_PACKETS} packets, Ctrl+C to stop)...");
Console.WriteLine();

int packetCount = 0;
int errorCount  = 0;
const int MAX_ERRORS = 5;

using (stream)
{
    // HidSharp requires a buffer sized to the device's max input report length (+1 for report ID byte)
    int bufSize = pm5.GetMaxInputReportLength();
    if (bufSize <= 0) bufSize = 65; // fallback
    byte[] buf = new byte[bufSize];

    Console.WriteLine($"  Input report buffer size: {bufSize} bytes");
    Console.WriteLine();

    while (MAX_PACKETS == 0 || packetCount < MAX_PACKETS)
    {
        try
        {
            int bytesRead = stream.Read(buf, 0, buf.Length);
            if (bytesRead <= 0)
            {
                Console.WriteLine("  [READ] 0 bytes — device may have disconnected.");
                break;
            }

            string hex = BitConverter.ToString(buf, 0, bytesRead).Replace("-", " ");
            Console.WriteLine($"  [PKT #{packetCount + 1:D3}] {bytesRead,3}b  {hex}");
            packetCount++;
            errorCount = 0; // reset on success
        }
        catch (TimeoutException)
        {
            // A timeout is not fatal — PM5 may simply not be sending right now
            Console.WriteLine("  [READ] Timeout waiting for data (no error, retrying...)");
            errorCount++;
        }
        catch (Exception ex)
        {
            Console.WriteLine($"  [READ] Error: {ex.Message}");
            errorCount++;
        }

        if (errorCount >= MAX_ERRORS)
        {
            Console.WriteLine($"  [STOP] {MAX_ERRORS} consecutive errors — aborting read loop.");
            break;
        }
    }
}

Console.WriteLine();
Console.WriteLine($"=== Done. Packets received: {packetCount} ===");
