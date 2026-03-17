// Concept2UsbReader.cs — FINAL PRODUCTION (StrengthErg)
// Reads rep count, rep time, pull distance, elapsed time, and heart rate
// from Concept2 StrengthErg PM5 via USB HID + HidSharp.
//
// CONFIRMED WORKING DATA (firmware v55.036):
//   STROKESTATS (0x6E):
//     byte[2]  = Drive Time (×0.01s)     — time of each push
//     byte[5]  = Pull Distance            — distance per rep (changes each rep)
//     byte[6]  = Rep Counter              — total reps
//   GETTWORK (0xA0):
//     bytes[0-2] = Hours, Minutes, Seconds — elapsed workout time
//   GETHRCUR (0xB0):
//     byte[0]  = Heart rate BPM           — requires HR belt
//
// USAGE:
//   Attach to a GameObject. Start a workout on the PM5 screen.
//   Access data via Concept2UsbReader.Instance.RepCount, .RepTimeSec, etc.

using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Threading;
using UnityEngine;
using HidSharp;

public class Concept2UsbReader : MonoBehaviour
{
    public static Concept2UsbReader Instance { get; private set; }

    // ─── Inspector ──────────────────────────────────────────────
    [Header("Connection")]
    [Tooltip("Polls per second (max ~15 due to CSAFE 50ms inter-frame gap)")]
    public float pollRateHz = 8f;

    [Header("Simulation")]
    public bool simulateInput = false;

    // ─── Public Data ────────────────────────────────────────────

    /// <summary>Total rep count this workout.</summary>
    public int RepCount { get; private set; }

    /// <summary>Drive time of the last rep in seconds.</summary>
    public float RepTimeSec { get; private set; }

    /// <summary>Pull distance value of the last rep (raw PM5 units).</summary>
    public int PullDistance { get; private set; }

    /// <summary>Workout elapsed time in seconds.</summary>
    public float ElapsedSeconds { get; private set; }

    /// <summary>Heart rate in BPM (0 if no HR belt connected).</summary>
    public int HeartRate { get; private set; }

    /// <summary>True when connected to the PM5 via USB.</summary>
    public bool IsConnected { get; private set; }

    /// <summary>True when reps are being performed (rep count > 0).</summary>
    public bool IsActive { get; private set; }

    /// <summary>Human-readable connection/activity status.</summary>
    public string StatusText { get; private set; } = "Disconnected";

    /// <summary>List of rep times for all reps this set.</summary>
    public List<float> RepTimeHistory { get; private set; } = new List<float>();

    /// <summary>List of pull distances for all reps this set.</summary>
    public List<int> PullDistanceHistory { get; private set; } = new List<int>();

    // ─── Events ─────────────────────────────────────────────────

    /// <summary>
    /// Fired on the main thread when a new rep is detected.
    /// Parameters: repNumber, repTimeSec, pullDistance
    /// </summary>
    public event Action<int, float, int> OnNewRep;

    // ─── Constants ──────────────────────────────────────────────
    private const int  PM5_VID = 0x17A4;
    private const int  PM5_PID = 0x0069;
    private const byte REPORT_ID   = 0x01;
    private const byte CSAFE_START = 0xF1;
    private const byte CSAFE_STOP  = 0xF2;
    private const byte WRAPPER_CMD = 0x1A;
    private const byte STROKESTATS = 0x6E;
    private const byte CMD_GETTWORK = 0xA0;
    private const byte CMD_GETHRCUR = 0xB0;

    // ─── Internal ───────────────────────────────────────────────
    private HidStream _stream;
    private Thread _pollThread;
    private volatile bool _running;
    private int _maxOut, _maxIn;

    private readonly object _dataLock = new object();
    private int _repCount, _pullDist, _hr;
    private float _repTime, _elapsed;
    private bool _connected, _active;
    private string _status = "Disconnected";
    private List<float> _repTimeHist = new List<float>();
    private List<int> _pullDistHist = new List<int>();

    // Pending new-rep event data (fired on main thread)
    private struct PendingRep { public int num; public float time; public int dist; }
    private ConcurrentQueue<PendingRep> _pendingReps = new ConcurrentQueue<PendingRep>();

    private ConcurrentQueue<string> _logs = new ConcurrentQueue<string>();
    private ConcurrentQueue<string> _warnings = new ConcurrentQueue<string>();

    // ─── Unity Lifecycle ────────────────────────────────────────
    void Awake()
    {
        if (Instance != null) { Destroy(gameObject); return; }
        Instance = this;
        DontDestroyOnLoad(gameObject);
    }

    void Start()
    {
        if (simulateInput)
        {
            IsConnected = true;
            StatusText = "Simulated";
            Debug.Log("[C2] Simulation mode.");
            return;
        }
        _running = true;
        _pollThread = new Thread(ConnectionLoop);
        _pollThread.IsBackground = true;
        _pollThread.Start();
    }

    void Update()
    {
        // Flush logs
        while (_logs.TryDequeue(out string m)) Debug.Log(m);
        while (_warnings.TryDequeue(out string m)) Debug.LogWarning(m);

        if (simulateInput) return;

        // Copy thread data
        lock (_dataLock)
        {
            RepCount       = _repCount;
            RepTimeSec     = _repTime;
            PullDistance    = _pullDist;
            ElapsedSeconds = _elapsed;
            HeartRate      = _hr;
            IsConnected    = _connected;
            IsActive       = _active;
            StatusText     = _status;

            if (_repTimeHist.Count != RepTimeHistory.Count)
            {
                RepTimeHistory = new List<float>(_repTimeHist);
                PullDistanceHistory = new List<int>(_pullDistHist);
            }
        }

        // Fire new-rep events on main thread
        while (_pendingReps.TryDequeue(out PendingRep rep))
        {
            OnNewRep?.Invoke(rep.num, rep.time, rep.dist);
        }
    }

    void OnApplicationQuit() { Shutdown(); }
    void OnDestroy() { Shutdown(); }
    void Shutdown()
    {
        _running = false;
        try { _pollThread?.Abort(); } catch { }
        try { _stream?.Close(); } catch { }
    }

    /// <summary>Resets rep history. Call between sets if needed.</summary>
    public void ResetHistory()
    {
        lock (_dataLock)
        {
            _repTimeHist.Clear();
            _pullDistHist.Clear();
        }
    }

    // ─── Connection Loop ────────────────────────────────────────
    void ConnectionLoop()
    {
        while (_running)
        {
            try { if (Connect()) PollLoop(); }
            catch (Exception e) { Warn($"[C2] Error: {e.Message}"); }

            lock (_dataLock) { _connected = false; _status = "Disconnected"; }
            try { _stream?.Close(); } catch { }
            _stream = null;

            if (_running) { Log("[C2] Reconnecting in 3s..."); Thread.Sleep(3000); }
        }
    }

    bool Connect()
    {
        Log("[C2] Searching for PM5...");
        var device = DeviceList.Local.GetHidDeviceOrNull(vendorID: PM5_VID, productID: PM5_PID);
        if (device == null) { lock (_dataLock) { _status = "PM5 not found"; } return false; }

        if (!device.TryOpen(out _stream))
        {
            Warn("[C2] PM5 found but locked by another app.");
            return false;
        }

        _maxOut = device.GetMaxOutputReportLength();
        _maxIn  = device.GetMaxInputReportLength();
        _stream.ReadTimeout  = 5000;
        _stream.WriteTimeout = 2000;

        Log("[C2] PM5 connected!");
        lock (_dataLock) { _connected = true; _status = "Connected"; }
        return true;
    }

    // ─── Poll Loop ──────────────────────────────────────────────
    void PollLoop()
    {
        int sleepMs = Math.Max(80, (int)(1000f / pollRateHz));
        int lastRepCount = -1;

        while (_running && _stream != null)
        {
            try
            {
                // 1. STROKESTATS — rep count, rep time, pull distance
                PollStrokeStats();
                Thread.Sleep(60);

                // 2. Elapsed time
                PollWorkTime();
                Thread.Sleep(60);

                // 3. Heart rate
                PollHeartRate();

                // Detect new rep and fire event
                int curRep;
                float curTime;
                int curDist;
                lock (_dataLock)
                {
                    curRep = _repCount;
                    curTime = _repTime;
                    curDist = _pullDist;
                }

                if (curRep > lastRepCount && curRep > 0)
                {
                    lastRepCount = curRep;
                    Log($"[C2] Rep #{curRep}: Time={curTime:F2}s, Distance={curDist}");
                    _pendingReps.Enqueue(new PendingRep { num = curRep, time = curTime, dist = curDist });
                }

                Thread.Sleep(Math.Max(10, sleepMs - 180));
            }
            catch (TimeoutException)
            {
                lock (_dataLock) { _status = "Connected (PM5 sleeping)"; _active = false; }
                Thread.Sleep(1000);
            }
            catch (Exception e)
            {
                Warn($"[C2] Poll error: {e.Message}");
                return;
            }
        }
    }

    // ─── STROKESTATS ────────────────────────────────────────────
    void PollStrokeStats()
    {
        byte[] resp = SendFrame(new byte[] { WRAPPER_CMD, 0x02, STROKESTATS, 0x00 });
        if (resp == null) return;

        byte[] data = ExtractProprietaryData(resp, STROKESTATS);
        if (data == null || data.Length < 7) return;

        int driveTimeRaw = data[2];   // ×0.01s
        int pullDist     = data[5];   // pull distance
        int repCounter   = data[6];   // rep count

        lock (_dataLock)
        {
            // Track history on new rep
            if (repCounter > _repCount && pullDist > 0)
            {
                _repTimeHist.Add(driveTimeRaw * 0.01f);
                _pullDistHist.Add(pullDist);
            }

            _repCount = repCounter;
            _repTime  = driveTimeRaw * 0.01f;
            _pullDist = pullDist;
            _active   = repCounter > 0;
            _status   = _active ? $"Active (Rep #{repCounter})" : "Connected (idle)";
        }
    }

    void PollWorkTime()
    {
        byte[] resp = SendFrame(new byte[] { CMD_GETTWORK });
        if (resp == null) return;

        var data = ExtractPublicCmdData(resp, CMD_GETTWORK);
        if (data != null && data.Length >= 3)
        {
            float total = data[0] * 3600f + data[1] * 60f + data[2];
            lock (_dataLock) { _elapsed = total; }
        }
    }

    void PollHeartRate()
    {
        byte[] resp = SendFrame(new byte[] { CMD_GETHRCUR });
        if (resp == null) return;

        var data = ExtractPublicCmdData(resp, CMD_GETHRCUR);
        if (data != null && data.Length >= 1)
            lock (_dataLock) { _hr = data[0]; }
    }

    // ─── CSAFE Framing ──────────────────────────────────────────
    byte[] SendFrame(byte[] commands)
    {
        byte checksum = 0;
        foreach (byte b in commands) checksum ^= b;

        byte[] report = new byte[_maxOut];
        report[0] = REPORT_ID;
        report[1] = CSAFE_START;
        int idx = 2;
        foreach (byte b in commands) report[idx++] = b;
        report[idx++] = checksum;
        report[idx++] = CSAFE_STOP;

        _stream.Write(report);

        byte[] buf = new byte[_maxIn];
        int n = _stream.Read(buf, 0, buf.Length);
        return n > 0 ? buf : null;
    }

    byte[] ExtractPublicCmdData(byte[] buf, byte cmd)
    {
        if (buf == null) return null;
        int start = -1, stop = -1;
        for (int i = 0; i < buf.Length; i++)
        {
            if (buf[i] == CSAFE_START && start < 0) start = i;
            if (buf[i] == CSAFE_STOP && start >= 0) { stop = i; break; }
        }
        if (start < 0 || stop <= start + 2) return null;

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

    byte[] ExtractProprietaryData(byte[] buf, byte subCmd)
    {
        if (buf == null) return null;
        int start = -1;
        for (int i = 0; i < buf.Length; i++) { if (buf[i] == CSAFE_START) { start = i; break; } }
        if (start < 0 || start + 4 >= buf.Length) return null;

        int pos = start + 2;
        int limit = Math.Min(buf.Length, start + 60);
        while (pos < limit)
        {
            if (buf[pos] == WRAPPER_CMD)
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

    void Log(string msg)  => _logs.Enqueue(msg);
    void Warn(string msg) => _warnings.Enqueue(msg);
}