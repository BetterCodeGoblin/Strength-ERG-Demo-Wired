// CsafeHelper.cs
// CSAFE frame construction and response parsing for PM5 BLE transport.
//
// BLE transport differences vs USB HID:
//   HID:  [REPORT_ID(0x01), 0xF1, ...cmds, checksum, 0xF2] padded to maxOutputReportLength
//   BLE:  [0xF1, ...cmds, checksum, 0xF2]  — no report ID, no padding
//
// The PM5 BLE GATT service uses:
//   Write characteristic CE060001: send CSAFE frames here
//   Notify characteristic CE060002: responses arrive here as notifications
//
// Confirmed byte offsets from PM5HidDiag production code (firmware v55.036):
//   STROKESTATS (0x6E) data payload:
//     [0],[1] = unknown/status
//     [2]     = drive time × 0.01 s
//     [3],[4] = unknown
//     [5]     = pull distance
//     [6]     = rep count
//
// CSAFE short command IDs used here:
//   0x94 = GETCADENCE  ? stroke rate (SPM for rowing, RPM for cycling)
//   0xB4 = GETPOWER    ? current power in watts (2-byte little-endian + units)
//   0xB0 = GETHRCUR    ? current heart rate BPM
//   0xA0 = GETTWORK    ? elapsed work time (h, m, s)
//   0x1A = WRAPPER     ? proprietary PM5 long command wrapper
//   0x6E = STROKESTATS ? sub-command inside wrapper (strength rep data)

namespace PM5BleBridge;

internal static class CsafeHelper
{
    public const byte START        = 0xF1;
    public const byte STOP         = 0xF2;
    public const byte CMD_CADENCE  = 0x94;
    public const byte CMD_POWER    = 0xB4;
    public const byte CMD_HR       = 0xB0;
    public const byte CMD_WORK     = 0xA0;
    public const byte CMD_WRAPPER  = 0x1A;
    public const byte SUB_STROKE   = 0x6E;

    // ?? Frame builders ??????????????????????????????????????????????????????

    /// <summary>
    /// Builds a BLE CSAFE frame (no HID report-ID prefix, no padding).
    /// Frame layout: [START, cmd1, cmd2, ..., XOR-checksum, STOP]
    /// </summary>
    public static byte[] BuildFrame(params byte[] commands)
    {
        byte cs = 0;
        foreach (byte b in commands) cs ^= b;

        var frame = new byte[2 + commands.Length + 2];
        frame[0] = START;
        Array.Copy(commands, 0, frame, 1, commands.Length);
        frame[1 + commands.Length] = cs;
        frame[2 + commands.Length] = STOP;
        return frame;
    }

    /// <summary>
    /// Combined strength poll frame: STROKESTATS (wrapper) + HR + ELAPSED.
    /// STROKESTATS is a proprietary long command inside the wrapper (0x1A).
    /// Wrapper body: [SUB_CMD, DATA_LEN=0] means "send me this sub-command's data".
    /// Used by the StrengthErg CE060020 CSAFE path.
    /// Note: RowErg/BikeErg use CE060030 passive notifications, not CSAFE polling.
    /// </summary>
    public static readonly byte[] StrengthFrame =
        BuildFrame(CMD_WRAPPER, 0x02, SUB_STROKE, 0x00, CMD_HR, CMD_WORK);

    // ?? Response parsers ????????????????????????????????????????????????????

    /// <summary>
    /// Locates a public short command's data payload inside a CSAFE response.
    /// Response layout between START and STOP: [cmdId, dataLen, data...]...
    /// </summary>
    public static byte[]? ExtractPublicCmd(byte[] buf, byte cmd)
    {
        int start = Array.IndexOf(buf, START);
        if (start < 0) return null;

        int stop = -1;
        for (int i = start + 1; i < buf.Length; i++)
            if (buf[i] == STOP) { stop = i; break; }
        if (stop < 0) return null;

        int pos = start + 1;
        // BLE CSAFE responses include a status byte after START (high bit set, e.g. 0x80/0x81).
        // Skip it so we land on the first real command ID.
        if (pos < stop && (buf[pos] & 0x80) != 0) pos++;
        while (pos < stop - 1)
        {
            if (pos >= buf.Length) break;
            byte id  = buf[pos++];
            if (pos >= stop) break;
            byte len = buf[pos++];
            if (id == cmd && len > 0 && pos + len <= stop)
            {
                var data = new byte[len];
                Array.Copy(buf, pos, data, 0, len);
                return data;
            }
            pos += len;
        }
        return null;
    }

    /// <summary>
    /// Locates a proprietary sub-command's data payload inside a CSAFE response.
    /// Wrapper layout: [WRAPPER_CMD(0x1A), totalLen, subCmd, dataLen, data...]
    /// </summary>
    public static byte[]? ExtractProprietarySubCmd(byte[] buf, byte subCmd)
    {
        int start = Array.IndexOf(buf, START);
        if (start < 0) return null;

        int pos   = start + 1;
        int limit = Math.Min(buf.Length, start + 128);
        // Skip CSAFE response status byte if present (high bit set, e.g. 0x01/0x81).
        if (pos < limit && (buf[pos] & 0x80) == 0 && buf[pos] != CMD_WRAPPER) pos++;

        while (pos < limit)
        {
            if (buf[pos] == CMD_WRAPPER)
            {
                pos++;
                if (pos >= limit) break;
                int wrapLen = buf[pos++];
                int wrapEnd = Math.Min(pos + wrapLen, limit);
                while (pos < wrapEnd)
                {
                    byte sc = buf[pos++];
                    if (pos >= wrapEnd) break;
                    byte dl = buf[pos++];
                    if (sc == subCmd && dl > 0 && pos + dl <= buf.Length)
                    {
                        var data = new byte[dl];
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

    // ?? Field decoders ??????????????????????????????????????????????????????

    /// <summary>GETCADENCE response: [value_byte, units_byte] ? strokes/min or RPM.</summary>
    public static float ParseCadence(byte[] d) => d.Length >= 1 ? d[0] : 0f;

    /// <summary>GETPOWER response: [lo, hi, units] ? watts (2-byte LE).</summary>
    public static float ParsePower(byte[] d) =>
        d.Length >= 2 ? (float)(d[0] | (d[1] << 8)) : 0f;

    /// <summary>GETHRCUR response: [bpm, zone] ? BPM.</summary>
    public static int ParseHR(byte[] d) => d.Length >= 1 ? d[0] : 0;

    /// <summary>GETTWORK response: [hours, minutes, seconds] ? total seconds.</summary>
    public static float ParseElapsed(byte[] d) =>
        d.Length >= 3 ? d[0] * 3600f + d[1] * 60f + d[2] : 0f;

    /// <summary>
    /// Returns a human-readable label for a CE060080 multiplexed selector byte.
    /// The selector byte equals the lower byte of the CE06xx characteristic UUID.
    /// </summary>
    public static string MuxSelectorLabel(byte sel) => sel switch
    {
        0x31 => "GeneralStatus",
        0x32 => "AdditionalStatus1",
        0x33 => "AdditionalStatus2",
        0x34 => "StrokeData1",
        0x35 => "StrokeData",
        0x36 => "ForcePlot",
        0x37 => "HeartRateBelt",
        0x3D => "WorkoutSummary",
        0x3E => "WorkoutSummaryData",
        _    => $"Unknown-0x{sel:X2}"
    };

    /// <summary>
    /// Standard C2 rowing pace formula.
    /// pace_sec_per_500m = 500 / speed_m_s,  speed_m_s = (P / 2.8)^(1/3)
    /// Returns 0 for zero power.
    /// </summary>
    public static float ComputeRowingPace(float powerWatts) =>
        powerWatts > 1f ? 500f / (float)Math.Pow(powerWatts / 2.8, 1.0 / 3.0) : 0f;
}
