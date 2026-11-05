using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.Json;
using System.Threading.Tasks;
using System.Windows;

namespace QuickPalette;

/// <summary>Everything searchable: apps, settings pages, built-in and custom commands.</summary>
public static class Catalog
{
    public static volatile List<ResultItem> All = new();

    class AppInfo { public string Name { get; set; } = ""; public string Id { get; set; } = ""; }
    static List<AppInfo>? _apps;
    static string CachePath => Path.Combine(Config.Dir, "apps.json");

    public static void Rebuild()
    {
        _apps ??= LoadAppCache();
        var l = new List<ResultItem>();
        l.AddRange(BuiltIns());
        l.AddRange(SettingItems());
        l.AddRange(CustomItems());
        foreach (var a in _apps.GroupBy(a => a.Id).Select(g => g.First()))
        {
            var id = a.Id;
            l.Add(new ResultItem
            {
                Id = "app:" + id, Title = a.Name, Subtitle = "Application", Category = "Apps", Track = true,
                Run = () => Proc.Open("explorer.exe", "shell:AppsFolder\\" + id)
            });
        }
        All = l;
    }

    // ---------- apps (desktop + Store apps) via Get-StartApps, cached for 24h ----------
    static List<AppInfo> LoadAppCache()
    {
        try
        {
            if (File.Exists(CachePath))
                return JsonSerializer.Deserialize<List<AppInfo>>(File.ReadAllText(CachePath)) ?? new();
        }
        catch { }
        return new();
    }

    public static void RefreshApps(bool force)
    {
        Task.Run(() =>
        {
            try
            {
                if (!force && File.Exists(CachePath) &&
                    DateTime.UtcNow - File.GetLastWriteTimeUtc(CachePath) < TimeSpan.FromHours(24)) return;

                var psi = new ProcessStartInfo("powershell.exe",
                    "-NoProfile -NonInteractive -ExecutionPolicy Bypass -Command \"[Console]::OutputEncoding=[Text.Encoding]::UTF8; Get-StartApps | Select-Object Name,AppID | ConvertTo-Json -Compress\"")
                {
                    RedirectStandardOutput = true, UseShellExecute = false, CreateNoWindow = true,
                    StandardOutputEncoding = Encoding.UTF8
                };
                using var p = Process.Start(psi)!;
                var json = p.StandardOutput.ReadToEnd().TrimStart('\uFEFF');
                p.WaitForExit(30000);

                var apps = new List<AppInfo>();
                using (var doc = JsonDocument.Parse(json))
                {
                    var root = doc.RootElement;
                    var els = root.ValueKind == JsonValueKind.Array
                        ? root.EnumerateArray().ToList()
                        : new List<JsonElement> { root };
                    foreach (var el in els)
                    {
                        var n = el.GetProperty("Name").GetString();
                        var id = el.GetProperty("AppID").GetString();
                        if (!string.IsNullOrWhiteSpace(n) && !string.IsNullOrWhiteSpace(id))
                            apps.Add(new AppInfo { Name = n!, Id = id! });
                    }
                }
                if (apps.Count == 0) return;

                Directory.CreateDirectory(Config.Dir);
                File.WriteAllText(CachePath, JsonSerializer.Serialize(apps));
                _apps = apps;
                Application.Current.Dispatcher.Invoke(Rebuild);
            }
            catch { }
        });
    }

    // ---------- custom commands from config ----------
    static IEnumerable<ResultItem> CustomItems()
    {
        foreach (var c in App.Cfg.Commands)
        {
            if (string.IsNullOrWhiteSpace(c.Name) || string.IsNullOrWhiteSpace(c.Target)) continue;
            var cc = c;
            yield return new ResultItem
            {
                Id = "cmd:custom:" + c.Name, Title = c.Name, Subtitle = "Custom command · " + c.Target,
                Category = "Commands", Keywords = c.Keywords, Track = true, Glyph = "\uE756",
                Run = () => Proc.Open(cc.Target, cc.Arguments)
            };
        }
    }

    // ---------- built-in commands ----------
    static ResultItem Cmd(string id, string title, string sub, string kw, string glyph, Action run, bool confirm = false)
    {
        Action act = !confirm ? run : () =>
        {
            if (MessageBox.Show($"{title}?", "Quick Palette", MessageBoxButton.YesNo, MessageBoxImage.Question) == MessageBoxResult.Yes)
                run();
        };
        return new ResultItem { Id = "cmd:" + id, Title = title, Subtitle = sub, Category = "Commands", Keywords = kw, Track = true, Glyph = glyph, Run = act };
    }

    static IEnumerable<ResultItem> BuiltIns()
    {
        yield return Cmd("lock", "Lock", "Lock this PC", "lock screen win+l", "\uE72E", () => Proc.Silent("rundll32.exe", "user32.dll,LockWorkStation"));
        yield return Cmd("sleep", "Sleep", "Put this PC to sleep", "suspend", "\uE708", () => Proc.Silent("rundll32.exe", "powrprof.dll,SetSuspendState 0,1,0"));
        yield return Cmd("signout", "Sign out", "Sign out of Windows", "log off logout", "\uE7E8", () => Proc.Silent("shutdown.exe", "/l"), true);
        yield return Cmd("restart", "Restart", "Restart this PC", "reboot", "\uE777", () => Proc.Silent("shutdown.exe", "/r /t 0"), true);
        yield return Cmd("shutdown", "Shut down", "Turn off this PC", "power off turn off", "\uE7E8", () => Proc.Silent("shutdown.exe", "/s /t 0"), true);
        yield return Cmd("recycle", "Empty Recycle Bin", "Permanently delete everything in it", "trash garbage clean", "\uE74D",
            () => Proc.Silent("powershell.exe", "-NoProfile -WindowStyle Hidden -Command \"Clear-RecycleBin -Force -ErrorAction SilentlyContinue\""), true);
        yield return Cmd("taskmgr", "Task Manager", "See running processes", "processes kill end task performance", "\uE9D9", () => Proc.Open("taskmgr.exe"));
        yield return Cmd("explorer", "File Explorer", "Open File Explorer", "files folders", "\uE8B7", () => Proc.Open("explorer.exe"));
        yield return Cmd("cmd", "Command Prompt", "Open cmd", "terminal console shell", "\uE756", () => Proc.Open("cmd.exe"));
        yield return Cmd("powershell", "PowerShell", "Open PowerShell", "terminal console shell", "\uE756", () => Proc.Open("powershell.exe"));
        yield return Cmd("regedit", "Registry Editor", "Open regedit", "registry", "\uE713", () => Proc.Open("regedit.exe"));
        yield return Cmd("settings", "Quick Palette Settings", "Shortcut, bangs, commands, hidden items", "preferences options hotkey", "\uE713", SettingsWindow.ShowSingle);
        yield return Cmd("reload", "Reload app list", "Re-scan installed apps", "refresh index update", "\uE72C", () => RefreshApps(true));
        yield return Cmd("config", "Open config folder", "config.json lives here", "settings json", "\uE838", () => Proc.Open("explorer.exe", Config.Dir));
        yield return Cmd("quit", "Quit Quick Palette", "Exit the app", "close exit", "\uE711", () => Application.Current.Shutdown());
    }

    // ---------- Windows settings ----------
    // Title | target | keywords      (target may contain arguments after a space)
    const string SettingsData = @"
Display|ms-settings:display|monitor screen resolution brightness scale orientation
Night light|ms-settings:nightlight|blue light warm eye
Sound|ms-settings:sound|volume audio speakers microphone output input
Notifications|ms-settings:notifications|alerts banners
Focus|ms-settings:quietmomentshome|do not disturb focus assist
Power & battery|ms-settings:powersleep|sleep screen timeout energy
Battery saver|ms-settings:batterysaver|power saving
Storage|ms-settings:storagesense|disk space cleanup free up
Multitasking|ms-settings:multitasking|snap windows virtual desktops alt tab
Clipboard|ms-settings:clipboard|history paste
Bluetooth & devices|ms-settings:bluetooth|pair add device
Printers & scanners|ms-settings:printers|print
Mouse|ms-settings:mouse|pointer cursor scroll buttons
Touchpad|ms-settings:devices-touchpad|gestures tap
Typing|ms-settings:typing|autocorrect spelling text suggestions
USB|ms-settings:usb|ports
AutoPlay|ms-settings:autoplay|removable drives media
Network & internet|ms-settings:network|connection status
Wi-Fi|ms-settings:network-wifi|wireless networks
Ethernet|ms-settings:network-ethernet|cable lan
VPN|ms-settings:network-vpn|virtual private network
Airplane mode|ms-settings:network-airplanemode|flight wireless radio
Mobile hotspot|ms-settings:network-mobilehotspot|share internet tethering
Proxy|ms-settings:network-proxy|proxy server
Data usage|ms-settings:datausage|metered bandwidth
Personalization|ms-settings:personalization|customize appearance
Background|ms-settings:personalization-background|wallpaper desktop picture
Colors|ms-settings:personalization-colors|dark mode light mode accent theme transparency
Themes|ms-settings:themes|desktop theme
Lock screen|ms-settings:lockscreen|wallpaper
Taskbar|ms-settings:taskbar|icons alignment widgets search
Start|ms-settings:personalization-start|menu recommended folders
Fonts|ms-settings:fonts|typefaces install
Installed apps|ms-settings:appsfeatures|uninstall programs remove apps features
Default apps|ms-settings:defaultapps|default browser file types open with
Startup apps|ms-settings:startupapps|autostart login boot
Optional features|ms-settings:optionalfeatures|windows features add
Your info|ms-settings:yourinfo|account profile picture
Sign-in options|ms-settings:signinoptions|password pin windows hello fingerprint face
Email & accounts|ms-settings:emailandaccounts|add account
Windows Backup|ms-settings:backup|onedrive restore
Access work or school|ms-settings:workplace|organization
Date & time|ms-settings:dateandtime|clock timezone time zone
Language & region|ms-settings:regionlanguage|keyboard layout input language
Speech|ms-settings:speech|voice recognition
Game Bar|ms-settings:gaming-gamebar|xbox capture record
Game Mode|ms-settings:gaming-gamemode|performance
Accessibility|ms-settings:easeofaccess|ease of access
Text size|ms-settings:easeofaccess-display|bigger text zoom
Narrator|ms-settings:easeofaccess-narrator|screen reader
Magnifier|ms-settings:easeofaccess-magnifier|zoom
Closed captions|ms-settings:easeofaccess-closedcaptioning|subtitles
Keyboard accessibility|ms-settings:easeofaccess-keyboard|sticky keys on-screen keyboard
Privacy & security|ms-settings:privacy|permissions
Location privacy|ms-settings:privacy-location|gps
Camera privacy|ms-settings:privacy-webcam|webcam permission
Microphone privacy|ms-settings:privacy-microphone|mic permission
Windows Security|windowsdefender:|antivirus firewall virus defender protection
For developers|ms-settings:developers|developer mode
Windows Update|ms-settings:windowsupdate|updates upgrade patches
Recovery|ms-settings:recovery|reset this pc advanced startup
Activation|ms-settings:activation|license product key
Troubleshoot|ms-settings:troubleshoot|fix problems
Find my device|ms-settings:findmydevice|locate
About|ms-settings:about|pc name specs version build device information
Programs and Features|appwiz.cpl|uninstall control panel
Network Connections|ncpa.cpl|adapters control panel
System Properties|sysdm.cpl|advanced performance control panel
Environment Variables|rundll32.exe sysdm.cpl,EditEnvironmentVariables|path env
Power Options|powercfg.cpl|power plan control panel
Classic Sound settings|mmsys.cpl|recording playback devices
Device Manager|devmgmt.msc|drivers hardware
Disk Management|diskmgmt.msc|partitions drives format
Services|services.msc|background services
Event Viewer|eventvwr.msc|logs errors
Control Panel|control.exe|classic settings
";

    static IEnumerable<ResultItem> SettingItems()
    {
        foreach (var line in SettingsData.Split('\n', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries))
        {
            var p = line.Split('|');
            if (p.Length < 3) continue;
            var target = p[1];
            string file = target, args = "";
            int sp = target.IndexOf(' ');
            if (sp > 0) { file = target[..sp]; args = target[(sp + 1)..]; }
            yield return new ResultItem
            {
                Id = "set:" + target, Title = p[0], Subtitle = "Settings", Category = "Settings",
                Keywords = p[2], Track = true, Run = () => Proc.Open(file, args)
            };
        }
    }
}
