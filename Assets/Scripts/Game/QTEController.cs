using UnityEngine;
using System;

/// <summary>
/// Manages the "PUSH NOW" timing window.
///
/// Two ways the window can open:
///   1. Fixed cycle timer  — pulses on a regular rhythm (original behaviour)
///   2. Animation-driven   — BoulderGameManager calls ForceOpenWindow() when
///                           the push animation reaches its peak effort moment,
///                           so pushing in rhythm with the character earns the bonus
/// </summary>
public class QTEController : MonoBehaviour
{
    [Header("Cycle Timing (fallback rhythm)")]
    [Tooltip("Total length of one QTE cycle in seconds")]
    public float cycleDuration = 3.0f;

    [Tooltip("How long the green window stays open per cycle")]
    public float windowDuration = 1.2f;

    [Tooltip("Fraction into the cycle at which the window opens (0..1)")]
    [Range(0f, 0.9f)]
    public float windowStartFraction = 0.10f;

    [Header("Push Multipliers")]
    [Tooltip("Force multiplier when pushing inside the green window")]
    public float perfectMultiplier = 1.5f;

    [Tooltip("Force multiplier when pushing outside the green window")]
    public float missMultiplier = 0.55f;

    // ── Public State ──────────────────────────────────────────────────────────
    /// <summary>True while the green push window is open (either source).</summary>
    public bool IsWindowOpen { get; private set; }

    /// <summary>0–1 fraction through the current cycle (for HUD bar animation).</summary>
    public float CycleProgress { get; private set; }

    /// <summary>0–1 fraction through the open window (0 when closed).</summary>
    public float WindowProgress { get; private set; }

    // ── Events ────────────────────────────────────────────────────────────────
    public event Action OnWindowOpened;
    public event Action OnWindowClosed;

    // ── Push Rating ───────────────────────────────────────────────────────────
    public enum PushRating { Perfect, Miss }

    // ── Private ───────────────────────────────────────────────────────────────
    private bool  _previouslyOpen;
    private float _forcedWindowTimer;   // counts down when animation-driven window is open

    // ─────────────────────────────────────────────────────────────────────────

    void Update()
    {
        // ── Cycle-based window ────────────────────────────────────────────────
        float cycleTime   = Time.time % cycleDuration;
        CycleProgress     = cycleTime / cycleDuration;

        float windowOpen  = windowStartFraction * cycleDuration;
        float windowClose = windowOpen + windowDuration;
        bool  cycleOpen   = (cycleTime >= windowOpen && cycleTime < windowClose);

        if (cycleOpen)
            WindowProgress = (cycleTime - windowOpen) / windowDuration;

        // ── Animation-driven window ───────────────────────────────────────────
        if (_forcedWindowTimer > 0f)
        {
            _forcedWindowTimer -= Time.deltaTime;
            WindowProgress = Mathf.Clamp01(1f - (_forcedWindowTimer /
                             Mathf.Max(_forcedWindowTimer + Time.deltaTime, 0.001f)));
        }

        // ── Combine both sources ──────────────────────────────────────────────
        IsWindowOpen = cycleOpen || _forcedWindowTimer > 0f;
        if (!IsWindowOpen) WindowProgress = 0f;

        // Fire edge events
        if (IsWindowOpen && !_previouslyOpen)
            OnWindowOpened?.Invoke();
        else if (!IsWindowOpen && _previouslyOpen)
            OnWindowClosed?.Invoke();

        _previouslyOpen = IsWindowOpen;
    }

    /// <summary>
    /// Opens the bonus window for a set duration.
    /// Called by BoulderGameManager when the push animation hits its peak,
    /// giving the player a chance to earn the timing bonus by pushing in rhythm.
    /// </summary>
    public void ForceOpenWindow(float duration)
    {
        _forcedWindowTimer = duration;
    }

    /// <summary>
    /// Call this when a rep fires. Compensates for PM5 latency by evaluating
    /// whether the drive START (Time.time - repTimeSec) fell inside the window,
    /// rather than the moment the rep data arrives.
    /// </summary>
    public (PushRating rating, float multiplier) EvaluatePush(float repTimeSec = 0f)
    {
        // Estimate when the drive actually began
        float evalTime  = Time.time - repTimeSec;

        // Cycle-based window check at drive-start time
        float cycleTime = evalTime % cycleDuration;
        float wStart    = windowStartFraction * cycleDuration;
        float wEnd      = wStart + windowDuration;
        bool  inCycle   = cycleTime >= wStart && cycleTime < wEnd;

        // Forced/animation window: was it still open when the drive started?
        bool inForced = (_forcedWindowTimer + repTimeSec) > 0f;

        return (inCycle || inForced)
            ? (PushRating.Perfect, perfectMultiplier)
            : (PushRating.Miss,    missMultiplier);
    }
}
