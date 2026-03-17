using UnityEngine;
using UnityEngine.UI;
using TMPro;

/// <summary>
/// Drives all UI elements for the Boulder QTE game.
///
/// Setup in the Inspector:
///   1. Assign each panel (startPanel, countdownPanel, gameplayPanel, winPanel, losePanel)
///   2. Wire up every named field inside those panels
///   3. The QTE bar uses a single Image whose fill/color shows the timing window
/// </summary>
[RequireComponent(typeof(Canvas))]
public class BoulderHUD : MonoBehaviour
{
    // ── External References ───────────────────────────────────────────────────
    [Header("Game References (auto-found if blank)")]
    public BoulderGameManager manager;
    public QTEController      qte;
    public BoulderController  boulder;

    // ── Panels ────────────────────────────────────────────────────────────────
    [Header("Panels")]
    public GameObject startPanel;
    public GameObject countdownPanel;
    public GameObject gameplayPanel;
    public GameObject winPanel;
    public GameObject losePanel;

    // ── Start Screen ──────────────────────────────────────────────────────────
    [Header("Start Screen")]
    public Button     startButton;
    public TMP_Text   startTitleText;

    // ── Countdown ─────────────────────────────────────────────────────────────
    [Header("Countdown")]
    public TMP_Text countdownText;

    // ── QTE Bar ───────────────────────────────────────────────────────────────
    [Header("QTE Timing Bar")]
    [Tooltip("Cycle track — set Image type=Filled, FillMethod=Horizontal")]
    public Image    qteCycleTrack;

    [Tooltip("Green/Red window indicator — colored image that flashes")]
    public Image    qteWindowFlash;

    [Tooltip("'PUSH NOW!' / 'WAIT...' label")]
    public TMP_Text qteStatusText;

    // ── Progress ──────────────────────────────────────────────────────────────
    [Header("Boulder Progress")]
    public Slider   progressSlider;
    public TMP_Text progressLabel;        // "47%"

    // ── Stats ─────────────────────────────────────────────────────────────────
    [Header("In-Game Stats")]
    public TMP_Text timerText;
    public TMP_Text repCountText;
    public TMP_Text comboText;
    public TMP_Text ergStatusText;
    public TMP_Text powerZoneText;   // shows "EXPLOSIVE" / "POWER" / "MODERATE" / "LOW"

    // ── Push Feedback Popup ───────────────────────────────────────────────────
    [Header("Push Feedback Popup")]
    [Tooltip("The TMP text that shows PERFECT! / PUSH! etc.")]
    public TMP_Text        feedbackText;

    [Tooltip("RectTransform of feedbackText; floats upward after a push.")]
    public RectTransform   feedbackRect;

    [Tooltip("Anchor position feedbackRect resets to each push.")]
    public Vector2         feedbackAnchorPos = new Vector2(0f, 0f);

    // ── Win / Lose ────────────────────────────────────────────────────────────
    [Header("End Screens")]
    public TMP_Text winBodyText;
    public Button   winRestartButton;
    public TMP_Text loseBodyText;
    public Button   loseRestartButton;

    // ── Colors ────────────────────────────────────────────────────────────────
    [Header("Colors")]
    public Color colorPerfect      = new Color(0.20f, 1.00f, 0.35f);
    public Color colorMiss         = new Color(1.00f, 0.45f, 0.10f);
    public Color colorWindowOpen   = new Color(0.10f, 0.95f, 0.25f, 0.90f);
    public Color colorWindowClosed = new Color(0.90f, 0.20f, 0.15f, 0.55f);
    public Color timerWarning      = new Color(1.00f, 0.20f, 0.10f);
    public Color zoneExplosive     = new Color(1.00f, 0.20f, 0.10f);   // red
    public Color zonePower         = new Color(1.00f, 0.60f, 0.10f);   // orange
    public Color zoneModerate      = new Color(0.95f, 0.90f, 0.10f);   // yellow
    public Color zoneLow           = new Color(0.70f, 0.70f, 0.70f);   // grey

    // ── Private ───────────────────────────────────────────────────────────────
    private float _feedbackTimer;
    private float _feedbackDuration = 1.1f;
    private Vector2 _feedbackStartPos;

    // ─────────────────────────────────────────────────────────────────────────

    void Awake()
    {
        HideAllPanels();
    }

    void Start()
    {
        // Auto-find if not assigned
        if (!manager) manager = BoulderGameManager.Instance;
        if (!qte)     qte     = FindObjectOfType<QTEController>();
        if (!boulder) boulder = FindObjectOfType<BoulderController>();

        if (startButton)       startButton.onClick.AddListener(    () => manager?.StartGame());
        if (winRestartButton)  winRestartButton.onClick.AddListener( () => manager?.RestartGame());
        if (loseRestartButton) loseRestartButton.onClick.AddListener(() => manager?.RestartGame());

        // Match all button sizes to the start button
        if (startButton)
        {
            RectTransform startRT = startButton.GetComponent<RectTransform>();
            if (startRT)
            {
                Vector2 sz = startRT.sizeDelta;
                if (winRestartButton)
                {
                    var rt = winRestartButton.GetComponent<RectTransform>();
                    if (rt) rt.sizeDelta = sz;
                }
                if (loseRestartButton)
                {
                    var rt = loseRestartButton.GetComponent<RectTransform>();
                    if (rt) rt.sizeDelta = sz;
                }
            }
        }

        if (startTitleText)
            startTitleText.text = "PUSH\nTHE BOULDER\n\n<size=60%>Row to Start</size>";
    }

    void Update()
    {
        if (manager == null) return;

        if (manager.State == GameState.Playing)
            UpdateGameplayUI();

        TickFeedbackPopup();
    }

    // ─────────────────────────────────────────────────────────────────────────
    //  Panel Switchers (called by BoulderGameManager)
    // ─────────────────────────────────────────────────────────────────────────

    public void ShowStartScreen()
    {
        HideAllPanels();
        SetActive(startPanel, true);
    }

    public void ShowCountdown()
    {
        HideAllPanels();
        SetActive(countdownPanel, true);
        if (countdownText) countdownText.text = "3";
    }

    public void UpdateCountdown(float remaining)
    {
        if (!countdownText) return;
        int n = Mathf.CeilToInt(Mathf.Max(remaining, 0f));
        countdownText.text = n > 0 ? n.ToString() : "GO!";
    }

    public void ShowGameplay()
    {
        HideAllPanels();
        SetActive(gameplayPanel, true);
        ResetFeedbackPopup();
    }

    public void ShowWinScreen(int reps, int perfects, float timeUsed,
                              float avgPower, float peakPower)
    {
        HideAllPanels();
        SetActive(winPanel, true);
        if (winBodyText)
        {
            winBodyText.alignment       = TMPro.TextAlignmentOptions.Center;
            winBodyText.enableWordWrapping = false;
            winBodyText.text = $"BOULDER REACHED THE TOP!\n" +
                               $"Reps: {reps}  |  Perfects: {perfects}  |  Time: {FormatTime(timeUsed)}\n" +
                               $"Avg Power: {avgPower:F1}  |  Peak: {peakPower:F1}";
        }
        PinEndScreen(winBodyText, winRestartButton);
    }

    public void ShowLoseScreen(float progress, int reps,
                               float avgPower, float peakPower)
    {
        HideAllPanels();
        SetActive(losePanel, true);
        if (loseBodyText)
        {
            loseBodyText.alignment        = TMPro.TextAlignmentOptions.Center;
            loseBodyText.enableWordWrapping = false;
            loseBodyText.text = $"The boulder rolled back...\n" +
                                $"{(progress * 100f):F0}% up the hill  |  Reps: {reps}\n" +
                                $"Avg Power: {avgPower:F1}  |  Peak: {peakPower:F1}";
        }
        PinEndScreen(loseBodyText, loseRestartButton);
    }

    // Anchors text to the top half and button to the bottom of an end screen panel.
    private static void PinEndScreen(TMPro.TMP_Text bodyText, Button button)
    {
        if (bodyText)
        {
            var rt = bodyText.GetComponent<RectTransform>();
            if (rt)
            {
                rt.anchorMin = new Vector2(0f,   0.35f);
                rt.anchorMax = new Vector2(1f,   1f);
                rt.offsetMin = new Vector2(20f,  0f);
                rt.offsetMax = new Vector2(-20f, -20f);
            }
        }
        if (button)
        {
            var rt = button.GetComponent<RectTransform>();
            if (rt)
            {
                rt.anchorMin = new Vector2(0.5f, 0f);
                rt.anchorMax = new Vector2(0.5f, 0f);
                rt.pivot     = new Vector2(0.5f, 0f);
                rt.anchoredPosition = new Vector2(0f, 20f);
            }
        }
    }

    // ─────────────────────────────────────────────────────────────────────────
    //  Push Feedback Popup (called by BoulderGameManager)
    // ─────────────────────────────────────────────────────────────────────────

    public void ShowPushFeedback(QTEController.PushRating rating,
                                 PowerZone zone, float repPower, int combo)
    {
        if (feedbackText == null) return;

        bool perfect = rating == QTEController.PushRating.Perfect;

        // Zone label — exercise science feedback on effort level
        string zoneLabel = zone switch
        {
            PowerZone.Explosive => "EXPLOSIVE",
            PowerZone.Power     => "POWER",
            PowerZone.Moderate  => "PUSH",
            _                   => "weak..."
        };

        string label = perfect ? $"PERFECT  {zoneLabel}!" : $"{zoneLabel}!";
        if (combo > 2) label = $"x{combo}  {label}";

        feedbackText.text = label;
        Color c = ZoneColor(zone);
        if (perfect) c = Color.Lerp(c, colorPerfect, 0.5f);
        c.a = 1f;
        feedbackText.color = c;

        ResetFeedbackPopup();
        _feedbackTimer = _feedbackDuration;
    }

    // ─────────────────────────────────────────────────────────────────────────
    //  Gameplay UI Update (called every frame while Playing)
    // ─────────────────────────────────────────────────────────────────────────

    private void UpdateGameplayUI()
    {
        //  QTE Timing Bar
        bool   windowOpen = qte != null && qte.IsWindowOpen;
        float  cycleProg  = qte != null ? qte.CycleProgress : 0f;

        if (qteCycleTrack)
        {
            qteCycleTrack.fillAmount = cycleProg;
            qteCycleTrack.color = windowOpen ? colorWindowOpen : colorWindowClosed;
        }

        if (qteWindowFlash)
        {
            if (windowOpen)
            {
                float pulse = 1f + 0.07f * Mathf.Sin(Time.time * 10f);
                qteWindowFlash.transform.localScale = Vector3.one * pulse;
                qteWindowFlash.color = colorWindowOpen;
            }
            else
            {
                qteWindowFlash.transform.localScale = Vector3.one;
                qteWindowFlash.color = colorWindowClosed;
            }
        }

        if (qteStatusText)
        {
            qteStatusText.text  = windowOpen ? "PUSH NOW!" : "WAIT...";
            qteStatusText.color = windowOpen ? colorPerfect : Color.gray;
        }

        // Boulder Progress
        float boulderProg = boulder != null ? boulder.Progress : 0f;
        if (progressSlider) progressSlider.value = boulderProg;
        if (progressLabel)  progressLabel.text   = $"{(boulderProg * 100f):F0}%";

        // Timer
        if (timerText)
        {
            float t = manager.TimeRemaining;
            timerText.text  = FormatTime(t);
            timerText.color = t < 15f ? timerWarning : Color.white;
        }

        // Reps / Combo
        if (repCountText) repCountText.text = $"Reps: {manager.TotalReps}";
        if (comboText)
        {
            bool showCombo = manager.Combo > 1;
            comboText.gameObject.SetActive(showCombo);
            if (showCombo)
            {
                comboText.text  = $"COMBO  x{manager.Combo}";
                float glow = Mathf.Sin(Time.time * 6f) * 0.15f + 0.85f;
                Color cc = Color.Lerp(Color.white, colorPerfect,
                                      Mathf.Min(manager.Combo / 6f, 1f));
                cc.a = glow;
                comboText.color = cc;
            }
        }

        // Power Zone
        if (powerZoneText && manager.TotalReps > 0)
        {
            string zoneName = manager.CurrentZone switch
            {
                PowerZone.Explosive => "EXPLOSIVE",
                PowerZone.Power     => "POWER ZONE",
                PowerZone.Moderate  => "MODERATE",
                _                   => "LOW"
            };
            powerZoneText.text  = $"{zoneName}  {manager.CurrentPower:F1}";
            powerZoneText.color = ZoneColor(manager.CurrentZone);
        }

        // ERG Connection Status
        if (ergStatusText)
        {
            var erg = Concept2UsbReader.Instance;
            if (erg != null)
            {
                bool conn = erg.IsConnected;
                ergStatusText.text  = conn
                    ? $"ERG  {(erg.HeartRate > 0 ? erg.HeartRate + " bpm" : "connected")}"
                    : "ERG  disconnected";
                ergStatusText.color = conn ? Color.green : Color.red;
            }
            else
            {
                ergStatusText.text  = "SIM MODE";
                ergStatusText.color = Color.yellow;
            }
        }
    }

    // ─────────────────────────────────────────────────────────────────────────
    //  Helpers
    // ─────────────────────────────────────────────────────────────────────────

    private Color ZoneColor(PowerZone zone) => zone switch
    {
        PowerZone.Explosive => zoneExplosive,
        PowerZone.Power     => zonePower,
        PowerZone.Moderate  => zoneModerate,
        _                   => zoneLow
    };

    private void TickFeedbackPopup()
    {
        if (_feedbackTimer <= 0f || feedbackRect == null) return;

        _feedbackTimer -= Time.deltaTime;

        // Float upward
        feedbackRect.anchoredPosition = _feedbackStartPos +
            Vector2.up * (1f - _feedbackTimer / _feedbackDuration) * 80f;

        // Fade out over the last 0.5 sec
        if (feedbackText)
        {
            Color c = feedbackText.color;
            c.a = Mathf.Clamp01(_feedbackTimer / (_feedbackDuration * 0.45f));
            feedbackText.color = c;
        }
    }

    private void ResetFeedbackPopup()
    {
        if (feedbackRect)
        {
            feedbackRect.anchoredPosition = feedbackAnchorPos;
            _feedbackStartPos = feedbackAnchorPos;
        }
        if (feedbackText)
        {
            feedbackText.text = "";
            Color c = feedbackText.color;
            c.a = 0f;
            feedbackText.color = c;
        }
        _feedbackTimer = 0f;
    }

    private void HideAllPanels()
    {
        SetActive(startPanel,     false);
        SetActive(countdownPanel, false);
        SetActive(gameplayPanel,  false);
        SetActive(winPanel,       false);
        SetActive(losePanel,      false);
    }

    private static void SetActive(GameObject go, bool active)
    {
        if (go) go.SetActive(active);
    }

    private static string FormatTime(float secs)
    {
        int m = Mathf.FloorToInt(secs / 60f);
        float s = secs % 60f;
        return $"{m:00}:{s:00.0}";
    }
}
