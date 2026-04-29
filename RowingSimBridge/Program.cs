// RowingSimBridge — simulates a Concept2 RowErg telemetry stream for Unreal Device Lab.
//
// Real hardware path: replace this with PM5BleBridge, which connects to a real PM5
// RowErg over BLE and serves the same JSON format on this port.
//
// JSON output format matches PM5BleBridge "status" lines so UE5 needs no code change
// when switching from sim to real hardware.
//
// Port: 6791

using System.Net;
using System.Net.Sockets;
using System.Text;

const int TCP_PORT = 6791;
const int TICK_MS  = 200;   // 5 Hz

Console.WriteLine("=== Rowing Sim Bridge ===");
Console.WriteLine($"Streaming simulated RowErg JSON on 127.0.0.1:{TCP_PORT}");
Console.WriteLine("Press Ctrl+C to stop.");
Console.WriteLine();

var listener = new TcpListener(IPAddress.Loopback, TCP_PORT);
listener.Start();

double elapsed    = 0.0;
float  strokeRate = 20f;
float  powerWatts = 180f;
float  heartRate  = 140f;

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

    elapsed = 0.0;

    try
    {
        while (true)
        {
            await Task.Delay(TICK_MS);
            elapsed += TICK_MS / 1000.0;

            strokeRate = 20f + 4f  * (float)Math.Sin(elapsed * 0.3);
            powerWatts = 180f + 30f * (float)Math.Sin(elapsed * 0.25);
            heartRate  = 140f + 8f  * (float)Math.Sin(elapsed * 0.1);

            // Rowing pace derived from power: pace_sec = 500 / (power/2.8)^(1/3)
            float paceSec = powerWatts > 0
                ? 500f / (float)Math.Pow(powerWatts / 2.8, 1.0 / 3.0)
                : 0f;

            string line = $"{{\"type\":\"status\",\"connected\":true," +
                           $"\"strokeRate\":{strokeRate:F1},\"powerWatts\":{powerWatts:F1}," +
                           $"\"paceSec500m\":{paceSec:F1},\"heartRate\":{(int)heartRate}," +
                           $"\"elapsedSec\":{elapsed:F1},\"statusText\":\"Sim Active\"}}";

            await writer.WriteLineAsync(line);
            Console.WriteLine($"  [Row] {line}");
        }
    }
    catch (Exception ex)
    {
        Console.WriteLine($"[TCP:{TCP_PORT}] Client disconnected: {ex.Message}");
    }
}
