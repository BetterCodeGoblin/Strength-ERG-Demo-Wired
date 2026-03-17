using UnityEngine;
using System;
using System.Net.Sockets;
using System.Text;
using System.Threading;

public class ErgBridgeClient : MonoBehaviour
{
    public Action<float, float, float, bool> OnDataReceived;

    private TcpClient     _client;
    private NetworkStream _stream;
    private Thread        _thread;
    private bool          _running = false;

    // Thread-safe staging fields
    private float _rate, _pace, _power;
    private bool  _connected;
    private bool  _hasNewData = false;
    private readonly object _lock = new object();

    void Start()
    {
        // Small delay to let ErgBridge initialize
        Invoke(nameof(Connect), 5f);
    }

   void Connect()
{
    try
    {
        _client  = new TcpClient("127.0.0.1", 6789);
        _stream  = _client.GetStream();
        _running = true;

        _thread = new Thread(ReceiveLoop);
        _thread.IsBackground = true;
        _thread.Start();

        Debug.Log("[ErgBridgeClient] Connected to ErgBridge.");
    }
    catch (Exception e)
    {
        Debug.LogWarning($"[ErgBridgeClient] Connection attempt failed, retrying in 3s: {e.Message}");
        Invoke(nameof(Connect), 3f);
    }
}

    void ReceiveLoop()
    {
        byte[] requestByte = new byte[] { 0x01 };
        byte[] buffer      = new byte[256];

        while (_running)
        {
            try
            {
                // Request data
                _stream.Write(requestByte, 0, 1);

                // Read response
                int bytesRead = _stream.Read(buffer, 0, buffer.Length);
                if (bytesRead == 0) break;

                string raw = Encoding.UTF8.GetString(buffer, 0, bytesRead).Trim();
                string[] parts = raw.Split(',');

                if (parts.Length >= 4)
                {
                    lock (_lock)
                    {
                        float.TryParse(parts[0], out _rate);
                        float.TryParse(parts[1], out _pace);
                        float.TryParse(parts[2], out _power);
                        _connected  = parts[3] == "1";
                        _hasNewData = true;
                    }
                }

                Thread.Sleep(100); // 10Hz
            }
            catch (Exception e)
            {
                Debug.LogWarning($"[ErgBridgeClient] Receive error: {e.Message}");
                break;
            }
        }
    }

    // Called on main thread — safe to fire Unity events here
    void Update()
    {
        if (!_hasNewData) return;

        float rate, pace, power;
        bool connected;

        lock (_lock)
        {
            rate       = _rate;
            pace       = _pace;
            power      = _power;
            connected  = _connected;
            _hasNewData = false;
        }

        OnDataReceived?.Invoke(rate, pace, power, connected);
    }

    void OnDestroy()
    {
        _running = false;
        _thread?.Abort();
        _stream?.Close();
        _client?.Close();
    }
}