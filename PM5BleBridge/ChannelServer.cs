// ChannelServer.cs
// Manages a single TCP listener for one device channel.
// When Unreal connects, it receives newline-delimited JSON lines.
// If Unreal disconnects and reconnects, the server accepts the new client automatically.

using System.Net;
using System.Net.Sockets;
using System.Text;

namespace PM5BleBridge;

internal class ChannelServer
{
    public string ChannelName    { get; }
    public int    Port           { get; }
    /// <summary>
    /// If non-empty, only the PM5 whose advertised name contains this string
    /// will be assigned to this slot.  Leave empty for auto-assign by discovery order.
    /// Example: "PM5 112233"  or just "112233"
    /// </summary>
    public string ConfiguredName { get; }

    private TcpListener  _listener = null!;  // assigned in Start()
    private StreamWriter? _writer;
    private readonly object _writeLock = new();
    private bool _running = true;

    public ChannelServer(string channelName, int port, string configuredName = "")
    {
        ChannelName    = channelName;
        Port           = port;
        ConfiguredName = configuredName;
    }

    public void Start()
    {
        _listener = new TcpListener(IPAddress.Loopback, Port);
        _listener.Start();
        Console.WriteLine($"[{ChannelName}] TCP server listening on 127.0.0.1:{Port}");
        _ = AcceptLoopAsync();
    }

    private async Task AcceptLoopAsync()
    {
        while (_running)
        {
            try
            {
                TcpClient client = await _listener.AcceptTcpClientAsync();
                Console.WriteLine($"[{ChannelName}] Unreal connected from {client.Client.RemoteEndPoint}");

                lock (_writeLock)
                {
                    try { _writer?.BaseStream.Close(); } catch { }
                    _writer = new StreamWriter(client.GetStream(), new UTF8Encoding(false))
                    {
                        AutoFlush = true,
                        NewLine   = "\n"
                    };
                }
            }
            catch (Exception ex) when (_running)
            {
                Console.WriteLine($"[{ChannelName}] Accept error: {ex.Message}");
                await Task.Delay(500);
            }
        }
    }

    /// <summary>Sends a JSON line to the currently connected Unreal client (no-op if none).</summary>
    public void Send(string jsonLine)
    {
        lock (_writeLock)
        {
            try   { _writer?.WriteLine(jsonLine); }
            catch { _writer = null; }  // client dropped — next AcceptLoopAsync iteration handles reconnect
        }
    }

    /// <summary>
    /// Returns true if this slot should accept a device with the given advertised name.
    /// Empty ConfiguredName = accept any device (auto-assign).
    /// </summary>
    public bool AcceptsDevice(string advertisedName) =>
        string.IsNullOrEmpty(ConfiguredName) ||
        advertisedName.Contains(ConfiguredName, StringComparison.OrdinalIgnoreCase);

    public void Stop()
    {
        _running = false;
        try { _listener.Stop(); } catch { }
    }
}
