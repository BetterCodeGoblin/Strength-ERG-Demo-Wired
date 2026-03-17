using UnityEngine;

/// <summary>
/// Third-person camera that follows a target (the boulder or the player)
/// from behind and slightly above, smoothly tracking along the hill.
///
/// Setup:
///   - Attach to Main Camera
///   - Set "target" to the Boulder (or a player character pivot)
///   - "lookAtTarget" can be the same or a point slightly ahead of the boulder
/// </summary>
public class CameraController : MonoBehaviour
{
    [Header("Follow Target")]
    [Tooltip("The transform the camera follows (assign Boulder or Player pivot).")]
    public Transform target;

    [Tooltip("Optional separate look-at point; if null, uses target position.")]
    public Transform lookAtTarget;

    [Header("Offset (local to target's facing direction)")]
    [Tooltip("Distance behind the target.")]
    public float distanceBehind = 7f;

    [Tooltip("Height above the target.")]
    public float heightAbove = 3.5f;

    [Tooltip("Lateral offset (positive = right).")]
    public float lateralOffset = 0.8f;

    [Header("Smoothing")]
    [Tooltip("Position follow speed (higher = snappier).")]
    public float positionSmoothSpeed = 5f;

    [Tooltip("Rotation follow speed.")]
    public float rotationSmoothSpeed = 6f;

    [Header("Look Angle")]
    [Tooltip("Extra downward tilt in degrees (look slightly down at the scene).")]
    [Range(-30f, 30f)]
    public float pitchOffset = 5f;

    // ── Private ───────────────────────────────────────────────────────────────
    private Vector3    _desiredPos;
    private Quaternion _desiredRot;

    // ─────────────────────────────────────────────────────────────────────────

    void LateUpdate()
    {
        if (target == null) return;

        // Build an offset in the direction the hill runs.
        // The hill path is approximated by the forward direction from pathStart→pathEnd.
        // If we can't infer that, fall back to world -Z (default Unity "forward into scene").
        Vector3 forward = GetHillForward();
        Vector3 right   = Vector3.Cross(Vector3.up, forward).normalized;

        // Desired camera position: behind, above, and slightly to the side
        _desiredPos = target.position
                    - forward * distanceBehind
                    + Vector3.up * heightAbove
                    + right * lateralOffset;

        // Smooth position
        transform.position = Vector3.Lerp(transform.position, _desiredPos,
                                          Time.deltaTime * positionSmoothSpeed);

        // Look at the target (or explicit look-at point)
        Vector3 lookAt = lookAtTarget != null ? lookAtTarget.position : target.position;
        _desiredRot = Quaternion.LookRotation(lookAt - transform.position);

        // Apply pitch offset
        _desiredRot = _desiredRot * Quaternion.Euler(pitchOffset, 0f, 0f);

        transform.rotation = Quaternion.Slerp(transform.rotation, _desiredRot,
                                              Time.deltaTime * rotationSmoothSpeed);
    }

    // ─────────────────────────────────────────────────────────────────────────

    private Vector3 GetHillForward()
    {
        // Try to read the path from a BoulderController in the scene
        BoulderController bc = FindObjectOfType<BoulderController>();
        if (bc != null && bc.pathStart != null && bc.pathEnd != null)
        {
            Vector3 dir = (bc.pathEnd.position - bc.pathStart.position).normalized;
            if (dir.sqrMagnitude > 0.001f)
                return dir;
        }
        // Fallback: forward along world Z
        return Vector3.forward;
    }
}
