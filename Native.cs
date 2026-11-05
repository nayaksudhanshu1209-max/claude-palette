using System;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Windows.Input;
using Microsoft.Win32;

namespace QuickPalette;

public static class Native
{
    [StructLayout(LayoutKind.Sequential)]
    public struct MARGINS { public int Left, Right, Top, Bottom; }

    [DllImport("user32.dll")] public static extern bool RegisterHotKey(IntPtr h, int id, uint mod, uint vk);
    [DllImport("user32.dll")] public static extern bool UnregisterHotKey(IntPtr h, int id);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("dwmapi.dll")] public static extern int DwmSetWindowAttribute(IntPtr h, int attr, ref int val, int size);
    [DllImport("dwmapi.dll")] public static extern int DwmExtendFrameIntoClientArea(IntPtr h, ref MARGINS m);
    [DllImport("kernel32.dll")] static extern bool SetProcessWorkingSetSize(IntPtr p, IntPtr min, IntPtr max);

    public static bool IsWin11 => Environment.OSVersion.Version.Build >= 22621;

    /// <summary>Gives memory back to Windows after the palette closes.</summary>
    public static void Trim()
    {
        GC.Collect();
        GC.WaitForPendingFinalizers();
        GC.Collect();
        using var p = Process.GetCurrentProcess();
        SetProcessWorkingSetSize(p.Handle, (IntPtr)(-1), (IntPtr)(-1));
    }
}

public static class Proc
{
    public static void Open(string target, string args = "")
    {
        Process.Start(new ProcessStartInfo(Environment.ExpandEnvironmentVariables(target), args)
        { UseShellExecute = true })?.Dispose();
    }

    public static void Silent(string file, string args)
    {
        Process.Start(new ProcessStartInfo(file, args) { CreateNoWindow = true, UseShellExecute = false })?.Dispose();
    }
}

public static class HotkeyParser
{
    public static bool TryParse(string s, out uint mods, out uint vk)
    {
        mods = 0; vk = 0;
        foreach (var part in s.Split('+', StringSplitOptions.TrimEntries | StringSplitOptions.RemoveEmptyEntries))
        {
            switch (part.ToLowerInvariant())
            {
                case "alt": mods |= 1; break;
                case "ctrl": case "control": mods |= 2; break;
                case "shift": mods |= 4; break;
                case "win": mods |= 8; break;
                default:
                    if (!Enum.TryParse<Key>(part, true, out var k)) return false;
                    vk = (uint)KeyInterop.VirtualKeyFromKey(k);
                    break;
            }
        }
        return vk != 0;
    }
}

public static class Startup
{
    const string RunKey = @"Software\Microsoft\Windows\CurrentVersion\Run";

    public static void Set(bool on)
    {
        try
        {
            using var k = Registry.CurrentUser.OpenSubKey(RunKey, true);
            if (k == null) return;
            if (on) k.SetValue("QuickPalette", "\"" + Environment.ProcessPath + "\"");
            else k.DeleteValue("QuickPalette", false);
        }
        catch { }
    }
}
