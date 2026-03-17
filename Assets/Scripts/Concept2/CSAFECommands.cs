public static class CSAFE
{
    // Short Commands
    public const uint CMD_GETSTATUS   = 0x80;
    public const uint CMD_RESET       = 0x81;
    public const uint CMD_GOIDLE      = 0x85;
    public const uint CMD_GOREADY     = 0x86;

    // PM-Specific Data Commands
    public const uint CSAFE_PM_GET_WORKTIME     = 0x00;
    public const uint CSAFE_PM_GET_WORKDISTANCE = 0x01;
    public const uint CSAFE_PM_GET_PACE         = 0x06;
    public const uint CSAFE_PM_GET_CADENCE      = 0x07; // Stroke rate (SPM)
    public const uint CSAFE_PM_GET_POWER        = 0x1A;
    public const uint CSAFE_PM_GET_CALHR        = 0x1B;

    // Long Command Prefix (Proprietary PM extension)
    public const uint LONG_CMD_PREFIX = 0x76;
}