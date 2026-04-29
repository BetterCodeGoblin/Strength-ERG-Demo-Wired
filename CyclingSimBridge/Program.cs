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

double elapsed    = 0.0;
float  cadenceRpm = 80f;
float  powerWatts = 200f;
float  heartRate  = 145f;

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

            cadenceRpm = 80f + 10f * (float)Math.Sin(elapsed * 0.4);
            powerWatts = 200f + 40f * (float)Math.Sin(elapsed * 0.28);
            heartRate  = 145f + 10f * (float)Math.Sin(elapsed * 0.1);

            string line = $"{{\"type\":\"status\",\"connected\":true," +
                           $"\"strokeRate\":{cadenceRpm:F1},\"powerWatts\":{powerWatts:F1}," +
                           $"\"paceSec500m\":0,\"heartRate\":{(int)heartRate}," +
                           $"\"elapsedSec\":{elapsed:F1},\"statusText\":\"Sim Active\"}}";

            await writer.WriteLineAsync(line);
            Console.WriteLine($"  [Cyc] {line}");
        }
    }
    catch (Exception ex)
    {
        Console.WriteLine($"[TCP:{TCP_PORT}] Client disconnected: {ex.Message}");
    }
}
