using UnityEngine;
using UnityEngine.Events;

/// <summary>
/// Central game-state machine for the Boulder QTE.
///
/// Data wired in from StrengthErg PM5 (via Concept2UsbReader):
///   RepCount     → total work done / endurance tracking
///   PullDistance → how far the boulder moves (raw cable pull units)
///   RepTimeSec   → drive speed → explosive force multiplier
///   (HeartRate available on Concept2UsbReader.Instance for future use)
/// </summary>
public class BoulderGameManager : MonoBehaviour
{
    public static BoulderGameManager Instance { get; private set; }

    // ── Scene References ──────────────────────────────────────────────────────
    [Header("Scene References")]
    public BoulderController    boulder;
    public QTEController        qte;
    public BoulderHUD           hud;
    public PlayerPusherController player;

    // ── Game Rules ────────────────────────────────────────────────────────────
    [Header("Game Rules")]
    [Tooltip("Total time limit for a run (seconds).")]
    public float timeLimitSeconds = 90f;

    [Tooltip("Seconds of idle before boulder starts rolling back.")]
    public float rollbackDelaySec = 2.5f;

    [Tooltip("How long the QTE bonus window stays open after animation peak. " +
             "Match this to roughly half your push animation length.")]
    public float animationWindowDuration = 0.5f;

    [Tooltip("Seconds for the displayed countdown before the run starts.")]
    public float countdownDuration = 3f;

    // ── Power Zones ───────────────────────────────────────────────────────────
    [Header("Power Zones (pullDistance / repTimeSec thresholds)")]
    [Tooltip("Power above this = Explosive (red)")]
    public float explosiveThreshold = 70f;
    [Tooltip("Power above this = Power zone (orange)")]
    public float powerThreshold     = 45f;
    [Tooltip("Power above this = Moderate (yellow)")]
    public float moderateThreshold  = 25f;
    // Below moderateThreshold = Low (white)
    [Tooltip("When true, Space bar fires a simulated rep.")]
    public bool simulateInput = false;

    [Tooltip("Simulated pull distance (raw PM5 units). 50–120 is typical.")]
    public int   simPullDistance = 80;

    [Tooltip("Simulated rep drive time (seconds). 0.3–0.7 is normal.")]
    public float simRepTimeSec   = 0.45f;

    // ── Events ────────────────────────────────────────────────────────────────
    [Header("Unity Events")]
    public UnityEvent onGameWon;
    public UnityEvent onGameLost;

    // ── Public Read-Only State ────────────────────────────────────────────────
    public GameState State         { get; private set; }
    public float     TimeRemaining { get; private set; }
    public int       TotalReps     { get; private set; }
    public int       Combo         { get; private set; }
    public int       PerfectCount  { get; private set; }
    public float     CurrentPower  { get; private set; }
    public float     PeakPower     { get; private set; }
    public PowerZone CurrentZone   { get; private set; }
    public float     AveragePower  => TotalReps > 0 ? _totalPower / TotalReps : 0f;

    // ── Private ───────────────────────────────────────────────────────────────
    private Concept2UsbReader _erg;
    private float _timeSinceLastRep;
    private float _countdownTimer;
    private int   _simRepNumber;
    private float _totalPower;

    // ─────────────────────────────────────────────────────────────────────────

    void Awake()
    {
        if (Instance != null && Instance != this)
        {
            Destroy(gameObject);
            return;
        }
        Instance = this;
    }

    void Start()
    {
        _erg = Concept2UsbReader.Instance;
        if (_erg != null)
        {
            _erg.OnNewRep += HandleNewRep;
            Debug.Log("[BoulderGame] Subscribed to ERG OnNewRep successfully.");
        }
        else
        {
            Debug.LogWarning("[BoulderGame] Concept2UsbReader not found — ERG pushes will not work. Enable Simulate Input for keyboard testing.");
        }

        if (player != null)
            player.OnPushPeak += OnAnimationPeak;

        ChangeState(GameState.Idle);
    }

    void OnDestroy()
    {
        if (_erg != null)
            _erg.OnNewRep -= HandleNewRep;

        if (player != null)
            player.OnPushPeak -= OnAnimationPeak;
    }

    // ─────────────────────────────────────────────────────────────────────────
    //  Public Controls
    // ─────────────────────────────────────────────────────────────────────────

    public void StartGame()
    {
        if (State == GameState.Playing || State == GameState.Countdown)
            return;

        boulder?.ResetPosition();

        TotalReps     = 0;
        Combo         = 0;
        PerfectCount  = 0;
        CurrentPower  = 0f;
        PeakPower     = 0f;
        _totalPower   = 0f;
        _timeSinceLastRep = 0f;
        _simRepNumber = 0;

        _countdownTimer = countdownDuration;
        ChangeState(GameState.Countdown);
    }

    public void RestartGame() => StartGame();

    // ─────────────────────────────────────────────────────────────────────────
    //  Update Loop
    // ─────────────────────────────────────────────────────────────────────────

    void Update()
    {
        switch (State)
        {
            case GameState.Countdown:
                _countdownTimer -= Time.deltaTime;
                hud?.UpdateCountdown(_countdownTimer);
                if (_countdownTimer <= 0f)
                {
                    TimeRemaining = timeLimitSeconds;
                    if (qte) qte.enabled = true;
                    ChangeState(GameState.Playing);
                }
                break;

            case GameState.Playing:
                TimeRemaining     -= Time.deltaTime;
                _timeSinceLastRep += Time.deltaTime;

                // Rollback when idle too long
                if (_timeSinceLastRep > rollbackDelaySec)
                    boulder?.ApplyRollback(Time.deltaTime);

                // Keyboard simulation
                if (simulateInput && Input.GetKeyDown(KeyCode.Space))
                    HandleNewRep(++_simRepNumber, simRepTimeSec, simPullDistance);

                // Win/Lose checks
                if (boulder != null && boulder.IsAtTop)
                    EndGame(true);
                else if (TimeRemaining <= 0f)
                    EndGame(false);
                break;
        }
    }

    // ─────────────────────────────────────────────────────────────────────────
    //  Rep Handler (called from ERG event or keyboard sim)
    // ─────────────────────────────────────────────────────────────────────────

    private void HandleNewRep(int repNumber, float repTimeSec, int pullDistance)
    {
        Debug.Log($"[BoulderGame] Rep received — #{repNumber}, state={State}, pull={pullDistance}");

        // Auto-start: first row on the Idle screen kicks off the countdown
        if (State == GameState.Idle)
        {
            StartGame();
            return;
        }

        if (State != GameState.Playing) return;

        _timeSinceLastRep = 0f;
        TotalReps++;

        // 1. Rep power — rewards explosive intent regardless of load setting
        float repPower = pullDistance / Mathf.Max(repTimeSec, 0.05f);
        CurrentPower   = repPower;
        _totalPower   += repPower;
        if (repPower > PeakPower) PeakPower = repPower;

        // 2. Classify power zone
        CurrentZone = repPower >= explosiveThreshold ? PowerZone.Explosive
                    : repPower >= powerThreshold     ? PowerZone.Power
                    : repPower >= moderateThreshold  ? PowerZone.Moderate
                    :                                  PowerZone.Low;

        // 3. QTE timing evaluation — pass repTimeSec so it checks drive-start, not rep-arrival
        var (rating, timingMult) = qte != null
            ? qte.EvaluatePush(repTimeSec)
            : (QTEController.PushRating.Miss, 0.8f);

        // 4. Combo tracker
        if (rating == QTEController.PushRating.Perfect) { Combo++; PerfectCount++; }
        else                                              Combo = 0;
        float comboBonus = 1f + Mathf.Min(Mathf.Max(Combo - 1, 0) * 0.10f, 0.50f);

        // 5. Combined multiplier
        float combined = timingMult * comboBonus;

        // 6. Push the boulder with power-based gain
        boulder?.Push(repPower, combined);

        // 7. Trigger animation synced to rep time
        player?.TriggerPushAnim(repTimeSec);

        // 8. Notify HUD
        hud?.ShowPushFeedback(rating, CurrentZone, repPower, Combo);
    }

    // ─────────────────────────────────────────────────────────────────────────
    //  Animation Peak Handler
    // ─────────────────────────────────────────────────────────────────────────

    /// <summary>
    /// Fires when the push animation hits its peak effort moment.
    /// Opens the QTE bonus window so the next push earns the timing bonus
    /// if the player pushes in rhythm with the animation.
    /// </summary>
    private void OnAnimationPeak()
    {
        if (State != GameState.Playing) return;
        qte?.ForceOpenWindow(animationWindowDuration);
    }

    // ─────────────────────────────────────────────────────────────────────────
    //  State Management
    // ─────────────────────────────────────────────────────────────────────────

    private void ChangeState(GameState next)
    {
        State = next;
        switch (next)
        {
            case GameState.Idle:
                if (qte) qte.enabled = false;
                hud?.ShowStartScreen();
                break;

            case GameState.Countdown:
                if (qte) qte.enabled = false;
                hud?.ShowCountdown();
                break;

            case GameState.Playing:
                hud?.ShowGameplay();
                break;

            case GameState.Won:
            case GameState.Lost:
                if (qte) qte.enabled = false;
                break;
        }
    }

    private void EndGame(bool won)
    {
        if (State != GameState.Playing) return;

        float progress = boulder != null ? boulder.Progress : 0f;
        float timeUsed = timeLimitSeconds - TimeRemaining;

        if (won)
        {
            ChangeState(GameState.Won);
            hud?.ShowWinScreen(TotalReps, PerfectCount, timeUsed, AveragePower, PeakPower);
            onGameWon?.Invoke();
        }
        else
        {
            ChangeState(GameState.Lost);
            hud?.ShowLoseScreen(progress, TotalReps, AveragePower, PeakPower);
            onGameLost?.Invoke();
        }
    }
}

// ── State Enum ─────────────────────────────────────────────────────────────────
public enum GameState  { Idle, Countdown, Playing, Won, Lost }
public enum PowerZone  { Low, Moderate, Power, Explosive }
