using UnityEngine;

/// <summary>
/// Moves the boulder along a straight path between two Transforms.
/// Progress 0 = bottom of hill, 1 = top.
///
/// Data mix driving movement:
///   Pull Distance  → base movement magnitude (longer cable pull = more distance)
///   Rep Time       → explosive speed factor  (faster drive = more force)
///   Timing mult    → QTE multiplier from BoulderGameManager (perfect/miss)
/// </summary>
public class BoulderController : MonoBehaviour
{
    [Header("Path (assign two empty GameObjects)")]
    [Tooltip("Empty GO at the foot of the hill")]
    public Transform pathStart;
    [Tooltip("Empty GO at the top of the hill")]
    public Transform pathEnd;

    [Header("Push Tuning")]
    [Tooltip("Scales raw rep power (pullDistance / repTimeSec) into progress gain. "
           + "At 0.00028: an average push (80 pull / 0.45s) = ~5% progress → ~20 reps to win.")]
    public float powerScale = 0.00028f;

    [Tooltip("Clamps maximum progress gain from a single rep.")]
    public float maxPushPerRep = 0.10f;

    [Header("Rollback")]
    [Tooltip("Progress units lost per second when rolling back.")]
    public float rollbackSpeed = 0.022f;

    [Header("Visuals")]
    [Tooltip("Position lerp speed (higher = snappier).")]
    public float visualSmoothSpeed = 9f;

    [Tooltip("Lifts the boulder perpendicular to the slope to prevent clipping. " +
             "Tune this in the Inspector until the boulder sits flush on the surface.")]
    public float heightOffset = 0f;

    [Tooltip("When enabled, the height offset scales from 0 at the bottom to full at the top, " +
             "matching terrain that is flatter at the base and steeper higher up.")]
    public bool rampOffsetAlongPath = true;

    [Tooltip("Rotation speed in degrees per progress unit (makes it look like rolling).")]
    public float rollDegreesPerProgress = 720f;

    [Tooltip("Short wobble angle (degrees) injected on each push.")]
    public float pushWobbleDegrees = 6f;

    // ── Public State ──────────────────────────────────────────────────────────
    /// <summary>Logical position: 0 = bottom, 1 = top.</summary>
    public float Progress { get; private set; }

    public bool IsAtTop    => Progress >= 0.999f;
    public bool IsAtBottom => Progress <= 0.001f;

    // ── Private ───────────────────────────────────────────────────────────────
    private float      _visualProgress;
    private float      _visualProgressVelocity;          // used by SmoothDamp
    private Quaternion _rollRotation = Quaternion.identity;  // accumulated roll
    private float      _wobble;                              // decays each frame

    // ─────────────────────────────────────────────────────────────────────────

    void Awake()
    {
        _visualProgress = 0f;
        SnapVisualToLogical();
    }

    // ─────────────────────────────────────────────────────────────────────────
    //  Public API
    // ─────────────────────────────────────────────────────────────────────────

    /// <summary>
    /// Called by BoulderGameManager for every valid rep.
    /// repPower = pullDistance / repTimeSec — rewards explosive intent.
    /// combinedMultiplier = timingMult * staminaFactor * comboBonus
    /// </summary>
    public void Push(float repPower, float combinedMultiplier)
    {
        float gain = Mathf.Clamp(repPower * powerScale * combinedMultiplier,
                                 0f, maxPushPerRep);
        Progress = Mathf.Clamp01(Progress + gain);
        _wobble  = pushWobbleDegrees;
    }

    /// <summary>
    /// Called every frame during rollback idle period.
    /// </summary>
    public void ApplyRollback(float deltaTime)
    {
        Progress = Mathf.Clamp01(Progress - rollbackSpeed * deltaTime);
    }

    /// <summary>Snaps both logical and visual progress back to 0.</summary>
    public void ResetPosition()
    {
        Progress = 0f;
        _visualProgress = 0f;
        _rollRotation = Quaternion.identity;
        _wobble = 0f;
        SnapVisualToLogical();
    }

    // ─────────────────────────────────────────────────────────────────────────
    //  Update
    // ─────────────────────────────────────────────────────────────────────────

    void Update()
    {
        float prevVisual = _visualProgress;

        // Smooth position — SmoothDamp is frame-rate independent and never overshoots
        _visualProgress = Mathf.SmoothDamp(_visualProgress, Progress,
                                           ref _visualProgressVelocity, 0.25f);

        // World position along path + perpendicular height offset
        if (pathStart != null && pathEnd != null)
            transform.position = PathPosition(_visualProgress);

        // Rolling rotation proportional to visual delta
        float delta = _visualProgress - prevVisual;
        if (pathStart != null && pathEnd != null && Mathf.Abs(delta) > 0.00001f)
        {
            // Axis perpendicular to the hill direction — this is what makes it
            // roll forward/back instead of sideways regardless of hill orientation
            Vector3 hillFwd  = (pathEnd.position - pathStart.position).normalized;
            Vector3 rollAxis = Vector3.Cross(Vector3.up, hillFwd).normalized;
            float   degrees  = delta * rollDegreesPerProgress;
            _rollRotation = Quaternion.AngleAxis(degrees, rollAxis) * _rollRotation;
        }

        // Apply base roll + wobble shake on top
        Quaternion wobbleQ = _wobble > 0.05f
            ? Quaternion.Euler(_wobble * Mathf.Sin(Time.time * 14f), 0f, _wobble * 0.4f)
            : Quaternion.identity;
        transform.rotation = _rollRotation * wobbleQ;

        // Decay wobble
        if (_wobble > 0.05f)
            _wobble = Mathf.Lerp(_wobble, 0f, Time.deltaTime * 8f);
        else
            _wobble = 0f;
    }

    // ─────────────────────────────────────────────────────────────────────────

    private void SnapVisualToLogical()
    {
        if (pathStart != null && pathEnd != null)
            transform.position = PathPosition(_visualProgress);
    }

    // Returns the world position at progress t, lifted perpendicular to the slope.
    private Vector3 PathPosition(float t)
    {
        Vector3 basePos         = Vector3.Lerp(pathStart.position, pathEnd.position, t);
        float   effectiveOffset = rampOffsetAlongPath ? heightOffset * t : heightOffset;
        if (Mathf.Approximately(effectiveOffset, 0f)) return basePos;

        Vector3 hillFwd   = (pathEnd.position - pathStart.position).normalized;
        Vector3 hillRight = Vector3.Cross(hillFwd, Vector3.up).normalized;
        Vector3 slopeUp   = Vector3.Cross(hillRight, hillFwd).normalized;
        return basePos + slopeUp * effectiveOffset;
    }

#if UNITY_EDITOR
    void OnDrawGizmosSelected()
    {
        if (pathStart == null || pathEnd == null) return;
        Gizmos.color = Color.yellow;
        Gizmos.DrawLine(pathStart.position, pathEnd.position);
        Gizmos.DrawSphere(pathStart.position, 0.25f);
        Gizmos.DrawSphere(pathEnd.position,   0.25f);
    }
#endif
}
