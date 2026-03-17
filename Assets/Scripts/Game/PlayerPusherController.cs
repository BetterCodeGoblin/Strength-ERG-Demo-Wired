using System;
using System.Collections;
using UnityEngine;

/// <summary>
/// Positions the player character just behind the boulder and drives
/// smooth push animations. Uses a Bool animator parameter so the
/// animation plays fully through instead of resetting every rep.
///
/// Also fires OnPushPeak at the animation's peak effort moment, which
/// BoulderGameManager uses to open the QTE bonus window — so pushing
/// in rhythm with the animation naturally earns the bonus.
/// </summary>
public class PlayerPusherController : MonoBehaviour
{
    [Header("References")]
    [Tooltip("The boulder being pushed.")]
    public Transform boulderTransform;

    [Header("Positioning (relative to boulder)")]
    [Tooltip("How far behind the boulder centre the character stands.")]
    public float standOffsetBehind = 1.4f;

    [Tooltip("Downward offset so feet touch the ground plane.")]
    public float standOffsetDown = 0f;

    [Tooltip("Lifts the player perpendicular to the slope, same as Boulder's Height Offset. " +
             "Set this to the same value or slightly less so feet sit on the surface.")]
    public float slopeHeightOffset = 0f;

    [Tooltip("When enabled, the height offset scales from 0 at the bottom to full at the top, " +
             "matching terrain that is flatter at the base and steeper higher up.")]
    public bool rampOffsetAlongPath = true;

    [Tooltip("Follow-smoothing speed.")]
    public float followSmoothSpeed = 8f;

    [Header("Push Lunge (script-driven body lean)")]
    [Tooltip("How far forward the character root lunges on each push.")]
    public float lungeDist = 0.5f;

    [Tooltip("Lunge speed (higher = snappier).")]
    public float lungeSpeed = 18f;

    [Tooltip("Lean-forward angle at peak lunge (degrees).")]
    public float leanAngle = 22f;

    [Header("Animator")]
    [Tooltip("Animator on the character model. Leave blank for capsule placeholder.")]
    public Animator animator;

    [Tooltip("Exact name of the Push state in your Animator Controller.")]
    public string pushBoolName = "Push";

    [Tooltip("Approximate length of your push animation clip in seconds. " +
             "Tune this to match the actual clip so the Bool resets at the right time.")]
    public float animationDuration = 0.8f;

    [Tooltip("Fraction through the animation when peak effort occurs (0=start, 1=end). " +
             "OnPushPeak fires here, opening the QTE bonus window.")]
    [Range(0f, 1f)]
    public float peakFraction = 0.4f;

    // ── Event ─────────────────────────────────────────────────────────────────
    /// <summary>
    /// Fires at the peak moment of the push animation.
    /// BoulderGameManager listens to this and opens the QTE bonus window,
    /// so pushing in rhythm with the animation earns a multiplier bonus.
    /// </summary>
    public event Action OnPushPeak;

    // ── Private ───────────────────────────────────────────────────────────────
    private float            _lungeT;
    private bool             _lunging;
    private bool             _isAnimating;
    private BoulderController _boulderController;

    // ─────────────────────────────────────────────────────────────────────────

    void Start()
    {
        if (boulderTransform != null)
            _boulderController = boulderTransform.GetComponent<BoulderController>();

        // Auto-find Animator if not assigned in Inspector
        if (animator == null)
            animator = GetComponentInChildren<Animator>();

        if (animator == null)
            Debug.LogWarning("[Pusher] No Animator found — assign it in the Inspector.");
    }

    void Update()
    {
        if (boulderTransform == null) return;

        Vector3 hillFwd  = GetHillForward();
        Vector3 hillRight = Vector3.Cross(hillFwd, Vector3.up).normalized;
        Vector3 slopeUp   = Vector3.Cross(hillRight, hillFwd).normalized;

        float progress        = _boulderController != null ? _boulderController.Progress : 1f;
        float effectiveOffset = rampOffsetAlongPath ? slopeHeightOffset * progress : slopeHeightOffset;

        Vector3 restPos = boulderTransform.position
                        - hillFwd   * standOffsetBehind
                        + Vector3.down * standOffsetDown
                        + slopeUp   * effectiveOffset;

        // Script-driven lunge (runs independent of Animator)
        if (_lunging)
        {
            _lungeT += Time.deltaTime * lungeSpeed;
            if (_lungeT >= 1f)
            {
                _lungeT  = 0f;
                _lunging = false;
            }
        }

        float   t      = _lunging ? Mathf.Sin(_lungeT * Mathf.PI) : 0f;
        Vector3 target = restPos + hillFwd * (t * lungeDist);

        transform.position = Vector3.Lerp(transform.position, target,
                                          Time.deltaTime * followSmoothSpeed);

        if (hillFwd.sqrMagnitude > 0.001f)
        {
            Quaternion baseRot = Quaternion.LookRotation(hillFwd);
            Quaternion lean    = Quaternion.Euler(-leanAngle * t, 0f, 0f);
            transform.rotation = Quaternion.Slerp(transform.rotation,
                                                  baseRot * lean,
                                                  Time.deltaTime * 10f);
        }
    }

    // ─────────────────────────────────────────────────────────────────────────

    /// <summary>Called by BoulderGameManager on every valid rep.</summary>
    /// <param name="repTimeSec">Actual drive time from the ERG — scales animation speed to match.</param>
    public void TriggerPushAnim(float repTimeSec = 0f)
    {
        // Only restart the lunge from zero if one isn't already running.
        // Resetting mid-lunge causes a visible snap/jitter.
        if (!_lunging)
        {
            _lungeT  = 0f;
            _lunging = true;
        }

        if (animator != null)
        {
            if (!_isAnimating)
                StartCoroutine(AnimationSequence(repTimeSec));
            else
                Debug.Log("[Pusher] TriggerPushAnim skipped — already animating");
        }
        else
        {
            Debug.LogWarning("[Pusher] TriggerPushAnim — animator is NULL, check Inspector reference");
            StartCoroutine(FirePeakOnly(repTimeSec));
        }
    }

    // ─────────────────────────────────────────────────────────────────────────
    //  Coroutines
    // ─────────────────────────────────────────────────────────────────────────

    private IEnumerator AnimationSequence(float repTimeSec)
    {
        _isAnimating = true;

        if (animator.isActiveAndEnabled)
        {
            // Scale animation speed so it finishes in repTimeSec.
            // If repTimeSec is 0 (keyboard sim), play at natural speed.
            float targetDuration = repTimeSec > 0.05f ? repTimeSec : animationDuration;
            float speed = Mathf.Clamp(animationDuration / targetDuration, 0.4f, 4f);

            Debug.Log($"[Pusher] Playing '{pushBoolName}' at speed={speed:F2} (rep={repTimeSec:F2}s)");
            animator.Play(pushBoolName, 0, 0f);
            animator.speed = speed;

            // Wait times use the target duration so they match the scaled playback
            yield return new WaitForSeconds(targetDuration * peakFraction);
            OnPushPeak?.Invoke();
            yield return new WaitForSeconds(targetDuration * (1f - peakFraction));
        }
        else
        {
            Debug.LogWarning("[Pusher] Animator is not active/enabled — skipping Play");
            yield return new WaitForSeconds(animationDuration * peakFraction);
            OnPushPeak?.Invoke();
            yield return new WaitForSeconds(animationDuration * (1f - peakFraction));
        }

        if (animator.isActiveAndEnabled)
            animator.speed = 0f;

        _isAnimating = false;
    }

    private IEnumerator FirePeakOnly(float repTimeSec)
    {
        float duration = repTimeSec > 0.05f ? repTimeSec : animationDuration;
        yield return new WaitForSeconds(duration * peakFraction);
        OnPushPeak?.Invoke();
    }

    // ─────────────────────────────────────────────────────────────────────────

    private Vector3 GetHillForward()
    {
        BoulderController bc = FindObjectOfType<BoulderController>();
        if (bc != null && bc.pathStart != null && bc.pathEnd != null)
        {
            Vector3 dir = (bc.pathEnd.position - bc.pathStart.position).normalized;
            if (dir.sqrMagnitude > 0.001f)
                return dir;
        }
        return Vector3.forward;
    }
}
