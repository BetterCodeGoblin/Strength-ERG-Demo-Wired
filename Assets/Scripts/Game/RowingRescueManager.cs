using UnityEngine;
using TMPro;

/// <summary>
/// Minimal overnight rowing rescue loop for tomorrow's PM5 demo.
///
/// Goal:
/// - Reuse existing ErgBridge/PM5 transport already proven in this project.
/// - Show a real rowing-driven progress loop with live SPM/pace/power feedback.
/// - Avoid deep dependencies on the shelved Imagine Unity scaffold.
///
/// Wiring:
/// - Add this to a GameObject in a simple rowing scene.
/// - Ensure an ErgManager exists in scene, or one is auto-found.
/// - Optionally assign TMP labels for status/debug.
/// - Press Start Session in inspector or call StartSession() from a UI button.
///
/// Demo loop:
/// - Each new stroke contributes progress based on watts.
/// - Must stay above min SPM to advance.
/// - Better pacing/form cadence earns a modest bonus.
/// - If strokes stop for too long, progress drifts backward.
/// </summary>
public class RowingRescueManager : MonoBehaviour
{
    [Header("References")]
    public ErgManager ergManager;

    [Header("Session Rules")]
    public bool autoStartOnFirstStroke = true;
    public float sessionLengthSeconds = 180f;
    public float targetDistanceMeters = 250f;
    public float minSPMForProgress = 14f;
    public float metersPerWatt = 0.05f;
    public float driftDelaySeconds = 2.0f;
    public float driftMetersPerSecond = 1.2f;

    [Header("Cadence Bonus")]
    public float idealLowSPM = 20f;
    public float idealHighSPM = 28f;
    public float idealLowPaceSeconds = 120f;
    public float idealHighPaceSeconds = 180f;
    public float cadenceBonusMultiplier = 1.2f;

    [Header("Optional UI")]
    public TMP_Text statusText;
    public TMP_Text timerText;
    public TMP_Text progressText;
    public TMP_Text telemetryText;
    public TMP_Text resultText;

    public bool IsSessionActive { get; private set; }
    public bool IsSessionComplete { get; private set; }
    public float SessionElapsedSeconds { get; private set; }
    public float ProgressMeters { get; private set; }
    public int StrokeCount { get; private set; }
    public float LastStrokePower { get; private set; }
    public float LastStrokeSPM { get; private set; }
    public float LastStrokePaceSeconds { get; private set; }

    private float _timeSinceLastStroke = 999f;
    private float _bestPower;

    void Start()
    {
        if (!ergManager) ergManager = FindObjectOfType<ErgManager>();
        RefreshUI();
    }

    void Update()
    {
        if (!ergManager)
        {
            if (statusText) statusText.text = "ERG manager missing";
            return;
        }

        float spm = ergManager.StrokeRate;
        float pace = ergManager.PaceSeconds;
        float watts = ergManager.PowerWatts;

        if (spm > 0.1f)
        {
            bool newStrokeLikely = _timeSinceLastStroke > Mathf.Max(0.5f, 60f / Mathf.Max(spm, 1f) * 0.55f);
            if (newStrokeLikely)
            {
                HandleStroke(spm, pace, watts);
            }
        }

        _timeSinceLastStroke += Time.deltaTime;

        if (IsSessionActive)
        {
            SessionElapsedSeconds += Time.deltaTime;

            if (_timeSinceLastStroke > driftDelaySeconds)
            {
                ProgressMeters = Mathf.Max(0f, ProgressMeters - driftMetersPerSecond * Time.deltaTime);
            }

            if (ProgressMeters >= targetDistanceMeters)
            {
                CompleteSession(true);
            }
            else if (SessionElapsedSeconds >= sessionLengthSeconds)
            {
                CompleteSession(false);
            }
        }

        LastStrokeSPM = spm;
        LastStrokePaceSeconds = pace;
        LastStrokePower = watts;
        RefreshUI();
    }

    public void StartSession()
    {
        IsSessionActive = true;
        IsSessionComplete = false;
        SessionElapsedSeconds = 0f;
        ProgressMeters = 0f;
        StrokeCount = 0;
        _timeSinceLastStroke = 999f;
        _bestPower = 0f;
        if (resultText) resultText.text = "";
        RefreshUI();
    }

    public void ResetSession()
    {
        IsSessionActive = false;
        IsSessionComplete = false;
        SessionElapsedSeconds = 0f;
        ProgressMeters = 0f;
        StrokeCount = 0;
        _timeSinceLastStroke = 999f;
        _bestPower = 0f;
        if (resultText) resultText.text = "";
        RefreshUI();
    }

    private void HandleStroke(float spm, float paceSeconds, float watts)
    {
        _timeSinceLastStroke = 0f;

        if (!IsSessionActive)
        {
            if (autoStartOnFirstStroke)
            {
                StartSession();
            }
            else
            {
                return;
            }
        }

        StrokeCount++;
        LastStrokeSPM = spm;
        LastStrokePaceSeconds = paceSeconds;
        LastStrokePower = watts;
        _bestPower = Mathf.Max(_bestPower, watts);

        if (spm < minSPMForProgress)
        {
            if (statusText) statusText.text = "Stroke rate too low, drive harder";
            return;
        }

        float baseProgress = watts * metersPerWatt;
        float cadenceBonus = EvaluateCadenceBonus(spm, paceSeconds);
        ProgressMeters += baseProgress * cadenceBonus;
    }

    private float EvaluateCadenceBonus(float spm, float paceSeconds)
    {
        bool spmGood = spm >= idealLowSPM && spm <= idealHighSPM;
        bool paceGood = paceSeconds >= idealLowPaceSeconds && paceSeconds <= idealHighPaceSeconds;

        if (spmGood && paceGood) return cadenceBonusMultiplier;
        if (spmGood || paceGood) return Mathf.Lerp(1f, cadenceBonusMultiplier, 0.5f);
        return 1f;
    }

    private void CompleteSession(bool success)
    {
        IsSessionActive = false;
        IsSessionComplete = true;

        if (!resultText) return;

        if (success)
        {
            resultText.text = $"SUMMIT REACHED\n{ProgressMeters:F0}m in {FormatTime(SessionElapsedSeconds)}\nStrokes: {StrokeCount}  Peak: {_bestPower:F0}W";
        }
        else
        {
            resultText.text = $"SESSION COMPLETE\n{ProgressMeters:F0}/{targetDistanceMeters:F0}m\nStrokes: {StrokeCount}  Peak: {_bestPower:F0}W";
        }
    }

    private void RefreshUI()
    {
        if (statusText)
        {
            string erg = ergManager
                ? (ergManager.IsConnected ? "PM5 connected" : "Waiting for PM5")
                : "ERG manager missing";
            string mode = IsSessionActive ? "Rowing active" : "Ready to row";
            statusText.text = $"{mode} | {erg}";
        }

        if (timerText)
        {
            float remaining = Mathf.Max(0f, sessionLengthSeconds - SessionElapsedSeconds);
            timerText.text = $"Time {FormatTime(remaining)}";
        }

        if (progressText)
        {
            progressText.text = $"Progress {ProgressMeters:F0}/{targetDistanceMeters:F0} m";
        }

        if (telemetryText)
        {
            telemetryText.text = $"SPM {LastStrokeSPM:F0} | Pace {FormatPace(LastStrokePaceSeconds)} | Power {LastStrokePower:F0}W | Strokes {StrokeCount}";
        }
    }

    private static string FormatTime(float seconds)
    {
        int total = Mathf.Max(0, Mathf.RoundToInt(seconds));
        return $"{total / 60}:{total % 60:D2}";
    }

    private static string FormatPace(float paceSeconds)
    {
        if (paceSeconds <= 0f) return "--:--/500";
        int total = Mathf.RoundToInt(paceSeconds);
        return $"{total / 60}:{total % 60:D2}/500";
    }
}
