// Quick Palette - tiny native Win32 launcher (C++17, no runtime needed).
// Apps + Windows settings + commands + web bangs + calculator, hide-anything, configurable hotkey.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <shlguid.h>
#include <knownfolders.h>
#include <shlwapi.h>
#include <dwmapi.h>
#include <shellscalingapi.h>
#include <cwctype>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "shcore.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "uuid.lib")
#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

using std::wstring;
using std::vector;

// ------------------------------------------------------------------ types
enum Cat { C_APP, C_SET, C_CMD, C_CALC, C_OPEN, C_WEB, C_BANG, C_COUNT };
enum Kind { K_NONE, K_OPEN, K_APP, K_LOCK, K_RECYCLE, K_QUIT, K_EDITCFG, K_RELOAD, K_UNHIDE, K_EDITHIDDEN, K_COPY, K_CFGDIR };

struct Item {
    wstring id, title, sub, tl, kl, group;
    Cat cat = C_CMD;
    wchar_t glyph = 0;
    bool track = false, confirm = false, hide = false;
    int score = 0;
    Kind kind = K_NONE;
    wstring a, b;
};
struct Bang { wstring trig, name, url; };
struct AppEntry { wstring name, id; };
struct CustomCmd { wstring name, kw, target, args; };
struct Row { bool header; int idx; wstring text; };

// ------------------------------------------------------------------ globals
static HINSTANCE g_inst;
static HWND g_hwnd;
static const wchar_t* kClass = L"QuickPaletteWnd";
static const UINT WM_TRAY = WM_APP + 1, WM_APPS_READY = WM_APP + 2, WM_SHOWME = WM_APP + 3;
static UINT g_wmTaskbar;
enum { T_BOOT = 1, T_TRIM = 2 };

static wstring g_dir, g_cfgPath, g_usagePath, g_hiddenPath, g_appsPath;
static wstring g_hotkey = L"Ctrl+Space", g_defaultBang = L"g";
static bool g_startup = true, g_boot = false, g_menuOpen = false;
static ULONGLONG g_cfgStamp = 0;

static vector<Bang> g_bangs;
static vector<CustomCmd> g_cmds;
static vector<AppEntry> g_apps;
static bool g_appsLoaded = false, g_catalogDirty = true, g_stateLoaded = false;
static vector<Item> g_catalog;
static std::unordered_map<wstring, int> g_usage;
static std::unordered_map<wstring, wstring> g_hidden;

static wstring g_query;
static int g_caret = 0;
static vector<Item> g_results;
static vector<Row> g_rows;
static int g_sel = -1, g_hover = -1, g_scroll = 0, g_dpi = 96;
static HFONT g_fSearch, g_fTitle, g_fSub, g_fHdr, g_fIcon;
static NOTIFYICONDATAW g_nid;

static int px(int v) { return MulDiv(v, g_dpi, 96); }

// ------------------------------------------------------------------ string helpers
static wstring lower(wstring s) { for (auto& c : s) c = (wchar_t)towlower(c); return s; }
static wstring Trim(const wstring& s) {
    size_t a = s.find_first_not_of(L" \t\r\n");
    if (a == wstring::npos) return L"";
    size_t b = s.find_last_not_of(L" \t\r\n");
    return s.substr(a, b - a + 1);
}
static vector<wstring> Split(const wstring& s, wchar_t sep) {
    vector<wstring> out; size_t st = 0;
    for (;;) {
        size_t p = s.find(sep, st);
        if (p == wstring::npos) { out.push_back(s.substr(st)); break; }
        out.push_back(s.substr(st, p - st)); st = p + 1;
    }
    return out;
}
static wstring ReplaceAll(wstring s, const wstring& from, const wstring& to) {
    size_t p = 0;
    while ((p = s.find(from, p)) != wstring::npos) { s.replace(p, from.size(), to); p += to.size(); }
    return s;
}
static wstring UrlEncode(const wstring& s) {
    int n = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0, nullptr, nullptr);
    std::string u(n, 0);
    WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.size(), &u[0], n, nullptr, nullptr);
    wstring o;
    for (unsigned char c : u) {
        if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '-' || c == '_' || c == '.' || c == '~')
            o += (wchar_t)c;
        else { wchar_t b[8]; swprintf(b, 8, L"%%%02X", c); o += b; }
    }
    return o;
}
static wstring ExpandEnv(const wstring& s) {
    wchar_t buf[2048];
    DWORD n = ExpandEnvironmentStringsW(s.c_str(), buf, 2048);
    return (n > 0 && n <= 2048) ? wstring(buf) : s;
}

// ------------------------------------------------------------------ file helpers (UTF-8 text)
static bool ReadLines(const wstring& path, vector<wstring>& out) {
    FILE* f = nullptr;
    if (_wfopen_s(&f, path.c_str(), L"r, ccs=UTF-8") != 0 || !f) return false;
    wchar_t buf[4096];
    while (fgetws(buf, 4096, f)) {
        wstring s(buf);
        while (!s.empty() && (s.back() == L'\n' || s.back() == L'\r')) s.pop_back();
        out.push_back(s);
    }
    fclose(f);
    return true;
}
static bool WriteText(const wstring& path, const wstring& text, const wchar_t* mode = L"w, ccs=UTF-8") {
    FILE* f = nullptr;
    if (_wfopen_s(&f, path.c_str(), mode) != 0 || !f) return false;
    fputws(text.c_str(), f);
    fclose(f);
    return true;
}
static ULONGLONG Stamp(const wstring& p) {
    WIN32_FILE_ATTRIBUTE_DATA d;
    if (!GetFileAttributesExW(p.c_str(), GetFileExInfoStandard, &d)) return 0;
    return ((ULONGLONG)d.ftLastWriteTime.dwHighDateTime << 32) | d.ftLastWriteTime.dwLowDateTime;
}

// ------------------------------------------------------------------ config
static void DefaultConfig() {
    WriteText(g_cfgPath,
        L"# Quick Palette settings. Save this file; changes load the next time you open the palette.\n"
        L"\n"
        L"# Shortcut that opens the palette. Examples: Ctrl+Space, Alt+Space, Ctrl+Alt+P, Ctrl+Shift+F1\n"
        L"# (Win+Space is reserved by Windows and cannot be used.)\n"
        L"hotkey=Ctrl+Space\n"
        L"\n"
        L"# 1 = start with Windows (delayed + low priority so it never slows boot), 0 = off\n"
        L"startup=1\n"
        L"\n"
        L"# Trigger of the bang used by the \"Search the web\" row\n"
        L"default_bang=g\n"
        L"\n"
        L"# Bangs: bang=trigger|Name|URL   ({q} is replaced by what you type). Use as: yt lofi beats\n"
        L"bang=g|Google|https://www.google.com/search?q={q}\n"
        L"bang=ddg|DuckDuckGo|https://duckduckgo.com/?q={q}\n"
        L"bang=yt|YouTube|https://www.youtube.com/results?search_query={q}\n"
        L"bang=gh|GitHub|https://github.com/search?q={q}\n"
        L"bang=so|Stack Overflow|https://stackoverflow.com/search?q={q}\n"
        L"bang=w|Wikipedia|https://en.wikipedia.org/w/index.php?search={q}\n"
        L"bang=r|Reddit|https://www.reddit.com/search/?q={q}\n"
        L"bang=maps|Google Maps|https://www.google.com/maps/search/{q}\n"
        L"bang=a|Amazon|https://www.amazon.com/s?k={q}\n"
        L"bang=tr|Google Translate|https://translate.google.com/?sl=auto&tl=en&text={q}\n"
        L"bang=npm|npm|https://www.npmjs.com/search?q={q}\n"
        L"bang=imdb|IMDb|https://www.imdb.com/find/?q={q}\n"
        L"\n"
        L"# Your own commands: cmd=Name|extra search words|program, file, folder or URL|arguments (optional)\n"
        L"# cmd=Notes|text editor write|notepad.exe|\n"
        L"# cmd=Downloads|folder files|%USERPROFILE%\\Downloads|\n"
        L"# cmd=My site|blog website|https://example.com|\n");
}

static void LoadConfig() {
    if (!PathFileExistsW(g_cfgPath.c_str())) DefaultConfig();
    vector<wstring> lines;
    ReadLines(g_cfgPath, lines);
    g_bangs.clear(); g_cmds.clear();
    for (auto& raw : lines) {
        wstring l = Trim(raw);
        if (l.empty() || l[0] == L'#') continue;
        size_t eq = l.find(L'=');
        if (eq == wstring::npos) continue;
        wstring k = lower(Trim(l.substr(0, eq))), v = Trim(l.substr(eq + 1));
        if (k == L"hotkey") g_hotkey = v;
        else if (k == L"startup") g_startup = (v != L"0");
        else if (k == L"default_bang") g_defaultBang = v;
        else if (k == L"bang") {
            auto p = Split(v, L'|');
            if (p.size() >= 3) g_bangs.push_back({ Trim(p[0]), Trim(p[1]), Trim(p[2]) });
        } else if (k == L"cmd") {
            auto p = Split(v, L'|');
            if (p.size() >= 3) g_cmds.push_back({ Trim(p[0]), Trim(p[1]), Trim(p[2]), p.size() > 3 ? Trim(p[3]) : L"" });
        }
    }
    g_cfgStamp = Stamp(g_cfgPath);
    g_catalogDirty = true;
}

static void SyncStartup() {
    HKEY k;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &k) != ERROR_SUCCESS) return;
    if (g_startup) {
        wchar_t exe[MAX_PATH];
        GetModuleFileNameW(nullptr, exe, MAX_PATH);
        wstring v = L"\"" + wstring(exe) + L"\" --boot";
        RegSetValueExW(k, L"QuickPalette", 0, REG_SZ, (const BYTE*)v.c_str(), (DWORD)((v.size() + 1) * sizeof(wchar_t)));
    } else RegDeleteValueW(k, L"QuickPalette");
    RegCloseKey(k);
}

static bool ParseHotkey(const wstring& s, UINT& mods, UINT& vk) {
    mods = 0; vk = 0;
    for (auto& part : Split(s, L'+')) {
        wstring t = lower(Trim(part));
        if (t.empty()) continue;
        if (t == L"ctrl" || t == L"control") mods |= MOD_CONTROL;
        else if (t == L"alt") mods |= MOD_ALT;
        else if (t == L"shift") mods |= MOD_SHIFT;
        else if (t == L"win") mods |= MOD_WIN;
        else if (t.size() == 1 && iswalnum(t[0])) vk = (UINT)towupper(t[0]);
        else if (t == L"space") vk = VK_SPACE;
        else if (t == L"enter") vk = VK_RETURN;
        else if (t == L"tab") vk = VK_TAB;
        else if (t == L"esc" || t == L"escape") vk = VK_ESCAPE;
        else if (t == L"backspace") vk = VK_BACK;
        else if (t[0] == L'f' && t.size() > 1) {
            int n = _wtoi(t.c_str() + 1);
            if (n >= 1 && n <= 24) vk = VK_F1 + n - 1; else return false;
        } else return false;
    }
    return vk != 0;
}
static bool RegisterHotkey() {
    UnregisterHotKey(g_hwnd, 1);
    UINT m, vk;
    if (!ParseHotkey(g_hotkey, m, vk)) return false;
    return RegisterHotKey(g_hwnd, 1, m | MOD_NOREPEAT, vk) != 0;
}

// ------------------------------------------------------------------ usage + hidden
static void SaveUsage() {
    wstring t;
    for (auto& kv : g_usage) t += kv.first + L"\t" + std::to_wstring(kv.second) + L"\n";
    WriteText(g_usagePath, t);
}
static void LoadState() {
    if (g_stateLoaded) return;
    g_stateLoaded = true;
    vector<wstring> ln;
    if (ReadLines(g_usagePath, ln))
        for (auto& l : ln) { auto p = Split(l, L'\t'); if (p.size() == 2) g_usage[p[0]] = _wtoi(p[1].c_str()); }
    ln.clear();
    if (ReadLines(g_hiddenPath, ln))
        for (auto& l : ln) { auto p = Split(l, L'\t'); if (!p[0].empty()) g_hidden[p[0]] = p.size() > 1 ? p[1] : p[0]; }
}

// ------------------------------------------------------------------ catalog
static wchar_t DefaultGlyph(Cat c) {
    switch (c) {
        case C_APP: return 0xE71D; case C_SET: return 0xE713; case C_CMD: return 0xE756;
        case C_CALC: return 0xE8EF; case C_OPEN: return 0xE8A7; default: return 0xE774;
    }
}
static const wchar_t* CatName(Cat c) {
    switch (c) {
        case C_APP: return L"Apps"; case C_SET: return L"Settings"; case C_CMD: return L"Commands";
        case C_CALC: return L"Calculator"; case C_OPEN: return L"Open"; case C_WEB: return L"Web"; default: return L"Web search";
    }
}
static COLORREF Accent(Cat c) {
    switch (c) {
        case C_APP: return RGB(59, 130, 246); case C_SET: return RGB(20, 184, 166); case C_CMD: return RGB(245, 158, 11);
        case C_CALC: return RGB(236, 72, 153); case C_OPEN: return RGB(100, 116, 139); default: return RGB(168, 85, 247);
    }
}

static void Add(vector<Item>& v, const wstring& id, const wstring& title, const wstring& sub, const wstring& kw, Cat cat,
                wchar_t glyph, Kind kind, const wstring& a = L"", const wstring& b = L"", bool confirm = false, bool hide = false) {
    Item it;
    it.id = id; it.title = title; it.sub = sub; it.tl = lower(title); it.kl = lower(kw);
    it.cat = cat; it.glyph = glyph ? glyph : DefaultGlyph(cat); it.group = CatName(cat);
    it.track = true; it.kind = kind; it.a = a; it.b = b; it.confirm = confirm; it.hide = hide;
    v.push_back(std::move(it));
}

static const wchar_t* kSettings[] = {
    L"Display|ms-settings:display|monitor screen resolution brightness scale orientation",
    L"Night light|ms-settings:nightlight|blue light warm eye",
    L"Sound|ms-settings:sound|volume audio speakers microphone output input",
    L"Notifications|ms-settings:notifications|alerts banners",
    L"Focus|ms-settings:quietmomentshome|do not disturb focus assist",
    L"Power & battery|ms-settings:powersleep|sleep screen timeout energy",
    L"Battery saver|ms-settings:batterysaver|power saving",
    L"Storage|ms-settings:storagesense|disk space cleanup free up",
    L"Multitasking|ms-settings:multitasking|snap windows virtual desktops alt tab",
    L"Clipboard|ms-settings:clipboard|history paste",
    L"Bluetooth & devices|ms-settings:bluetooth|pair add device",
    L"Printers & scanners|ms-settings:printers|print",
    L"Mouse|ms-settings:mouse|pointer cursor scroll buttons",
    L"Touchpad|ms-settings:devices-touchpad|gestures tap",
    L"Typing|ms-settings:typing|autocorrect spelling text suggestions",
    L"USB|ms-settings:usb|ports",
    L"AutoPlay|ms-settings:autoplay|removable drives media",
    L"Network & internet|ms-settings:network|connection status",
    L"Wi-Fi|ms-settings:network-wifi|wireless networks",
    L"Ethernet|ms-settings:network-ethernet|cable lan",
    L"VPN|ms-settings:network-vpn|virtual private network",
    L"Airplane mode|ms-settings:network-airplanemode|flight wireless radio",
    L"Mobile hotspot|ms-settings:network-mobilehotspot|share internet tethering",
    L"Proxy|ms-settings:network-proxy|proxy server",
    L"Data usage|ms-settings:datausage|metered bandwidth",
    L"Personalization|ms-settings:personalization|customize appearance",
    L"Background|ms-settings:personalization-background|wallpaper desktop picture",
    L"Colors|ms-settings:personalization-colors|dark mode light mode accent theme transparency",
    L"Themes|ms-settings:themes|desktop theme",
    L"Lock screen|ms-settings:lockscreen|wallpaper",
    L"Taskbar|ms-settings:taskbar|icons alignment widgets search",
    L"Start|ms-settings:personalization-start|menu recommended folders",
    L"Fonts|ms-settings:fonts|typefaces install",
    L"Installed apps|ms-settings:appsfeatures|uninstall programs remove apps features",
    L"Default apps|ms-settings:defaultapps|default browser file types open with",
    L"Startup apps|ms-settings:startupapps|autostart login boot",
    L"Optional features|ms-settings:optionalfeatures|windows features add",
    L"Your info|ms-settings:yourinfo|account profile picture",
    L"Sign-in options|ms-settings:signinoptions|password pin windows hello fingerprint face",
    L"Email & accounts|ms-settings:emailandaccounts|add account",
    L"Windows Backup|ms-settings:backup|onedrive restore",
    L"Access work or school|ms-settings:workplace|organization",
    L"Date & time|ms-settings:dateandtime|clock timezone time zone",
    L"Language & region|ms-settings:regionlanguage|keyboard layout input language",
    L"Speech|ms-settings:speech|voice recognition",
    L"Game Bar|ms-settings:gaming-gamebar|xbox capture record",
    L"Game Mode|ms-settings:gaming-gamemode|performance",
    L"Accessibility|ms-settings:easeofaccess|ease of access",
    L"Text size|ms-settings:easeofaccess-display|bigger text zoom",
    L"Narrator|ms-settings:easeofaccess-narrator|screen reader",
    L"Magnifier|ms-settings:easeofaccess-magnifier|zoom",
    L"Closed captions|ms-settings:easeofaccess-closedcaptioning|subtitles",
    L"Keyboard accessibility|ms-settings:easeofaccess-keyboard|sticky keys on-screen keyboard",
    L"Privacy & security|ms-settings:privacy|permissions",
    L"Location privacy|ms-settings:privacy-location|gps",
    L"Camera privacy|ms-settings:privacy-webcam|webcam permission",
    L"Microphone privacy|ms-settings:privacy-microphone|mic permission",
    L"Windows Security|windowsdefender:|antivirus firewall virus defender protection",
    L"For developers|ms-settings:developers|developer mode",
    L"Windows Update|ms-settings:windowsupdate|updates upgrade patches",
    L"Recovery|ms-settings:recovery|reset this pc advanced startup",
    L"Activation|ms-settings:activation|license product key",
    L"Troubleshoot|ms-settings:troubleshoot|fix problems",
    L"About|ms-settings:about|pc name specs version build device information",
    L"Programs and Features|appwiz.cpl|uninstall control panel",
    L"Network Connections|ncpa.cpl|adapters control panel",
    L"System Properties|sysdm.cpl|advanced performance control panel",
    L"Environment Variables|rundll32.exe sysdm.cpl,EditEnvironmentVariables|path env",
    L"Power Options|powercfg.cpl|power plan control panel",
    L"Classic Sound settings|mmsys.cpl|recording playback devices",
    L"Device Manager|devmgmt.msc|drivers hardware",
    L"Disk Management|diskmgmt.msc|partitions drives format",
    L"Services|services.msc|background services",
    L"Event Viewer|eventvwr.msc|logs errors",
    L"Control Panel|control.exe|classic settings",
};

static void BuildCatalog() {
    vector<Item> v;
    v.reserve(160 + g_apps.size());

    // commands
    Add(v, L"cmd:lock", L"Lock", L"Lock this PC", L"lock screen", C_CMD, 0xE72E, K_LOCK);
    Add(v, L"cmd:sleep", L"Sleep", L"Put this PC to sleep", L"suspend", C_CMD, 0xE708, K_OPEN, L"rundll32.exe", L"powrprof.dll,SetSuspendState 0,1,0", false, true);
    Add(v, L"cmd:signout", L"Sign out", L"Sign out of Windows", L"log off logout", C_CMD, 0xE7E8, K_OPEN, L"shutdown.exe", L"/l", true, true);
    Add(v, L"cmd:restart", L"Restart", L"Restart this PC", L"reboot", C_CMD, 0xE777, K_OPEN, L"shutdown.exe", L"/r /t 0", true, true);
    Add(v, L"cmd:shutdown", L"Shut down", L"Turn off this PC", L"power off turn off", C_CMD, 0xE7E8, K_OPEN, L"shutdown.exe", L"/s /t 0", true, true);
    Add(v, L"cmd:recycle", L"Empty Recycle Bin", L"Permanently delete everything in it", L"trash garbage clean", C_CMD, 0xE74D, K_RECYCLE, L"", L"", true);
    Add(v, L"cmd:taskmgr", L"Task Manager", L"See running processes", L"processes kill end task performance", C_CMD, 0xE9D9, K_OPEN, L"taskmgr.exe");
    Add(v, L"cmd:explorer", L"File Explorer", L"Open File Explorer", L"files folders", C_CMD, 0xE8B7, K_OPEN, L"explorer.exe");
    Add(v, L"cmd:cmd", L"Command Prompt", L"Open cmd", L"terminal console shell", C_CMD, 0xE756, K_OPEN, L"cmd.exe");
    Add(v, L"cmd:powershell", L"PowerShell", L"Open PowerShell", L"terminal console shell", C_CMD, 0xE756, K_OPEN, L"powershell.exe");
    Add(v, L"cmd:regedit", L"Registry Editor", L"Open regedit", L"registry", C_CMD, 0xE713, K_OPEN, L"regedit.exe");
    Add(v, L"cmd:settings", L"Quick Palette Settings", L"Opens config.ini: shortcut, bangs, your commands", L"preferences options hotkey config shortcut startup", C_CMD, 0xE713, K_EDITCFG);
    Add(v, L"cmd:reload", L"Reload app list", L"Re-scan installed apps", L"refresh index update", C_CMD, 0xE72C, K_RELOAD);
    Add(v, L"cmd:unhide", L"Unhide all hidden items", L"Bring back everything you hid", L"show restore reset", C_CMD, 0xE7B3, K_UNHIDE);
    Add(v, L"cmd:edithidden", L"Edit hidden items list", L"hidden.txt - delete a line to unhide one item", L"show restore", C_CMD, 0xE7B3, K_EDITHIDDEN);
    Add(v, L"cmd:cfgdir", L"Open config folder", L"Where Quick Palette keeps its files", L"settings", C_CMD, 0xE838, K_CFGDIR);
    Add(v, L"cmd:quit", L"Quit Quick Palette", L"Exit the app", L"close exit", C_CMD, 0xE711, K_QUIT);

    // settings
    for (const wchar_t* line : kSettings) {
        auto p = Split(line, L'|');
        if (p.size() < 3) continue;
        wstring file = p[1], args;
        size_t sp = file.find(L' ');
        if (sp != wstring::npos) { args = file.substr(sp + 1); file = file.substr(0, sp); }
        Add(v, L"set:" + p[1], p[0], L"Settings", p[2], C_SET, 0, K_OPEN, file, args);
    }

    // user commands
    for (auto& c : g_cmds)
        Add(v, L"cmd:custom:" + c.name, c.name, L"Custom command \u00B7 " + c.target, c.kw, C_CMD, 0xE756, K_OPEN, c.target, c.args);

    // apps
    for (auto& a : g_apps) Add(v, L"app:" + a.id, a.name, L"Application", L"", C_APP, 0, K_APP, a.id);

    g_catalog.swap(v);
    g_catalogDirty = false;
}

static void LoadAppsCache() {
    g_appsLoaded = true;
    vector<wstring> ln;
    if (!ReadLines(g_appsPath, ln)) return;
    for (auto& l : ln) { auto p = Split(l, L'\t'); if (p.size() == 2 && !p[0].empty()) g_apps.push_back({ p[0], p[1] }); }
}

static void EnsureCatalog() {
    LoadState();
    if (!g_appsLoaded) { LoadAppsCache(); g_catalogDirty = true; }
    if (g_catalogDirty) BuildCatalog();
}

// ------------------------------------------------------------------ app scanning (background thread)
static void ScanLnk(const wstring& dir, vector<AppEntry>& out, int depth) {
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW((dir + L"\\*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        wstring n = fd.cFileName;
        if (n == L"." || n == L"..") continue;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) { if (depth < 4) ScanLnk(dir + L"\\" + n, out, depth + 1); }
        else if (n.size() > 4 && lower(n.substr(n.size() - 4)) == L".lnk") out.push_back({ n.substr(0, n.size() - 4), L"lnk:" + dir + L"\\" + n });
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

static void ScanApps(vector<AppEntry>& out) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    IShellItem* folder = nullptr;
    if (SUCCEEDED(SHGetKnownFolderItem(FOLDERID_AppsFolder, KF_FLAG_DEFAULT, nullptr, IID_PPV_ARGS(&folder))) && folder) {
        IEnumShellItems* en = nullptr;
        if (SUCCEEDED(folder->BindToHandler(nullptr, BHID_EnumItems, IID_PPV_ARGS(&en))) && en) {
            IShellItem* it = nullptr; ULONG got = 0;
            while (en->Next(1, &it, &got) == S_OK && got == 1) {
                PWSTR name = nullptr, id = nullptr;
                if (SUCCEEDED(it->GetDisplayName(SIGDN_NORMALDISPLAY, &name)) && SUCCEEDED(it->GetDisplayName(SIGDN_PARENTRELATIVEPARSING, &id)) && name && id && *name && *id)
                    out.push_back({ name, id });
                if (name) CoTaskMemFree(name);
                if (id) CoTaskMemFree(id);
                it->Release();
            }
            en->Release();
        }
        folder->Release();
    }
    if (out.empty()) {   // fallback: Start Menu shortcuts
        PWSTR p = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_CommonPrograms, 0, nullptr, &p))) { ScanLnk(p, out, 0); CoTaskMemFree(p); }
        p = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Programs, 0, nullptr, &p))) { ScanLnk(p, out, 0); CoTaskMemFree(p); }
    }
    CoUninitialize();
}

static DWORD WINAPI AppsThread(LPVOID) {
    SetThreadPriority(GetCurrentThread(), THREAD_MODE_BACKGROUND_BEGIN);
    auto* out = new vector<AppEntry>();
    ScanApps(*out);
    if (out->empty()) { delete out; return 0; }
    wstring txt;
    for (auto& a : *out) txt += a.name + L"\t" + a.id + L"\n";
    WriteText(g_appsPath, txt);
    PostMessageW(g_hwnd, WM_APPS_READY, 0, (LPARAM)out);
    return 0;
}
static void StartAppsThread() {
    HANDLE h = CreateThread(nullptr, 0, AppsThread, nullptr, 0, nullptr);
    if (h) CloseHandle(h);
}

// ------------------------------------------------------------------ calculator
struct Calc {
    const wchar_t* p; bool ok = true;
    void ws() { while (*p == L' ') p++; }
    double expr() {
        double v = term();
        for (;;) { ws(); if (*p == L'+') { p++; v += term(); } else if (*p == L'-') { p++; v -= term(); } else break; }
        return v;
    }
    double term() {
        double v = power();
        for (;;) {
            ws();
            if (*p == L'*') { p++; v *= power(); }
            else if (*p == L'/') { p++; v /= power(); }
            else if (*p == L'%') { p++; v = fmod(v, power()); }
            else break;
        }
        return v;
    }
    double power() { double b = unary(); ws(); if (*p == L'^') { p++; return pow(b, power()); } return b; }
    double unary() { ws(); if (*p == L'-') { p++; return -unary(); } if (*p == L'+') { p++; return unary(); } return primary(); }
    double primary() {
        ws();
        if (*p == L'(') { p++; double v = expr(); ws(); if (*p == L')') p++; else ok = false; return v; }
        wchar_t* e = nullptr; double v = wcstod(p, &e);
        if (e == p) { ok = false; return 0; }
        p = e; return v;
    }
};
static bool TryCalc(const wstring& q, wstring& result) {
    bool digit = false, op = false;
    for (size_t i = 0; i < q.size(); i++) {
        wchar_t c = q[i];
        if (c >= L'0' && c <= L'9') digit = true;
        else if (c == L'+' || c == L'-' || c == L'*' || c == L'/' || c == L'%' || c == L'^') { if (i > 0) op = true; }
        else if (c == L'(' || c == L')') op = op || c == L'(';
        else if (c != L'.' && c != L' ' && c != L',') return false;
    }
    if (!digit || !op) return false;
    wstring e = ReplaceAll(q, L",", L".");
    Calc c; c.p = e.c_str();
    double v = c.expr(); c.ws();
    if (!c.ok || *c.p != 0 || std::isnan(v) || std::isinf(v)) return false;
    wchar_t buf[64]; swprintf(buf, 64, L"%.10g", v);
    result = buf;
    return true;
}

// ------------------------------------------------------------------ search
static bool IsWordChar(wchar_t c) { return iswalnum(c) != 0; }

static int ScoreOne(const wstring& q, const wstring& t, const wstring& kw) {
    if (t == q) return 100;
    if (t.compare(0, q.size(), q) == 0) return 85;
    for (size_t i = 1; i < t.size(); i++)
        if (!IsWordChar(t[i - 1]) && t.compare(i, q.size(), q) == 0) return 70;
    if (t.find(q) != wstring::npos) return 55;
    if (q.size() >= 2) {
        wstring ini; bool st = true;
        for (wchar_t c : t) { if (IsWordChar(c)) { if (st) ini += c; st = false; } else st = true; }
        if (ini.compare(0, q.size(), q) == 0) return 50;
    }
    if (!kw.empty() && kw.find(q) != wstring::npos) return 35;
    if (q.size() >= 3) {
        size_t i = 0;
        for (wchar_t c : t) if (i < q.size() && c == q[i]) i++;
        if (i == q.size()) return 20;
    }
    return 0;
}
static int ScoreText(const wstring& q, const Item& it) {
    int s = ScoreOne(q, it.tl, it.kl);
    if (s > 0 || q.find(L' ') == wstring::npos) return s;
    int sum = 0, n = 0;
    for (auto& tk : Split(q, L' ')) {
        if (tk.empty()) continue;
        int x = ScoreOne(tk, it.tl, it.kl);
        if (x == 0) return 0;
        sum += x; n++;
    }
    return n ? sum / n : 0;
}

static bool LooksLikeUrl(const wstring& q) {
    if (q.find(L' ') != wstring::npos) return false;
    wstring l = lower(q);
    if (l.rfind(L"http://", 0) == 0 || l.rfind(L"https://", 0) == 0 || l.rfind(L"localhost", 0) == 0) return true;
    size_t slash = l.find(L'/');
    wstring host = l.substr(0, slash);
    size_t colon = host.find(L':');
    if (colon != wstring::npos) host = host.substr(0, colon);
    size_t dot = host.rfind(L'.');
    if (dot == wstring::npos || dot == 0) return false;
    static const wchar_t* tlds[] = { L"com", L"org", L"net", L"io", L"dev", L"app", L"in", L"co", L"edu", L"gov", L"ai", L"me", L"tv", L"uk", L"de", L"fr", L"info", L"xyz" };
    wstring tld = host.substr(dot + 1);
    for (auto t : tlds) if (tld == t) return true;
    return false;
}

static Item MakeBangItem(const Bang& b, const wstring& text, Cat cat) {
    Item it;
    it.cat = cat; it.group = CatName(cat); it.glyph = DefaultGlyph(cat); it.kind = K_OPEN;
    if (text.empty()) {
        it.title = L"Search " + b.name;
        it.sub = L"Keep typing to search directly on the site \u00B7 Enter opens it";
        size_t s = b.url.find(L"://");
        size_t e = (s == wstring::npos) ? wstring::npos : b.url.find(L'/', s + 3);
        it.a = (e == wstring::npos) ? ReplaceAll(b.url, L"{q}", L"") : b.url.substr(0, e);
    } else {
        it.title = L"Search " + b.name + L" for \u201C" + text + L"\u201D";
        it.sub = L"Web search";
        it.a = ReplaceAll(b.url, L"{q}", UrlEncode(text));
    }
    return it;
}

static vector<Item> RunQuery(const wstring& raw) {
    EnsureCatalog();
    vector<Item> res;
    wstring q = Trim(raw);

    if (q.empty()) {   // most used
        vector<const Item*> f;
        for (auto& it : g_catalog) if (it.track && !g_hidden.count(it.id) && g_usage.count(it.id)) f.push_back(&it);
        std::sort(f.begin(), f.end(), [](const Item* x, const Item* y) { return g_usage[x->id] > g_usage[y->id]; });
        for (size_t i = 0; i < f.size() && i < 6; i++) { Item c = *f[i]; c.group = L"Frequently used"; res.push_back(c); }
        return res;
    }

    // bang: only the FIRST word counts
    size_t lead = raw.find_first_not_of(L' ');
    wstring r2 = raw.substr(lead);
    size_t sp = r2.find(L' ');
    if (sp != wstring::npos && sp > 0) {
        wstring trig = r2.substr(0, sp);
        for (auto& b : g_bangs)
            if (_wcsicmp(b.trig.c_str(), trig.c_str()) == 0) { res.push_back(MakeBangItem(b, Trim(r2.substr(sp + 1)), C_BANG)); return res; }
    }

    wstring calc;
    if (TryCalc(q, calc)) {
        Item it; it.cat = C_CALC; it.group = CatName(C_CALC); it.glyph = DefaultGlyph(C_CALC);
        it.title = L"= " + calc; it.sub = L"Calculator \u00B7 Enter copies the result"; it.kind = K_COPY; it.a = calc;
        res.push_back(it);
    }
    if (LooksLikeUrl(q)) {
        Item it; it.cat = C_OPEN; it.group = CatName(C_OPEN); it.glyph = DefaultGlyph(C_OPEN);
        wstring l = lower(q);
        wstring url = (l.rfind(L"http", 0) == 0) ? q : (l.rfind(L"localhost", 0) == 0 ? L"http://" : L"https://") + q;
        it.title = L"Open " + q; it.sub = L"Open in your browser"; it.kind = K_OPEN; it.a = url;
        res.push_back(it);
    }
    if (q.size() > 2 && (q[1] == L':' || q.rfind(L"\\\\", 0) == 0) && PathFileExistsW(q.c_str())) {
        Item it; it.cat = C_OPEN; it.group = CatName(C_OPEN); it.glyph = DefaultGlyph(C_OPEN);
        it.title = q; it.sub = PathIsDirectoryW(q.c_str()) ? L"Open folder" : L"Open file"; it.kind = K_OPEN; it.a = q;
        res.push_back(it);
    }

    // indexed items, best-matching category first
    wstring ql = lower(q);
    struct Sc { int idx, s; };
    vector<Sc> cand;
    int best[C_COUNT] = {};
    for (size_t i = 0; i < g_catalog.size(); i++) {
        const Item& it = g_catalog[i];
        if (g_hidden.count(it.id)) continue;
        int s = ScoreText(ql, it);
        if (s <= 0) continue;
        auto u = g_usage.find(it.id);
        if (u != g_usage.end()) s += std::min(u->second, 15);
        cand.push_back({ (int)i, s });
        best[it.cat] = std::max(best[it.cat], s);
    }
    vector<int> order;
    for (int c = 0; c < C_COUNT; c++) if (best[c] > 0) order.push_back(c);
    std::sort(order.begin(), order.end(), [&](int x, int y) { return best[x] > best[y]; });
    for (int c : order) {
        vector<Sc> part;
        for (auto& s : cand) if ((int)g_catalog[s.idx].cat == c) part.push_back(s);
        std::sort(part.begin(), part.end(), [](const Sc& x, const Sc& y) {
            if (x.s != y.s) return x.s > y.s;
            return g_catalog[x.idx].title.size() < g_catalog[y.idx].title.size();
        });
        size_t lim = (c == C_APP) ? 6 : 4;
        for (size_t i = 0; i < part.size() && i < lim; i++) res.push_back(g_catalog[part[i].idx]);
    }

    // always offer a web search at the bottom
    const Bang* def = nullptr;
    for (auto& b : g_bangs) if (_wcsicmp(b.trig.c_str(), g_defaultBang.c_str()) == 0) def = &b;
    if (!def && !g_bangs.empty()) def = &g_bangs[0];
    if (def) res.push_back(MakeBangItem(*def, q, C_WEB));
    return res;
}

// ------------------------------------------------------------------ actions
static void SetClipboard(const wstring& s) {
    if (!OpenClipboard(g_hwnd)) return;
    EmptyClipboard();
    HGLOBAL m = GlobalAlloc(GMEM_MOVEABLE, (s.size() + 1) * sizeof(wchar_t));
    if (m) {
        memcpy(GlobalLock(m), s.c_str(), (s.size() + 1) * sizeof(wchar_t));
        GlobalUnlock(m);
        SetClipboardData(CF_UNICODETEXT, m);
    }
    CloseClipboard();
}
static wstring GetClipboard() {
    wstring r;
    if (!OpenClipboard(g_hwnd)) return r;
    HANDLE h = GetClipboardData(CF_UNICODETEXT);
    if (h) { auto* p = (const wchar_t*)GlobalLock(h); if (p) { r = p; GlobalUnlock(h); } }
    CloseClipboard();
    return r;
}
static void OpenNotepad(const wstring& path) {
    ShellExecuteW(nullptr, L"open", L"notepad.exe", (L"\"" + path + L"\"").c_str(), nullptr, SW_SHOWNORMAL);
}

static void Exec(const Item& it) {
    if (it.confirm && MessageBoxW(nullptr, (it.title + L"?").c_str(), L"Quick Palette", MB_YESNO | MB_ICONQUESTION | MB_TOPMOST | MB_SETFOREGROUND) != IDYES) return;
    switch (it.kind) {
        case K_OPEN:
            ShellExecuteW(nullptr, L"open", ExpandEnv(it.a).c_str(), it.b.empty() ? nullptr : ExpandEnv(it.b).c_str(), nullptr, it.hide ? SW_HIDE : SW_SHOWNORMAL);
            break;
        case K_APP:
            if (it.a.rfind(L"lnk:", 0) == 0) ShellExecuteW(nullptr, L"open", it.a.c_str() + 4, nullptr, nullptr, SW_SHOWNORMAL);
            else ShellExecuteW(nullptr, L"open", L"explorer.exe", (L"shell:AppsFolder\\" + it.a).c_str(), nullptr, SW_SHOWNORMAL);
            break;
        case K_LOCK: LockWorkStation(); break;
        case K_RECYCLE: SHEmptyRecycleBinW(nullptr, nullptr, SHERB_NOCONFIRMATION | SHERB_NOPROGRESSUI | SHERB_NOSOUND); break;
        case K_QUIT: DestroyWindow(g_hwnd); break;
        case K_EDITCFG: OpenNotepad(g_cfgPath); break;
        case K_RELOAD: StartAppsThread(); break;
        case K_UNHIDE: g_hidden.clear(); DeleteFileW(g_hiddenPath.c_str()); break;
        case K_EDITHIDDEN:
            if (!PathFileExistsW(g_hiddenPath.c_str())) WriteText(g_hiddenPath, L"");
            OpenNotepad(g_hiddenPath);
            break;
        case K_COPY: SetClipboard(it.a); break;
        case K_CFGDIR: ShellExecuteW(nullptr, L"open", L"explorer.exe", (L"\"" + g_dir + L"\"").c_str(), nullptr, SW_SHOWNORMAL); break;
        default: break;
    }
}

// ------------------------------------------------------------------ layout
static int ContentH() { int h = 0; for (auto& r : g_rows) h += r.header ? px(28) : px(50); return h; }
static int ListH() { return std::min(ContentH(), px(430)); }
static int ListTop() { return px(60) + 1 + px(4); }
static int TotalH() { int h = px(60); if (!g_rows.empty()) h += 1 + px(4) + ListH() + px(6) + px(28); return h; }

static bool g_win11 = false;

static void ApplyLayout() {
    int w = px(640), h = TotalH();
    SetWindowPos(g_hwnd, nullptr, 0, 0, w, h, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    if (!g_win11) SetWindowRgn(g_hwnd, CreateRoundRectRgn(0, 0, w + 1, h + 1, px(14), px(14)), TRUE);
}

static void BuildRows() {
    g_rows.clear();
    wstring last;
    for (size_t i = 0; i < g_results.size(); i++) {
        if (g_results[i].group != last) { last = g_results[i].group; g_rows.push_back({ true, -1, last }); }
        g_rows.push_back({ false, (int)i, L"" });
    }
}

static void EnsureVisible() {
    int cy = 0;
    for (size_t i = 0; i < g_rows.size(); i++) {
        int h = g_rows[i].header ? px(28) : px(50);
        if (!g_rows[i].header && g_rows[i].idx == g_sel) {
            int top = cy;
            if (i > 0 && g_rows[i - 1].header) top -= px(28);
            int bot = cy + h, lh = ListH();
            if (top < g_scroll) g_scroll = top;
            else if (bot > g_scroll + lh) g_scroll = bot - lh;
            break;
        }
        cy += h;
    }
    int mx = std::max(0, ContentH() - ListH());
    g_scroll = std::max(0, std::min(g_scroll, mx));
}

static int HitTest(int y) {
    int lt = ListTop();
    if (y < lt || y >= lt + ListH()) return -1;
    int cy = lt - g_scroll;
    for (auto& r : g_rows) {
        int h = r.header ? px(28) : px(50);
        if (y >= cy && y < cy + h) return r.header ? -1 : r.idx;
        cy += h;
    }
    return -1;
}

static void UpdateCaret() {
    HDC dc = GetDC(g_hwnd);
    HGDIOBJ o = SelectObject(dc, g_fSearch);
    SIZE sz = { 0, 0 };
    GetTextExtentPoint32W(dc, g_query.c_str(), g_caret, &sz);
    SelectObject(dc, o);
    ReleaseDC(g_hwnd, dc);
    SetCaretPos(px(52) + sz.cx, (px(60) - px(26)) / 2);
}

static void Refresh() {
    g_results = RunQuery(g_query);
    BuildRows();
    g_sel = g_results.empty() ? -1 : 0;
    g_hover = -1; g_scroll = 0;
    ApplyLayout();
    InvalidateRect(g_hwnd, nullptr, FALSE);
}

// ------------------------------------------------------------------ fonts + drawing
static HFONT MakeFont(int h, int weight, const wchar_t* face) {
    return CreateFontW(-h, 0, 0, 0, weight, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, face);
}
static void SetDpi(int dpi) {
    if (dpi == g_dpi && g_fTitle) return;
    g_dpi = dpi;
    for (HFONT* f : { &g_fSearch, &g_fTitle, &g_fSub, &g_fHdr, &g_fIcon }) if (*f) { DeleteObject(*f); *f = nullptr; }
    g_fSearch = MakeFont(px(22), 400, L"Segoe UI");
    g_fTitle = MakeFont(px(15), 400, L"Segoe UI");
    g_fSub = MakeFont(px(12), 400, L"Segoe UI");
    g_fHdr = MakeFont(px(12), 600, L"Segoe UI");
    g_fIcon = MakeFont(px(17), 400, L"Segoe MDL2 Assets");
}

static COLORREF Mix(COLORREF a, COLORREF b, int pct) {   // pct of b
    return RGB(GetRValue(a) + (GetRValue(b) - GetRValue(a)) * pct / 100,
               GetGValue(a) + (GetGValue(b) - GetGValue(a)) * pct / 100,
               GetBValue(a) + (GetBValue(b) - GetBValue(a)) * pct / 100);
}
static void FillRound(HDC dc, RECT r, COLORREF c, int rad) {
    HBRUSH b = CreateSolidBrush(c);
    HGDIOBJ ob = SelectObject(dc, b), op = SelectObject(dc, GetStockObject(NULL_PEN));
    RoundRect(dc, r.left, r.top, r.right + 1, r.bottom + 1, rad, rad);
    SelectObject(dc, ob); SelectObject(dc, op);
    DeleteObject(b);
}
static void Text(HDC dc, HFONT f, COLORREF c, const wstring& s, RECT r, UINT flags = DT_LEFT) {
    SelectObject(dc, f);
    SetTextColor(dc, c);
    DrawTextW(dc, s.c_str(), (int)s.size(), &r, flags | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
}

static const COLORREF BG = RGB(31, 31, 35), BORDER = RGB(64, 64, 70), DIM = RGB(130, 130, 138);

static void Draw(HDC dc, int W, int H) {
    HBRUSH bg = CreateSolidBrush(BG);
    RECT all = { 0, 0, W, H };
    FillRect(dc, &all, bg);
    DeleteObject(bg);
    HBRUSH bd = CreateSolidBrush(BORDER);
    FrameRect(dc, &all, bd);
    DeleteObject(bd);
    SetBkMode(dc, TRANSPARENT);

    // search row
    wchar_t g[2] = { 0xE721, 0 };
    Text(dc, g_fIcon, RGB(150, 150, 158), g, { px(20), 0, px(48), px(60) }, DT_VCENTER);
    RECT qr = { px(52), 0, W - px(20), px(60) };
    if (g_query.empty()) Text(dc, g_fSearch, RGB(105, 105, 112), L"Search apps, settings, commands\u2026   try: g cats  |  yt lofi", qr, DT_VCENTER);
    else Text(dc, g_fSearch, RGB(245, 245, 247), g_query, qr, DT_VCENTER);

    if (g_rows.empty()) return;

    HBRUSH ln = CreateSolidBrush(RGB(52, 52, 58));
    RECT lr = { 1, px(60), W - 1, px(60) + 1 };
    FillRect(dc, &lr, ln);
    DeleteObject(ln);

    int lt = ListTop(), lh = ListH();
    int sc = SaveDC(dc);
    IntersectClipRect(dc, 0, lt, W, lt + lh);
    int y = lt - g_scroll;
    for (auto& r : g_rows) {
        int h = r.header ? px(28) : px(50);
        if (y + h > lt && y < lt + lh) {
            if (r.header) {
                Text(dc, g_fHdr, RGB(140, 140, 148), r.text, { px(22), y + px(8), W - px(20), y + h }, 0);
            } else {
                const Item& it = g_results[r.idx];
                RECT rr = { px(8), y + px(1), W - px(8), y + h - px(1) };
                if (r.idx == g_sel) FillRound(dc, rr, RGB(62, 62, 69), px(8));
                else if (r.idx == g_hover) FillRound(dc, rr, RGB(46, 46, 52), px(8));
                COLORREF ac = Accent(it.cat);
                int ts = px(34);
                RECT tile = { px(18), y + (h - ts) / 2, px(18) + ts, y + (h - ts) / 2 + ts };
                FillRound(dc, tile, Mix(r.idx == g_sel ? RGB(62, 62, 69) : BG, ac, 24), px(9));
                wchar_t gl[2] = { it.glyph, 0 };
                Text(dc, g_fIcon, ac, gl, tile, DT_CENTER | DT_VCENTER);
                int tx = tile.right + px(12);
                Text(dc, g_fTitle, RGB(245, 245, 247), it.title, { tx, y + px(7), W - px(20), y + px(27) }, 0);
                Text(dc, g_fSub, DIM, it.sub, { tx, y + px(27), W - px(20), y + px(45) }, 0);
            }
        }
        y += h;
    }
    RestoreDC(dc, sc);

    // footer
    int fy = lt + lh + px(6);
    RECT fr = { 1, fy, W - 1, H - 1 };
    HBRUSH fb = CreateSolidBrush(RGB(26, 26, 30));
    FillRect(dc, &fr, fb);
    DeleteObject(fb);
    Text(dc, g_fSub, RGB(110, 110, 118), L"\u21B5 Open      \u2191\u2193 Move      Ctrl+H Hide item      Esc Close", { px(20), fy, W - px(20), H - 1 }, DT_VCENTER);
}

// ------------------------------------------------------------------ show / hide
static void ForceForeground(HWND h) {
    HWND fg = GetForegroundWindow();
    DWORD ft = fg ? GetWindowThreadProcessId(fg, nullptr) : 0, ct = GetCurrentThreadId();
    if (ft && ft != ct) AttachThreadInput(ct, ft, TRUE);
    SetForegroundWindow(h); BringWindowToTop(h); SetFocus(h);
    if (ft && ft != ct) AttachThreadInput(ct, ft, FALSE);
}

static void ShowPalette() {
    if (IsWindowVisible(g_hwnd)) return;
    if (Stamp(g_cfgPath) != g_cfgStamp) { LoadConfig(); SyncStartup(); RegisterHotkey(); }
    POINT pt; GetCursorPos(&pt);
    HMONITOR mon = MonitorFromPoint(pt, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO mi = { sizeof(mi) };
    GetMonitorInfoW(mon, &mi);
    UINT dx = 96, dy = 96;
    GetDpiForMonitor(mon, MDT_EFFECTIVE_DPI, &dx, &dy);
    SetDpi((int)dx);
    g_query.clear(); g_caret = 0;
    Refresh();
    int w = px(640);
    int x = mi.rcWork.left + ((mi.rcWork.right - mi.rcWork.left) - w) / 2;
    int y = mi.rcWork.top + (mi.rcWork.bottom - mi.rcWork.top) * 16 / 100;
    SetWindowPos(g_hwnd, HWND_TOPMOST, x, y, w, TotalH(), SWP_SHOWWINDOW);
    ForceForeground(g_hwnd);
}

static void HidePalette() {
    if (!IsWindowVisible(g_hwnd)) return;
    ShowWindow(g_hwnd, SW_HIDE);
    SetTimer(g_hwnd, T_TRIM, 800, nullptr);   // give memory back shortly after
}

static void RunSelected() {
    if (g_sel < 0 || g_sel >= (int)g_results.size()) return;
    Item it = g_results[g_sel];
    HidePalette();
    if (it.track) { g_usage[it.id]++; SaveUsage(); }
    Exec(it);
}

static void HideIndex(int i) {
    if (i < 0 || i >= (int)g_results.size() || !g_results[i].track) return;
    const Item& it = g_results[i];
    g_hidden[it.id] = it.title;
    WriteText(g_hiddenPath, it.id + L"\t" + it.title + L"\n", L"a, ccs=UTF-8");
    int keep = g_sel;
    Refresh();
    if (!g_results.empty()) { g_sel = std::min(keep, (int)g_results.size() - 1); EnsureVisible(); }
}

static void Move(int d) {
    int n = (int)g_results.size();
    if (!n) return;
    g_sel = (g_sel + d + n) % n;
    EnsureVisible();
    InvalidateRect(g_hwnd, nullptr, FALSE);
}

static void QueryChanged() { Refresh(); UpdateCaret(); }

// ------------------------------------------------------------------ tray
static void AddTray() {
    ZeroMemory(&g_nid, sizeof g_nid);
    g_nid.cbSize = sizeof g_nid; g_nid.hWnd = g_hwnd; g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAY;
    g_nid.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wcscpy_s(g_nid.szTip, L"Quick Palette");
    Shell_NotifyIconW(NIM_ADD, &g_nid);
}
static void Balloon(const wchar_t* title, const wchar_t* msg) {
    NOTIFYICONDATAW n = g_nid;
    n.uFlags = NIF_INFO; n.dwInfoFlags = NIIF_WARNING;
    wcscpy_s(n.szInfoTitle, title); wcscpy_s(n.szInfo, msg);
    Shell_NotifyIconW(NIM_MODIFY, &n);
}
static void TrayMenu() {
    HMENU m = CreatePopupMenu();
    AppendMenuW(m, MF_STRING, 1, L"Open palette");
    AppendMenuW(m, MF_STRING, 2, L"Edit settings");
    AppendMenuW(m, MF_STRING, 3, L"Reload app list");
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING, 4, L"Quit");
    POINT p; GetCursorPos(&p);
    SetForegroundWindow(g_hwnd);
    int c = TrackPopupMenu(m, TPM_RETURNCMD | TPM_RIGHTBUTTON, p.x, p.y, 0, g_hwnd, nullptr);
    PostMessageW(g_hwnd, WM_NULL, 0, 0);
    DestroyMenu(m);
    if (c == 1) ShowPalette();
    else if (c == 2) OpenNotepad(g_cfgPath);
    else if (c == 3) StartAppsThread();
    else if (c == 4) DestroyWindow(g_hwnd);
}

// ------------------------------------------------------------------ window procedure
static LRESULT CALLBACK WndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_HOTKEY:
            if (IsWindowVisible(h)) HidePalette(); else ShowPalette();
            return 0;
        case WM_SHOWME: ShowPalette(); return 0;
        case WM_APPS_READY: {
            auto* v = (vector<AppEntry>*)lp;
            g_apps = std::move(*v); delete v;
            g_appsLoaded = true; g_catalogDirty = true;
            if (IsWindowVisible(h)) { Refresh(); }
            return 0;
        }
        case WM_TRAY:
            if (LOWORD(lp) == WM_LBUTTONUP) ShowPalette();
            else if (LOWORD(lp) == WM_RBUTTONUP) TrayMenu();
            return 0;
        case WM_TIMER:
            if (wp == T_BOOT) {
                KillTimer(h, T_BOOT);
                if (g_boot) SetPriorityClass(GetCurrentProcess(), PROCESS_MODE_BACKGROUND_END);
                FILETIME now; GetSystemTimeAsFileTime(&now);
                ULONGLONG n = ((ULONGLONG)now.dwHighDateTime << 32) | now.dwLowDateTime, s = Stamp(g_appsPath);
                if (s == 0 || n - s > 864000000000ULL) StartAppsThread();   // older than 24h
                SetTimer(h, T_TRIM, 1500, nullptr);
            } else if (wp == T_TRIM) {
                KillTimer(h, T_TRIM);
                if (!IsWindowVisible(h)) SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);
            }
            return 0;
        case WM_ACTIVATE:
            if (LOWORD(wp) == WA_INACTIVE && !g_menuOpen) HidePalette();
            return 0;
        case WM_SETFOCUS:
            CreateCaret(h, nullptr, px(2), px(26));
            UpdateCaret();
            ShowCaret(h);
            return 0;
        case WM_KILLFOCUS: DestroyCaret(); return 0;
        case WM_ERASEBKGND: return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(h, &ps);
            RECT rc; GetClientRect(h, &rc);
            HideCaret(h);
            HDC m = CreateCompatibleDC(dc);
            HBITMAP bmp = CreateCompatibleBitmap(dc, rc.right, rc.bottom);
            HGDIOBJ old = SelectObject(m, bmp);
            Draw(m, rc.right, rc.bottom);
            BitBlt(dc, 0, 0, rc.right, rc.bottom, m, 0, 0, SRCCOPY);
            SelectObject(m, old); DeleteObject(bmp); DeleteDC(m);
            ShowCaret(h);
            EndPaint(h, &ps);
            return 0;
        }
        case WM_CHAR: {
            wchar_t c = (wchar_t)wp;
            if (c >= 32 && c != 127) { g_query.insert(g_caret, 1, c); g_caret++; QueryChanged(); }
            return 0;
        }
        case WM_KEYDOWN: {
            bool ctrl = GetKeyState(VK_CONTROL) < 0, shift = GetKeyState(VK_SHIFT) < 0;
            switch (wp) {
                case VK_ESCAPE: HidePalette(); return 0;
                case VK_DOWN: Move(1); return 0;
                case VK_UP: Move(-1); return 0;
                case VK_TAB: Move(shift ? -1 : 1); return 0;
                case VK_RETURN: RunSelected(); return 0;
                case VK_BACK:
                    if (g_caret > 0) {
                        if (ctrl) {
                            int s = g_caret;
                            while (s > 0 && g_query[s - 1] == L' ') s--;
                            while (s > 0 && g_query[s - 1] != L' ') s--;
                            g_query.erase(s, g_caret - s); g_caret = s;
                        } else g_query.erase(--g_caret, 1);
                        QueryChanged();
                    }
                    return 0;
                case VK_DELETE:
                    if (g_caret < (int)g_query.size()) { g_query.erase(g_caret, 1); QueryChanged(); }
                    return 0;
                case VK_LEFT: if (g_caret > 0) { g_caret--; UpdateCaret(); } return 0;
                case VK_RIGHT: if (g_caret < (int)g_query.size()) { g_caret++; UpdateCaret(); } return 0;
                case VK_HOME: g_caret = 0; UpdateCaret(); return 0;
                case VK_END: g_caret = (int)g_query.size(); UpdateCaret(); return 0;
                case 'H': if (ctrl) { HideIndex(g_sel); return 0; } break;
                case 'V':
                    if (ctrl) {
                        wstring t = GetClipboard();
                        for (auto& c : t) if (c < 32) c = L' ';
                        if (!t.empty()) { g_query.insert(g_caret, t); g_caret += (int)t.size(); QueryChanged(); }
                        return 0;
                    }
                    break;
            }
            break;
        }
        case WM_MOUSEMOVE: {
            int i = HitTest(GET_Y_LPARAM(lp));
            if (i != g_hover) { g_hover = i; InvalidateRect(h, nullptr, FALSE); }
            TRACKMOUSEEVENT t = { sizeof t, TME_LEAVE, h, 0 };
            TrackMouseEvent(&t);
            return 0;
        }
        case WM_MOUSELEAVE:
            if (g_hover != -1) { g_hover = -1; InvalidateRect(h, nullptr, FALSE); }
            return 0;
        case WM_MOUSEWHEEL: {
            g_scroll -= GET_WHEEL_DELTA_WPARAM(wp) * px(50) / WHEEL_DELTA;
            int mx = std::max(0, ContentH() - ListH());
            g_scroll = std::max(0, std::min(g_scroll, mx));
            InvalidateRect(h, nullptr, FALSE);
            return 0;
        }
        case WM_LBUTTONUP: {
            int i = HitTest(GET_Y_LPARAM(lp));
            if (i >= 0) { g_sel = i; RunSelected(); }
            return 0;
        }
        case WM_RBUTTONUP: {
            int i = HitTest(GET_Y_LPARAM(lp));
            if (i >= 0 && g_results[i].track) {
                g_menuOpen = true;
                HMENU m = CreatePopupMenu();
                AppendMenuW(m, MF_STRING, 1, L"Hide from suggestions");
                POINT p; GetCursorPos(&p);
                int c = TrackPopupMenu(m, TPM_RETURNCMD | TPM_RIGHTBUTTON, p.x, p.y, 0, h, nullptr);
                DestroyMenu(m);
                g_menuOpen = false;
                if (c == 1) HideIndex(i);
                SetFocus(h);
            }
            return 0;
        }
        case WM_DESTROY:
            Shell_NotifyIconW(NIM_DELETE, &g_nid);
            UnregisterHotKey(h, 1);
            PostQuitMessage(0);
            return 0;
        default:
            if (msg == g_wmTaskbar && g_wmTaskbar) { AddTray(); return 0; }
    }
    return DefWindowProcW(h, msg, wp, lp);
}

// ------------------------------------------------------------------ entry
static LONG WINAPI OnCrash(EXCEPTION_POINTERS* e) {
    FILE* f = nullptr;
    if (_wfopen_s(&f, (g_dir + L"\\crash.log").c_str(), L"a") == 0 && f) {
        fprintf(f, "crash code=0x%08lX addr=%p\n", e->ExceptionRecord->ExceptionCode, e->ExceptionRecord->ExceptionAddress);
        fclose(f);
    }
    return EXCEPTION_EXECUTE_HANDLER;
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR cmd, int) {
    g_inst = inst;
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    // paths
    PWSTR roam = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &roam))) { g_dir = wstring(roam) + L"\\QuickPalette"; CoTaskMemFree(roam); }
    else g_dir = L".";
    CreateDirectoryW(g_dir.c_str(), nullptr);
    g_cfgPath = g_dir + L"\\config.ini";
    g_usagePath = g_dir + L"\\usage.txt";
    g_hiddenPath = g_dir + L"\\hidden.txt";
    g_appsPath = g_dir + L"\\apps.txt";
    SetUnhandledExceptionFilter(OnCrash);

    // one instance; launching again just opens the palette
    HANDLE mtx = CreateMutexW(nullptr, TRUE, L"Local\\QuickPalette.v2");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND w = FindWindowW(kClass, nullptr);
        if (w) PostMessageW(w, WM_SHOWME, 0, 0);
        return 0;
    }

    g_boot = cmd && wcsstr(cmd, L"--boot") != nullptr;
    if (g_boot) SetPriorityClass(GetCurrentProcess(), PROCESS_MODE_BACKGROUND_BEGIN);   // low CPU + low disk priority while Windows boots

    LoadConfig();
    SyncStartup();
    SetDpi(96);

    WNDCLASSEXW wc = { sizeof wc };
    wc.style = CS_DROPSHADOW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kClass;
    RegisterClassExW(&wc);

    g_hwnd = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST, kClass, L"Quick Palette", WS_POPUP, 0, 0, px(640), px(60), nullptr, nullptr, inst, nullptr);
    if (!g_hwnd) return 1;
    int pref = 2;   // rounded corners (only succeeds on Windows 11)
    g_win11 = SUCCEEDED(DwmSetWindowAttribute(g_hwnd, 33, &pref, sizeof pref));

    g_wmTaskbar = RegisterWindowMessageW(L"TaskbarCreated");
    AddTray();
    if (!RegisterHotkey())
        Balloon(L"Quick Palette", L"The shortcut in config.ini is invalid or used by another app. Edit it via the tray icon > Edit settings.");

    // heavy work waits: 45 s after boot, 2.5 s after a manual start
    SetTimer(g_hwnd, T_BOOT, g_boot ? 45000 : 2500, nullptr);

    MSG m;
    while (GetMessageW(&m, nullptr, 0, 0) > 0) { TranslateMessage(&m); DispatchMessageW(&m); }
    if (mtx) CloseHandle(mtx);
    return 0;
}
