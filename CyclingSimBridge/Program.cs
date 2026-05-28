// CyclingSimBridge — simulates a BikeErg / cycling device telemetry stream for Unreal Device Lab.
//
// Real hardware path: replace this with PM5BleBridge, which connects to a real PM5
// BikeErg over BLE and serves the same JSON format on this port.
//
// JSON output format matches PM5BleBridge "status" lines so UE5 needs no code change
// when switching from sim to real hardware.
//
// Port: 6792

using System.Net;
using System.Net.Sockets;
using System.Text;

const int TCP_PORT = 6792;
const int TICK_MS  = 200;   // 5 Hz

Console.WriteLine("=== Cycling Sim Bridge ===");
Console.WriteLine($"Streaming simulated BikeErg JSON on 127.0.0.1:{TCP_PORT}");
Console.WriteLine("Press Ctrl+C to stop.");
Console.WriteLine();

var listener = new TcpListener(IPAddress.Loopback, TCP_PORT);
listener.Start();

double elapsed      = 0.0;
float  cadenceRpm   = 80f;
float  powerWatts   = 200f;
float  heartRate    = 145f;
int    pedalCount   = 0;
double nextPedalAt  = 0.0;  // elapsed seconds when next simulated pedal stroke fires

while (true)
{
    Console.WriteLine($"[TCP:{TCP_PORT}] Waiting for Unreal to connect...");
    using TcpClient client = listener.AcceptTcpClient();
    Console.WriteLine($"[TCP:{TCP_PORT}] Client connected: {client.Client.RemoteEndPoint}");

    var writer = new StreamWriter(client.GetStream(), new UTF8Encoding(false))
    {
        AutoFlush = true,
        NewLine   = "\n"
    };

    elapsed     = 0.0;
    pedalCount  = 0;
    nextPedalAt = 0.0;

    try
    {
        while (true)
        {
            await Task.Delay(TICK_MS);
            elapsed += TICK_MS / 1000.0;

            cadenceRpm = 80f + 10f * (float)Math.Sin(elapsed * 0.4);
            powerWatts = 200f + 40f * (float)Math.Sin(elapsed * 0.28);
            heartRate  = 145f + 10f * (float)Math.Sin(elapsed * 0.1);

            // Emit a rep event each time a simulated pedal revolution completes.
            // Revolution interval = 60 / cadenceRpm seconds.
            // Use while so that if a tick overshoots multiple revolution boundaries
            // (e.g. after a stall) each missed stroke fires in order, matching how
            // CE060035 StrokeData fires one notification per completed pedal stroke
            // on real BikeErg hardware.
            while (elapsed >= nextPedalAt)
            {
                pedalCount++;
                float revInterval = cadenceRpm > 0f ? 60f / cadenceRpm : 0.75f;
                nextPedalAt += revInterval;  // advance by interval, not from now, to avoid drift

                // driveTimeSec ? half revolution; pullDistance maps to crank arc (arbitrary units).
                float driveTime = revInterval * 0.5f;
                int   pullDist  = (int)(80f + powerWatts * 0.15f);

                string rep = $"{{\"type\":\"rep\",\"repCount\":{pedalCount}," +
                              $"\"driveTimeSec\":{driveTime:F3},\"pullDistance\":{pullDist}," +
                              $"\"heartRate\":{(int)heartRate},\"elapsedSec\":{elapsed:F1}," +
                              $"\"connected\":true}}";
                await writer.WriteLineAsync(rep);
                Console.WriteLine($"  [Cyc] {rep}");
            }

            string line = $"{{\"type\":\"status\",\"connected\":true," +
                           $"\"strokeRate\":{cadenceRpm:F1},\"powerWatts\":{powerWatts:F1}," +
                           $"\"paceSec500m\":0,\"heartRate\":{(int)heartRate}," +
                           $"\"elapsedSec\":{elapsed:F1},\"repCount\":{pedalCount}," +
                           $"\"statusText\":\"Sim Active\"}}";

            await writer.WriteLineAsync(line);
            Console.WriteLine($"  [Cyc] {line}");
        }
    }
    catch (Exception ex)
    {
        Console.WriteLine($"[TCP:{TCP_PORT}] Client disconnected: {ex.Message}");
    }
}
