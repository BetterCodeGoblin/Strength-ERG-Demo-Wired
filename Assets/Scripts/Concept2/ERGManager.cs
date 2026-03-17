using UnityEngine;
using System.Diagnostics;
using System.IO;

public class ErgManager : MonoBehaviour
{
    public static ErgManager Instance { get; private set; }

    [Header("Bridge Settings")]
    public string bridgeExePath = "ErgBridge/ErgBridge.exe";

    [Header("Debug / Simulation")]
    public bool simulateInput = false;
    public float simulatedStrokeRate = 22f;
    public float simulatedPower      = 150f;
    public float simulatedPace       = 160f;

    // Live data
    public float StrokeRate     { get; private set; }
    public float PaceSeconds    { get; private set; }
    public float PowerWatts     { get; private set; }
    public bool  IsConnected    { get; private set; }

    private Process        _bridgeProcess;
    private ErgBridgeClient _bridgeClient;

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
            UnityEngine.Debug.Log("[ErgManager] Simulation mode.");
            return;
        }

        LaunchBridge();

        _bridgeClient = gameObject.AddComponent<ErgBridgeClient>();
        _bridgeClient.OnDataReceived += OnErgData;
    }

    void Update()
    {
        if (simulateInput)
        {
            StrokeRate  = simulatedStrokeRate;
            PowerWatts  = simulatedPower;
            PaceSeconds = simulatedPace;
        }
    }

    void LaunchBridge()
    {
        string fullPath = Path.Combine(Application.dataPath, "..", bridgeExePath);
        fullPath = Path.GetFullPath(fullPath);

        if (!File.Exists(fullPath))
        {
            UnityEngine.Debug.LogError($"[ErgManager] Bridge not found at: {fullPath}");
            return;
        }

        ProcessStartInfo psi = new ProcessStartInfo
        {
            FileName        = fullPath,
            UseShellExecute = false,
            CreateNoWindow  = true
        };

        _bridgeProcess = Process.Start(psi);
        UnityEngine.Debug.Log("[ErgManager] ErgBridge launched.");
    }

    void OnErgData(float rate, float pace, float power, bool connected)
    {
        StrokeRate  = rate;
        PaceSeconds = pace;
        PowerWatts  = power;
        IsConnected = connected;
    }

    void OnApplicationQuit()
    {
        if (_bridgeProcess != null && !_bridgeProcess.HasExited)
        {
            _bridgeProcess.Kill();
            UnityEngine.Debug.Log("[ErgManager] ErgBridge killed.");
        }
    }
}