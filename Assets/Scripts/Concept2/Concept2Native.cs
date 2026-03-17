using System;
using System.Runtime.InteropServices;

public static class Concept2Native
{
    private const string DLL_NAME = "PM3CsafeCP";

    [DllImport(DLL_NAME)]
    public static extern int tkcmdsetDDI_init();

    [DllImport(DLL_NAME)]
    public static extern int tkcmdsetDDI_discover_pm_units();

    [DllImport(DLL_NAME)]
    public static extern int tkcmdsetDDI_get_pm_count(ref uint count);

    [DllImport(DLL_NAME)]
    public static extern int tkcmdsetCSAFE_init_protocol(ushort timeout);

    [DllImport(DLL_NAME)]
    public static extern int tkcmdsetCSAFE_command(
        uint unitAddress,
        uint inBufferLength,
        uint[] inBuffer,
        ref uint outBufferLength,
        uint[] outBuffer
    );

    [DllImport(DLL_NAME)]
    public static extern int tkcmdsetDDI_close();
}