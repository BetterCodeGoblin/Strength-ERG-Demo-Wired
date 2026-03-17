// Concept2DebugHUD.cs — StrengthErg Production HUD
// Shows rep count, rep time, pull distance, elapsed time, heart rate,
// and a visual bar chart of recent rep times.

using UnityEngine;

public class Concept2DebugHUD : MonoBehaviour
{
    [Header("Display")]
    public bool showHUD = true;
    public int fontSize = 20;

    private GUIStyle _label;
    private GUIStyle _header;
    private GUIStyle _big;
    private GUIStyle _small;

    void OnGUI()
    {
        if (!showHUD) return;
        var r = Concept2UsbReader.Instance;
        if (r == null) return;

        if (_label == null)
        {
            _label = new GUIStyle(GUI.skin.label)
            {
                fontSize = fontSize,
                normal = { textColor = Color.white }
            };
            _header = new GUIStyle(_label) { fontStyle = FontStyle.Bold };
            _big = new GUIStyle(_label)
            {
                fontSize = fontSize + 14,
                fontStyle = FontStyle.Bold,
                normal = { textColor = Color.cyan }
            };
            _small = new GUIStyle(_label) { fontSize = fontSize - 4 };
        }

        _header.normal.textColor = r.IsConnected
            ? (r.IsActive ? Color.green : Color.yellow)
            : Color.red;

        float x = 10, y = 10, h = fontSize + 6;
        float boxH = h * 7 + 36;

        // Add space for bar chart
        int barCount = Mathf.Min(r.RepTimeHistory.Count, 10);
        if (barCount > 0) boxH += barCount * 18 + 24;

        GUI.Box(new Rect(x - 5, y - 5, 350, boxH), "");

        // Header
        GUI.Label(new Rect(x, y, 340, h), $"StrengthErg: {r.StatusText}", _header);
        y += h + 4;

        // Rep count
        GUI.Label(new Rect(x, y, 340, h + 16), $"Rep #{r.RepCount}", _big);
        y += h + 20;

        // Rep time
        GUI.Label(new Rect(x, y, 340, h), $"Rep Time:       {r.RepTimeSec:F2} sec", _label);
        y += h;

        // Pull distance
        GUI.Label(new Rect(x, y, 340, h), $"Pull Distance:  {r.PullDistance}", _label);
        y += h;

        // Elapsed time
        int totalSec = Mathf.RoundToInt(r.ElapsedSeconds);
        GUI.Label(new Rect(x, y, 340, h), $"Elapsed:        {totalSec / 60}:{totalSec % 60:D2}", _label);
        y += h;

        // Heart rate
        string hrText = r.HeartRate > 0 ? $"{r.HeartRate} bpm" : "—";
        GUI.Label(new Rect(x, y, 340, h), $"Heart Rate:     {hrText}", _label);
        y += h;

        // Rep time history bars (last 10 reps)
        if (r.RepTimeHistory.Count > 0)
        {
            y += 8;
            GUI.Label(new Rect(x, y, 340, h), "Recent Reps:", _small);
            y += fontSize;

            int startIdx = Mathf.Max(0, r.RepTimeHistory.Count - 10);
            float barMaxW = 260f;
            float barH = 14f;

            // Find max time for scaling
            float maxTime = 0.5f;
            for (int i = startIdx; i < r.RepTimeHistory.Count; i++)
                if (r.RepTimeHistory[i] > maxTime) maxTime = r.RepTimeHistory[i];

            for (int i = startIdx; i < r.RepTimeHistory.Count; i++)
            {
                int repNum = i + 1;
                float time = r.RepTimeHistory[i];
                float pct = time / maxTime;

                // Rep number label
                GUI.Label(new Rect(x, y - 1, 30, barH + 2), $"#{repNum}", _small);

                // Bar
                GUI.color = Color.Lerp(Color.green, Color.yellow, pct);
                GUI.DrawTexture(new Rect(x + 32, y, barMaxW * pct, barH), Texture2D.whiteTexture);
                GUI.color = Color.white;

                // Time label
                int dist = i < r.PullDistanceHistory.Count ? r.PullDistanceHistory[i] : 0;
                GUI.Label(new Rect(x + 36 + barMaxW * pct, y - 2, 100, barH + 4),
                    $"{time:F2}s  d={dist}", _small);

                y += barH + 2;
            }
        }
    }
}