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
#include <powrprof.h>
#include <uxtheme.h>
#include <cstdint>
#include <string_view>
#include <unordered_map>
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
#pragma comment(lib, "powrprof.lib")
#pragma comment(lib, "uxtheme.lib")

using std::wstring;
using std::vector;

// ------------------------------------------------------------------ types
enum Cat { C_APP, C_SET, C_CMD, C_CALC, C_OPEN, C_WEB, C_BANG, C_FILE, C_SNIP, C_COUNT };
enum Kind { K_NONE, K_OPEN, K_APP, K_LOCK, K_RECYCLE, K_QUIT, K_EDITCFG, K_RELOAD, K_UNHIDE, K_EDITHIDDEN, K_COPY, K_CFGDIR,
            K_SHUTDOWN, K_RESTART, K_LOGOFF, K_SLEEP, K_STARTUP_ON, K_STARTUP_OFF, K_REINDEX, K_NOTE };

struct Item {
    wstring id, title, sub, tl, kl, group;
    Cat cat = C_CMD;
    wchar_t glyph = 0;
    bool track = false, confirm = false, hide = false;
    int score = 0;
    Kind kind = K_NONE;
    wstring a, b, alt;
};
struct Snippet { wstring name, text; };
struct Bang { wstring trig, name, url; };
struct AppEntry { wstring name, id; };
struct CustomCmd { wstring name, kw, target, args; };
struct Row { bool header; int idx; wstring text; };

// ------------------------------------------------------------------ globals
static HINSTANCE g_inst;
static HWND g_hwnd;
static const wchar_t* kClass = L"QuickPaletteWnd";
static const UINT WM_TRAY = WM_APP + 1, WM_APPS_READY = WM_APP + 2, WM_SHOWME = WM_APP + 3, WM_FILES_READY = WM_APP + 4;
static UINT g_wmTaskbar;
enum { T_BOOT = 1, T_TRIM = 2, T_AUTOSAVE = 3, T_PREVIEW = 4, T_FREE = 5 };

static wstring g_dir, g_cfgPath, g_usagePath, g_hiddenPath, g_appsPath, g_filesPath;
static wstring g_hotkey = L"Ctrl+Space", g_defaultBang = L"g";
static bool g_startup = false, g_boot = false, g_menuOpen = false;
static ULONGLONG g_cfgStamp = 0;

static vector<Bang> g_bangs;
static vector<CustomCmd> g_cmds;
static vector<Snippet> g_snippets;
static vector<wstring> g_folders;
static wstring g_fileTrigger = L"f";
static int g_fileDepth = 0, g_fileMax = 100000;   // depth 0 = unlimited
static bool g_firstRun = false;
struct FilePool {   // all indexed paths in ONE buffer (much lighter than a vector of strings)
    wstring buf; vector<uint32_t> off;
    std::wstring_view at(size_t i) const {
        size_t st = off[i], e = (i + 1 < off.size()) ? off[i + 1] - 1 : buf.size() - 1;
        return std::wstring_view(buf.data() + st, e - st);
    }
};
static FilePool g_files;
struct NoteDef { wstring trig, name, path; };
static vector<NoteDef> g_notes;
static bool g_compact = false, g_showRecent = true, g_atCursor = false, g_hideOnFocusLoss = true, g_showFooter = true, g_fileHidden = false;
static int g_recentCount = 6, g_width = 640, g_listMax = 430, g_appsLimit = 6, g_otherLimit = 4;
static int g_noteFontSize = 15, g_noteW = 1000, g_noteH = 680;
static wstring g_themeMode = L"auto", g_noteFont = L"Consolas", g_noteViewCfg = L"split";
static wstring g_fileSkip = L"node_modules|.git|$RECYCLE.BIN|System Volume Information";
static bool g_filesLoaded = false;
static volatile LONG g_filesBusy = 0;
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

static bool ReadFileUtf8(const wstring& path, wstring& out, size_t maxBytes) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER sz;
    if (!GetFileSizeEx(h, &sz) || (unsigned long long)sz.QuadPart > maxBytes) { CloseHandle(h); return false; }
    std::string b((size_t)sz.QuadPart, '\0');
    size_t got = 0; bool ok = true;
    while (got < b.size()) {
        DWORD r = 0;
        if (!ReadFile(h, &b[got], (DWORD)std::min<size_t>(b.size() - got, 1u << 24), &r, nullptr) || r == 0) { ok = false; break; }
        got += r;
    }
    CloseHandle(h);
    if (!ok) return false;
    size_t st = (b.size() >= 3 && (unsigned char)b[0] == 0xEF && (unsigned char)b[1] == 0xBB && (unsigned char)b[2] == 0xBF) ? 3 : 0;
    int n = MultiByteToWideChar(CP_UTF8, 0, b.data() + st, (int)(b.size() - st), nullptr, 0);
    out.assign((size_t)n, L'\0');
    if (n) MultiByteToWideChar(CP_UTF8, 0, b.data() + st, (int)(b.size() - st), &out[0], n);
    return true;
}
static bool WriteFileUtf8(const wstring& path, const wstring& text) {   // writes a temp file, then swaps it in
    int n = WideCharToMultiByte(CP_UTF8, 0, text.data(), (int)text.size(), nullptr, 0, nullptr, nullptr);
    std::string u((size_t)n, '\0');
    if (n) WideCharToMultiByte(CP_UTF8, 0, text.data(), (int)text.size(), &u[0], n, nullptr, nullptr);
    wstring tmp = path + L".tmp";
    HANDLE h = CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    bool ok = true; size_t off = 0;
    while (off < u.size()) {
        DWORD w = 0;
        if (!WriteFile(h, u.data() + off, (DWORD)std::min<size_t>(u.size() - off, 1u << 24), &w, nullptr) || w == 0) { ok = false; break; }
        off += w;
    }
    CloseHandle(h);
    if (!ok) { DeleteFileW(tmp.c_str()); return false; }
    return MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
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
        L"# 1 = start with Windows (waits ~45 s and runs at low priority so it never slows boot), 0 = off\n"
        L"# You can also type \"start with windows\" in the palette to switch this.\n"
        L"startup=0\n"
        L"\n"
        L"# ---- File search: type  f report  to search file and folder NAMES --------------------\n"
        L"# Everything inside each folder= path (all sub-folders too) is indexed. With none set,\n"
        L"# Desktop + Documents + Downloads are used. Only these folders are indexed, never the whole drive.\n"
        L"# folder=D:\\Projects\n"
        L"# folder=%USERPROFILE%\\Pictures\n"
        L"file_trigger=f\n"
        L"file_depth=0\n"
        L"file_max=100000\n"
        L"# folder names that are skipped while indexing (separate with |). Leave empty to index everything.\n"
        L"file_skip=node_modules|.git|$RECYCLE.BIN|System Volume Information\n"
        L"file_include_hidden=0\n"
        L"\n"
        L"# Snippets: snippet=Name|text that gets copied to the clipboard when you press Enter\n"
        L"# snippet=My email|me@example.com\n"
        L"\n"
        L"# ---- Notes: note=trigger|Name|path-to-markdown-file ------------------------------------\n"
        L"# Type the trigger and a space (e.g.  n ) and the palette becomes a full Markdown note editor.\n"
        L"# {date} in the path becomes today's date (daily notes). Relative paths live in Documents.\n"
        L"note=n|Notes|Documents\\QuickNotes.md\n"
        L"# note=j|Journal|D:\\Journal\\{date}.md\n"
        L"note_view=split\n"
        L"note_font=Consolas\n"
        L"note_font_size=15\n"
        L"note_width=1000\n"
        L"note_height=680\n"
        L"\n"
        L"# ---- Look and behavior ------------------------------------------------------------------\n"
        L"theme=auto\n"
        L"compact=0\n"
        L"width=640\n"
        L"list_height=430\n"
        L"show_recent=1\n"
        L"recent_count=6\n"
        L"show_footer=1\n"
        L"at_cursor=0\n"
        L"hide_on_focus_loss=1\n"
        L"apps_limit=6\n"
        L"other_limit=4\n"
        L"# theme: auto (follows Windows) / dark / light.  at_cursor: open at the mouse pointer instead of screen center.\n"
        L"# note_view: edit / split / preview.  show_recent: show your most-used items when the box is empty.\n"
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
    if (!PathFileExistsW(g_cfgPath.c_str())) { DefaultConfig(); g_firstRun = true; }
    vector<wstring> lines;
    ReadLines(g_cfgPath, lines);
    g_bangs.clear(); g_cmds.clear(); g_folders.clear(); g_snippets.clear(); g_notes.clear();
    for (auto& raw : lines) {
        wstring l = Trim(raw);
        if (l.empty() || l[0] == L'#') continue;
        size_t eq = l.find(L'=');
        if (eq == wstring::npos) continue;
        wstring k = lower(Trim(l.substr(0, eq))), v = Trim(l.substr(eq + 1));
        if (k == L"hotkey") g_hotkey = v;
        else if (k == L"startup") g_startup = (v != L"0");
        else if (k == L"default_bang") g_defaultBang = v;
        else if (k == L"folder") { if (!v.empty()) g_folders.push_back(v); }
        else if (k == L"file_trigger") { if (!v.empty()) g_fileTrigger = v; }
        else if (k == L"file_depth") g_fileDepth = std::max(0, std::min(64, _wtoi(v.c_str())));
        else if (k == L"file_max") g_fileMax = std::max(1000, std::min(500000, _wtoi(v.c_str())));
        else if (k == L"file_skip") g_fileSkip = v;
        else if (k == L"file_include_hidden") g_fileHidden = (v == L"1");
        else if (k == L"theme") g_themeMode = lower(v);
        else if (k == L"compact") g_compact = (v == L"1");
        else if (k == L"width") g_width = std::max(420, std::min(1200, _wtoi(v.c_str())));
        else if (k == L"list_height") g_listMax = std::max(150, std::min(900, _wtoi(v.c_str())));
        else if (k == L"show_recent") g_showRecent = (v != L"0");
        else if (k == L"recent_count") g_recentCount = std::max(1, std::min(20, _wtoi(v.c_str())));
        else if (k == L"show_footer") g_showFooter = (v != L"0");
        else if (k == L"at_cursor") g_atCursor = (v == L"1");
        else if (k == L"hide_on_focus_loss") g_hideOnFocusLoss = (v != L"0");
        else if (k == L"apps_limit") g_appsLimit = std::max(1, std::min(30, _wtoi(v.c_str())));
        else if (k == L"other_limit") g_otherLimit = std::max(1, std::min(30, _wtoi(v.c_str())));
        else if (k == L"note_view") g_noteViewCfg = lower(v);
        else if (k == L"note_font") { if (!v.empty()) g_noteFont = v; }
        else if (k == L"note_font_size") g_noteFontSize = std::max(9, std::min(32, _wtoi(v.c_str())));
        else if (k == L"note_width") g_noteW = std::max(560, std::min(2000, _wtoi(v.c_str())));
        else if (k == L"note_height") g_noteH = std::max(360, std::min(1400, _wtoi(v.c_str())));
        else if (k == L"note") {
            auto p = Split(v, L'|');
            if (p.size() >= 3 && !Trim(p[0]).empty() && !Trim(p[2]).empty()) g_notes.push_back({ Trim(p[0]), Trim(p[1]), Trim(p[2]) });
        }
        else if (k == L"snippet") {
            size_t b = v.find(L'|');
            if (b != wstring::npos && b > 0) g_snippets.push_back({ Trim(v.substr(0, b)), Trim(v.substr(b + 1)) });
        }
        else if (k == L"bang") {
            auto p = Split(v, L'|');
            if (p.size() >= 3) g_bangs.push_back({ Trim(p[0]), Trim(p[1]), Trim(p[2]) });
        } else if (k == L"cmd") {
            auto p = Split(v, L'|');
            if (p.size() >= 3) g_cmds.push_back({ Trim(p[0]), Trim(p[1]), Trim(p[2]), p.size() > 3 ? Trim(p[3]) : L"" });
        }
    }
    if (g_notes.empty()) g_notes.push_back({ L"n", L"Notes", L"Documents\\QuickNotes.md" });
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

static void SetConfigValue(const wstring& key, const wstring& val) {
    vector<wstring> ln;
    ReadLines(g_cfgPath, ln);
    bool done = false;
    for (auto& l : ln) {
        wstring t = Trim(l);
        size_t eq = t.find(L'=');
        if (!t.empty() && t[0] != L'#' && eq != wstring::npos && lower(Trim(t.substr(0, eq))) == key) { l = key + L"=" + val; done = true; }
    }
    if (!done) ln.push_back(key + L"=" + val);
    wstring out;
    for (auto& l : ln) out += l + L"\n";
    WriteText(g_cfgPath, out);
    LoadConfig();
    SyncStartup();
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
        case C_CALC: return 0xE8EF; case C_OPEN: return 0xE8A7; case C_FILE: return 0xE8A5; case C_SNIP: return 0xE8C8; default: return 0xE774;
    }
}
static const wchar_t* CatName(Cat c) {
    switch (c) {
        case C_APP: return L"Apps"; case C_SET: return L"Settings"; case C_CMD: return L"Commands";
        case C_CALC: return L"Calculator"; case C_OPEN: return L"Open"; case C_WEB: return L"Web"; case C_FILE: return L"Files"; case C_SNIP: return L"Snippets"; default: return L"Web search";
    }
}
static COLORREF Accent(Cat c) {
    switch (c) {
        case C_APP: return RGB(59, 130, 246); case C_SET: return RGB(20, 184, 166); case C_CMD: return RGB(245, 158, 11);
        case C_CALC: return RGB(236, 72, 153); case C_OPEN: return RGB(100, 116, 139); case C_FILE: return RGB(234, 179, 8); case C_SNIP: return RGB(34, 197, 94); default: return RGB(168, 85, 247);
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
    Add(v, L"cmd:sleep", L"Sleep", L"Put this PC to sleep", L"suspend", C_CMD, 0xE708, K_SLEEP);
    Add(v, L"cmd:signout", L"Sign out", L"Sign out of Windows", L"log off logout", C_CMD, 0xE7E8, K_LOGOFF, L"", L"", true);
    Add(v, L"cmd:restart", L"Restart", L"Restart this PC", L"reboot", C_CMD, 0xE777, K_RESTART, L"", L"", true);
    Add(v, L"cmd:shutdown", L"Shut down", L"Turn off this PC", L"power off turn off", C_CMD, 0xE7E8, K_SHUTDOWN, L"", L"", true);
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

    Add(v, L"cmd:startup-on", L"Start with Windows: turn on", L"Launch Quick Palette at login (delayed, low priority)", L"startup autostart boot login", C_CMD, 0xE7E8, K_STARTUP_ON);
    Add(v, L"cmd:startup-off", L"Start with Windows: turn off", L"Stop launching at login", L"startup autostart boot login disable", C_CMD, 0xE7E8, K_STARTUP_OFF);
    Add(v, L"cmd:reindex", L"Rebuild file index", L"Re-scan the folders used by file search", L"files refresh update search", C_CMD, 0xE72C, K_REINDEX);

    // quick folders
    struct QF { const wchar_t* id; const wchar_t* title; const wchar_t* target; };
    static const QF qf[] = {
        { L"docs", L"Documents", L"shell:Personal" }, { L"downloads", L"Downloads", L"shell:Downloads" },
        { L"desktop", L"Desktop", L"shell:Desktop" }, { L"pictures", L"Pictures", L"shell:My Pictures" },
        { L"music", L"Music", L"shell:My Music" }, { L"videos", L"Videos", L"shell:My Video" },
        { L"recyclebin", L"Recycle Bin", L"shell:RecycleBinFolder" }, { L"thispc", L"This PC", L"shell:MyComputerFolder" },
        { L"startupfolder", L"Startup folder", L"shell:Startup" }, { L"appdata", L"AppData folder", L"shell:AppData" },
        { L"temp", L"Temp folder", L"%TEMP%" },
    };
    for (auto& f : qf) Add(v, wstring(L"cmd:folder-") + f.id, f.title, L"Open folder", L"folder files directory", C_CMD, 0xE8B7, K_OPEN, f.target);

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

    // snippets
    for (auto& sn : g_snippets) Add(v, L"snip:" + sn.name, sn.name, L"Snippet \u00B7 Enter copies it", L"", C_SNIP, 0, K_COPY, sn.text);

    // notes
    for (auto& nd : g_notes) Add(v, L"note:" + nd.trig, L"Open note: " + nd.name, nd.path, L"note notes markdown write journal", C_CMD, 0xE70F, K_NOTE, nd.trig);

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

// ------------------------------------------------------------------ file index (only YOUR folders, background thread)
static vector<wstring> EffectiveFolders() {
    vector<wstring> r;
    for (auto& f : g_folders) {
        wstring e = ExpandEnv(f);
        while (e.size() > 3 && (e.back() == L'\\' || e.back() == L'/')) e.pop_back();
        if (!e.empty()) r.push_back(e);
    }
    if (r.empty()) {
        const KNOWNFOLDERID* ids[] = { &FOLDERID_Desktop, &FOLDERID_Documents, &FOLDERID_Downloads };
        for (auto id : ids) {
            PWSTR p = nullptr;
            if (SUCCEEDED(SHGetKnownFolderPath(*id, 0, nullptr, &p)) && p) { r.push_back(p); CoTaskMemFree(p); }
        }
    }
    return r;
}
static wstring FolderSig() {
    wstring s = L"#";
    for (auto& f : EffectiveFolders()) s += f + L"|";
    return s + std::to_wstring(g_fileDepth) + L"/" + std::to_wstring(g_fileMax) + L"/" + g_fileSkip + L"/" + (g_fileHidden ? L"h" : L"-");
}
static bool FilesCacheFresh() {
    ULONGLONG st = Stamp(g_filesPath);
    if (!st) return false;
    FILETIME now; GetSystemTimeAsFileTime(&now);
    ULONGLONG n = ((ULONGLONG)now.dwHighDateTime << 32) | now.dwLowDateTime;
    if (n - st > 864000000000ULL) return false;   // older than 24h
    FILE* f = nullptr;
    if (_wfopen_s(&f, g_filesPath.c_str(), L"r, ccs=UTF-8") != 0 || !f) return false;
    wchar_t buf[8192]; wstring first;
    if (fgetws(buf, 8192, f)) first = buf;
    fclose(f);
    while (!first.empty() && (first.back() == L'\n' || first.back() == L'\r')) first.pop_back();
    return first == FolderSig();
}
static void BuildOffsets(FilePool& p) {
    p.off.clear();
    if (p.buf.empty()) return;
    if (p.buf.back() != L'\n') p.buf += L'\n';
    p.off.push_back(0);
    for (size_t i = 0; i + 1 < p.buf.size(); i++) if (p.buf[i] == L'\n') p.off.push_back((uint32_t)(i + 1));
}
static void EnsureFiles() {
    if (g_filesLoaded) return;
    g_filesLoaded = true;
    wstring txt;
    if (!ReadFileUtf8(g_filesPath, txt, 300u * 1024 * 1024)) return;
    size_t nl = txt.find(L'\n');
    if (nl == wstring::npos) return;
    wstring first = txt.substr(0, nl);
    if (first != FolderSig()) return;
    txt.erase(0, nl + 1);
    FilePool p; p.buf = std::move(txt);
    BuildOffsets(p);
    g_files = std::move(p);
}
static void FreeFiles() { g_files = FilePool(); g_filesLoaded = false; }

struct ScanJob { vector<wstring> folders; int depth, max; vector<wstring> skip; bool hidden; wstring sig; };

static void ScanFolder(const wstring& dir, int depth, const ScanJob& job, FilePool& pool) {
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileExW((dir + L"\\*").c_str(), FindExInfoBasic, &fd, FindExSearchNameMatch, nullptr, FIND_FIRST_EX_LARGE_FETCH);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if ((int)pool.off.size() >= job.max) break;
        const wchar_t* n = fd.cFileName;
        if (n[0] == L'.' && (n[1] == 0 || (n[1] == L'.' && n[2] == 0))) continue;
        if (!job.hidden && (fd.dwFileAttributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM))) continue;
        bool isDir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        if (isDir) {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) continue;   // no junction loops
            bool skip = false;
            for (auto& sk : job.skip) if (!_wcsicmp(n, sk.c_str())) { skip = true; break; }
            if (skip) continue;
        }
        wstring full = dir + L"\\" + n;
        pool.off.push_back((uint32_t)pool.buf.size());
        pool.buf += full;
        if (isDir) pool.buf += L'\\';
        pool.buf += L'\n';
        if (isDir && depth < job.depth) ScanFolder(full, depth + 1, job, pool);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

static DWORD WINAPI FilesThread(LPVOID param) {
    ScanJob* job = (ScanJob*)param;
    SetThreadPriority(GetCurrentThread(), THREAD_MODE_BACKGROUND_BEGIN);
    auto* pool = new FilePool();
    for (auto& f : job->folders) {
        if ((int)pool->off.size() >= job->max) break;
        ScanFolder(f, 1, *job, *pool);
    }
    WriteFileUtf8(g_filesPath, job->sig + L"\n" + pool->buf);
    delete job;
    PostMessageW(g_hwnd, WM_FILES_READY, 0, (LPARAM)pool);
    InterlockedExchange(&g_filesBusy, 0);
    return 0;
}
static void StartFilesThread() {
    if (InterlockedCompareExchange(&g_filesBusy, 1, 0) != 0) return;
    auto* job = new ScanJob();
    job->folders = EffectiveFolders();
    job->depth = g_fileDepth > 0 ? g_fileDepth : 64;
    job->max = g_fileMax;
    for (auto& p : Split(g_fileSkip, L'|')) { wstring t = Trim(p); if (!t.empty()) job->skip.push_back(t); }
    job->hidden = g_fileHidden;
    job->sig = FolderSig();
    HANDLE h = CreateThread(nullptr, 0, FilesThread, job, 0, nullptr);
    if (h) CloseHandle(h);
    else { delete job; InterlockedExchange(&g_filesBusy, 0); }
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

static inline wchar_t LC(wchar_t c) { return c < 128 ? (c >= L'A' && c <= L'Z' ? (wchar_t)(c + 32) : c) : (wchar_t)towlower(c); }
static bool CIContains(const wchar_t* h, size_t hn, const wstring& nd) {
    size_t m = nd.size();
    if (!m) return true;
    if (hn < m) return false;
    wchar_t f = nd[0];
    for (size_t i = 0; i + m <= hn; i++) {
        if (LC(h[i]) != f) continue;
        size_t k = 1;
        while (k < m && LC(h[i + k]) == nd[k]) k++;
        if (k == m) return true;
    }
    return false;
}

static vector<Item> FileQuery(const wstring& text) {
    EnsureFiles();
    vector<Item> res;
    wstring ql = lower(text);
    vector<wstring> toks;
    for (auto& t : Split(ql, L' ')) if (!t.empty()) toks.push_back(t);
    auto hint = [&](const wstring& title, const wstring& sub) {
        Item it; it.cat = C_FILE; it.group = L"Files"; it.glyph = 0xE721; it.title = title; it.sub = sub; it.kind = K_NONE;
        res.push_back(it);
    };
    if (toks.empty()) {
        hint(L"Search your files", g_files.off.empty() ? L"Index is empty or still building \u00B7 set folder= lines in config.ini"
                                                        : std::to_wstring(g_files.off.size()) + L" items indexed \u00B7 type part of a name");
        return res;
    }
    struct Sc { uint32_t idx; int s; };
    vector<Sc> c;
    for (size_t i = 0; i < g_files.off.size(); i++) {
        std::wstring_view pv = g_files.at(i);
        bool pass = true;
        for (auto& t : toks) if (!CIContains(pv.data(), pv.size(), t)) { pass = false; break; }
        if (!pass) continue;
        size_t end = pv.size();
        if (end && pv[end - 1] == L'\\') end--;
        size_t sl = end ? pv.rfind(L'\\', end - 1) : std::wstring_view::npos;
        size_t st = (sl == std::wstring_view::npos) ? 0 : sl + 1;
        wstring name = lower(wstring(pv.substr(st, end - st)));
        int sum = 0;
        for (auto& t : toks) { int x = ScoreOne(t, name, L""); sum += x ? x : 15; }
        c.push_back({ (uint32_t)i, sum / (int)toks.size() });
    }
    size_t n = std::min<size_t>(c.size(), 12);
    std::partial_sort(c.begin(), c.begin() + n, c.end(), [](const Sc& a, const Sc& b) {
        if (a.s != b.s) return a.s > b.s;
        return g_files.at(a.idx).size() < g_files.at(b.idx).size();
    });
    for (size_t i = 0; i < n; i++) {
        wstring p(g_files.at(c[i].idx));
        bool dir = !p.empty() && p.back() == L'\\';
        size_t end = dir ? p.size() - 1 : p.size();
        size_t sl = end ? p.rfind(L'\\', end - 1) : wstring::npos;
        size_t st = (sl == wstring::npos) ? 0 : sl + 1;
        Item it; it.cat = C_FILE; it.group = L"Files"; it.glyph = dir ? 0xE8B7 : 0xE8A5;
        it.title = p.substr(st, end - st);
        it.sub = (sl == wstring::npos) ? p : p.substr(0, sl);
        it.kind = K_OPEN; it.a = p; it.alt = p;
        res.push_back(it);
    }
    if (res.empty()) hint(L"No files found", L"Only the folders listed in config.ini are indexed (default: Desktop, Documents, Downloads)");
    return res;
}

struct UnitDef { const wchar_t* names; int dim; double f; };
static const UnitDef kUnits[] = {
    { L"mm|millimeter|millimeters", 0, 0.001 }, { L"cm|centimeter|centimeters", 0, 0.01 }, { L"m|meter|meters|metre|metres", 0, 1 },
    { L"km|kilometer|kilometers", 0, 1000 }, { L"in|inch|inches", 0, 0.0254 }, { L"ft|foot|feet", 0, 0.3048 },
    { L"yd|yard|yards", 0, 0.9144 }, { L"mi|mile|miles", 0, 1609.344 },
    { L"mg", 1, 1e-6 }, { L"g|gram|grams", 1, 0.001 }, { L"kg|kilogram|kilograms|kgs", 1, 1 },
    { L"lb|lbs|pound|pounds", 1, 0.45359237 }, { L"oz|ounce|ounces", 1, 0.028349523125 }, { L"t|tonne|tonnes", 1, 1000 },
    { L"ml|milliliter|milliliters", 2, 0.001 }, { L"l|liter|liters|litre|litres", 2, 1 }, { L"gal|gallon|gallons", 2, 3.785411784 },
    { L"qt|quart|quarts", 2, 0.946352946 }, { L"cup|cups", 2, 0.2365882365 }, { L"floz", 2, 0.0295735295625 },
    { L"b|byte|bytes", 3, 1 }, { L"kb", 3, 1024 }, { L"mb", 3, 1048576 }, { L"gb", 3, 1073741824.0 }, { L"tb", 3, 1099511627776.0 },
    { L"s|sec|second|seconds", 4, 1 }, { L"min|minute|minutes", 4, 60 }, { L"h|hr|hour|hours", 4, 3600 },
    { L"d|day|days", 4, 86400 }, { L"wk|week|weeks", 4, 604800 },
    { L"mps", 5, 1 }, { L"kmh|kph|kmph", 5, 1 / 3.6 }, { L"mph", 5, 0.44704 }, { L"kn|knot|knots", 5, 0.514444 },
    { L"sqm|m2", 6, 1 }, { L"sqkm|km2", 6, 1e6 }, { L"sqft|ft2", 6, 0.09290304 }, { L"acre|acres", 6, 4046.8564224 }, { L"ha|hectare|hectares", 6, 10000 },
    { L"c|celsius", 7, 0 }, { L"f|fahrenheit", 7, 0 }, { L"k|kelvin", 7, 0 },
};
static const UnitDef* FindUnit(const wstring& n) {
    for (auto& u : kUnits)
        for (auto& a : Split(u.names, L'|')) if (a == n) return &u;
    return nullptr;
}
static bool TryConvert(const wstring& ql, wstring& out) {
    const wchar_t* p = ql.c_str();
    wchar_t* e = nullptr;
    double v = wcstod(p, &e);
    if (e == p || !std::isfinite(v)) return false;
    while (*e == L' ') e++;
    wstring rest = e;
    size_t sp = rest.find(L' ');
    if (sp == wstring::npos) return false;
    wstring u1 = rest.substr(0, sp);
    rest = Trim(rest.substr(sp + 1));
    if (rest.rfind(L"to ", 0) == 0 || rest.rfind(L"in ", 0) == 0) rest = Trim(rest.substr(3)); else return false;
    const UnitDef* a = FindUnit(u1); const UnitDef* b = FindUnit(rest);
    if (!a || !b || a->dim != b->dim) return false;
    double r;
    if (a->dim == 7) {
        wchar_t fa = a->names[0], fb = b->names[0];
        double c = (fa == L'c') ? v : (fa == L'f') ? (v - 32) * 5 / 9 : v - 273.15;
        r = (fb == L'c') ? c : (fb == L'f') ? c * 9 / 5 + 32 : c + 273.15;
    } else r = v * a->f / b->f;
    wchar_t buf[64]; swprintf(buf, 64, L"%.6g", r);
    out = wstring(buf) + L" " + rest;
    return true;
}

static bool TryInstant(const wstring& ql, wstring& title, wstring& sub, wstring& copy) {
    wchar_t b[128];
    if (ql == L"date" || ql == L"today") {
        GetDateFormatW(LOCALE_USER_DEFAULT, DATE_LONGDATE, nullptr, nullptr, b, 128);
        title = copy = b; sub = L"Today \u00B7 Enter copies"; return true;
    }
    if (ql == L"time" || ql == L"now") {
        GetTimeFormatW(LOCALE_USER_DEFAULT, TIME_NOSECONDS, nullptr, nullptr, b, 128);
        title = copy = b; sub = L"Current time \u00B7 Enter copies"; return true;
    }
    if (ql == L"unix" || ql == L"timestamp") {
        FILETIME ft; GetSystemTimeAsFileTime(&ft);
        ULONGLONG t = (((ULONGLONG)ft.dwHighDateTime << 32) | ft.dwLowDateTime);
        title = copy = std::to_wstring((t - 116444736000000000ULL) / 10000000ULL);
        sub = L"Unix timestamp \u00B7 Enter copies"; return true;
    }
    if (ql == L"guid" || ql == L"uuid") {
        GUID g; CoCreateGuid(&g);
        StringFromGUID2(g, b, 64);
        wstring s = lower(b);
        title = copy = s.substr(1, s.size() - 2);
        sub = L"New random GUID \u00B7 Enter copies"; return true;
    }
    return false;
}
static Item QuickItem(const wstring& title, const wstring& sub, const wstring& copy) {
    Item it; it.cat = C_CALC; it.group = L"Quick answers"; it.glyph = DefaultGlyph(C_CALC);
    it.title = title; it.sub = sub; it.kind = K_COPY; it.a = copy;
    return it;
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
        if (!g_showRecent) return res;
        vector<const Item*> f;
        for (auto& it : g_catalog) if (it.track && !g_hidden.count(it.id) && g_usage.count(it.id)) f.push_back(&it);
        std::sort(f.begin(), f.end(), [](const Item* x, const Item* y) { return g_usage[x->id] > g_usage[y->id]; });
        for (size_t i = 0; i < f.size() && i < (size_t)g_recentCount; i++) { Item c = *f[i]; c.group = L"Frequently used"; res.push_back(c); }
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

    // file search: "f report"
    if (sp != wstring::npos && sp > 0 && _wcsicmp(r2.substr(0, sp).c_str(), g_fileTrigger.c_str()) == 0)
        return FileQuery(Trim(r2.substr(sp + 1)));

    // run box: "> notepad C:\\file.txt"
    if (q[0] == L'>') {
        wstring cl = Trim(q.substr(1)), file, args;
        if (!cl.empty()) {
            if (cl[0] == L'"') {
                size_t e = cl.find(L'"', 1);
                if (e == wstring::npos) { file = cl.substr(1); } else { file = cl.substr(1, e - 1); args = Trim(cl.substr(e + 1)); }
            } else {
                size_t s2 = cl.find(L' ');
                if (s2 == wstring::npos) file = cl; else { file = cl.substr(0, s2); args = Trim(cl.substr(s2 + 1)); }
            }
            Item it; it.cat = C_OPEN; it.group = L"Run"; it.glyph = 0xE756;
            it.title = L"Run: " + cl; it.sub = L"Like the Windows Run box"; it.kind = K_OPEN; it.a = file; it.b = args;
            res.push_back(it);
            return res;
        }
    }

    {   // quick answers: dates, GUIDs, unit conversion
        wstring ql0 = lower(q), t, sb, cp;
        if (TryInstant(ql0, t, sb, cp)) res.push_back(QuickItem(t, sb, cp));
        if (TryConvert(ql0, cp)) res.push_back(QuickItem(L"= " + cp, L"Unit conversion \u00B7 Enter copies", cp));
    }

    for (auto& nd : g_notes)   // typed exactly the note trigger: offer it first
        if (_wcsicmp(nd.trig.c_str(), q.c_str()) == 0) {
            Item it; it.cat = C_CMD; it.group = L"Notes"; it.glyph = 0xE70F;
            it.title = L"Open note: " + nd.name; it.sub = L"Tip: type  " + nd.trig + L" + space  to jump straight in";
            it.kind = K_NOTE; it.a = nd.trig;
            res.push_back(it);
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
        size_t lim = (c == C_APP) ? (size_t)g_appsLimit : (size_t)g_otherLimit;
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
static void OpenFile(const wstring& path) {   // opens with whatever app Windows has for it (Notepad for .ini/.txt)
    ShellExecuteW(nullptr, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}
static void Reveal(const wstring& path) {   // "show in folder"
    PIDLIST_ABSOLUTE pidl = nullptr;
    if (SUCCEEDED(SHParseDisplayName(path.c_str(), nullptr, &pidl, 0, nullptr)) && pidl) {
        SHOpenFolderAndSelectItems(pidl, 0, nullptr, 0);
        CoTaskMemFree(pidl);
    }
}
static void ExitWin(UINT flags) {
    if (flags & (EWX_POWEROFF | EWX_REBOOT)) {
        HANDLE tok = nullptr;
        if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &tok)) {
            TOKEN_PRIVILEGES tp = {};
            tp.PrivilegeCount = 1;
            tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
            if (LookupPrivilegeValueW(nullptr, SE_SHUTDOWN_NAME, &tp.Privileges[0].Luid))
                AdjustTokenPrivileges(tok, FALSE, &tp, 0, nullptr, nullptr);
            CloseHandle(tok);
        }
    }
    ExitWindowsEx(flags, SHTDN_REASON_MAJOR_OTHER);
}
static void Balloon(const wchar_t* title, const wchar_t* msg, DWORD icon = NIIF_INFO);

static void Exec(const Item& it) {
    if (it.confirm && MessageBoxW(nullptr, (it.title + L"?").c_str(), L"Quick Palette", MB_YESNO | MB_ICONQUESTION | MB_TOPMOST | MB_SETFOREGROUND) != IDYES) return;
    switch (it.kind) {
        case K_OPEN:
            ShellExecuteW(nullptr, L"open", ExpandEnv(it.a).c_str(), it.b.empty() ? nullptr : ExpandEnv(it.b).c_str(), nullptr, SW_SHOWNORMAL);
            break;
        case K_APP:
            if (it.a.rfind(L"lnk:", 0) == 0) ShellExecuteW(nullptr, L"open", it.a.c_str() + 4, nullptr, nullptr, SW_SHOWNORMAL);
            else ShellExecuteW(nullptr, L"open", (L"shell:AppsFolder\\" + it.a).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            break;
        case K_LOCK: LockWorkStation(); break;
        case K_SLEEP: SetSuspendState(FALSE, FALSE, FALSE); break;
        case K_SHUTDOWN: ExitWin(EWX_POWEROFF); break;
        case K_RESTART: ExitWin(EWX_REBOOT); break;
        case K_LOGOFF: ExitWin(EWX_LOGOFF); break;
        case K_RECYCLE: SHEmptyRecycleBinW(nullptr, nullptr, SHERB_NOCONFIRMATION | SHERB_NOPROGRESSUI | SHERB_NOSOUND); break;
        case K_QUIT: DestroyWindow(g_hwnd); break;
        case K_EDITCFG: OpenFile(g_cfgPath); break;
        case K_RELOAD: StartAppsThread(); break;
        case K_REINDEX: StartFilesThread(); break;
        case K_UNHIDE: g_hidden.clear(); DeleteFileW(g_hiddenPath.c_str()); break;
        case K_EDITHIDDEN:
            if (!PathFileExistsW(g_hiddenPath.c_str())) WriteText(g_hiddenPath, L"");
            OpenFile(g_hiddenPath);
            break;
        case K_COPY: SetClipboard(it.a); break;
        case K_CFGDIR: OpenFile(g_dir); break;
        case K_STARTUP_ON: SetConfigValue(L"startup", L"1"); Balloon(L"Quick Palette", L"Will start with Windows (delayed, low priority)."); break;
        case K_STARTUP_OFF: SetConfigValue(L"startup", L"0"); Balloon(L"Quick Palette", L"Will no longer start with Windows."); break;
        default: break;
    }
}

#include "md.h"
#include "ui.h"
