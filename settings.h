// settings.h - the settings window. It only edits values in config.ini (the file stays the source of truth;
// you can still edit it by hand, and the window always shows what is in the file).
#pragma once
#include <commctrl.h>
#pragma comment(lib, "comctl32.lib")

static const wchar_t* kSetClass = L"QuickPaletteSettings";
static const wchar_t* kPageClass = L"QuickPaletteSettingsPage";
static HWND g_sw = nullptr, g_swTab = nullptr, g_swPage[6] = {};
static HFONT g_fCtl = nullptr;
static WNDPROC g_oldHot = nullptr;

// ------------------------------------------------------------------ reading / writing config.ini
static vector<wstring> CfgGetAll(const wstring& key) {
    vector<wstring> out, ln;
    ReadLines(g_cfgPath, ln);
    for (auto& l : ln) {
        wstring t = Trim(l);
        size_t eq = t.find(L'=');
        if (t.empty() || t[0] == L'#' || eq == wstring::npos) continue;
        if (lower(Trim(t.substr(0, eq))) == key) out.push_back(Trim(t.substr(eq + 1)));
    }
    return out;
}
static wstring CfgGet(const wstring& key, const wstring& def) {
    auto v = CfgGetAll(key);
    return v.empty() ? def : v.back();
}
static void CfgWrite(const vector<std::pair<wstring, vector<wstring>>>& changes) {   // replaces ALL lines of each key; other lines and comments stay
    vector<wstring> ln;
    ReadLines(g_cfgPath, ln);
    for (auto& ch : changes) {
        vector<wstring> res;
        size_t at = wstring::npos;
        for (auto& l : ln) {
            wstring t = Trim(l);
            size_t eq = t.find(L'=');
            bool isKey = !t.empty() && t[0] != L'#' && eq != wstring::npos && lower(Trim(t.substr(0, eq))) == ch.first;
            if (isKey) { if (at == wstring::npos) at = res.size(); continue; }
            res.push_back(l);
        }
        if (at == wstring::npos) at = res.size();
        vector<wstring> add;
        for (auto& v : ch.second) add.push_back(ch.first + L"=" + v);
        res.insert(res.begin() + (std::ptrdiff_t)at, add.begin(), add.end());
        ln.swap(res);
    }
    wstring out;
    for (auto& l : ln) out += l + L"\n";
    WriteText(g_cfgPath, out);
}

// ------------------------------------------------------------------ the fields
enum { FT_CHECK, FT_EDIT, FT_COMBO, FT_LIST, FT_HOTKEY };
struct SField { int tab; int type; const wchar_t* key; const wchar_t* label; const wchar_t* def; const wchar_t* presets; int lines; HWND hw; };
static SField g_sf[] = {
    // 0 General
    { 0, FT_HOTKEY, L"hotkey", L"Shortcut that opens the palette (click, then press the keys)", L"Ctrl+Space", nullptr, 0, nullptr },
    { 0, FT_CHECK, L"startup", L"Start with Windows (waits about 45 s, low priority)", L"0", nullptr, 0, nullptr },
    { 0, FT_CHECK, L"hide_on_focus_loss", L"Close the palette when I click somewhere else", L"1", nullptr, 0, nullptr },
    { 0, FT_CHECK, L"at_cursor", L"Open at the mouse pointer (instead of the screen center)", L"0", nullptr, 0, nullptr },
    { 0, FT_CHECK, L"animations", L"Animations (quick fade, smooth highlight)", L"1", nullptr, 0, nullptr },
    { 0, FT_CHECK, L"show_recent", L"Show pinned and most-used items when the box is empty", L"1", nullptr, 0, nullptr },
    { 0, FT_EDIT, L"recent_count", L"How many most-used items to show", L"6", nullptr, 0, nullptr },
    { 0, FT_EDIT, L"apps_limit", L"Most app results shown", L"6", nullptr, 0, nullptr },
    { 0, FT_EDIT, L"other_limit", L"Most results shown per other group", L"4", nullptr, 0, nullptr },
    // 1 Appearance
    { 1, FT_COMBO, L"theme", L"Theme", L"auto", L"auto|dark|light", 0, nullptr },
    { 1, FT_CHECK, L"compact", L"Compact mode (smaller, denser rows)", L"0", nullptr, 0, nullptr },
    { 1, FT_CHECK, L"show_footer", L"Show the key hints under the list", L"1", nullptr, 0, nullptr },
    { 1, FT_EDIT, L"width", L"Palette width (pixels at 100% scale)", L"640", nullptr, 0, nullptr },
    { 1, FT_EDIT, L"list_height", L"Longest the result list can get (pixels)", L"430", nullptr, 0, nullptr },
    // 2 Files
    { 2, FT_LIST, L"folder", L"Folders to search for files (one per line). Everything inside is indexed. {desktop} {documents} {downloads} {pictures} {music} {videos} work too.", L"", nullptr, 5, nullptr },
    { 2, FT_CHECK, L"folder_none", L"Index no folders when the list above is empty", L"0", nullptr, 0, nullptr },
    { 2, FT_EDIT, L"file_trigger", L"Word that starts a file search", L"f", nullptr, 0, nullptr },
    { 2, FT_EDIT, L"file_max", L"Most files to index", L"100000", nullptr, 0, nullptr },
    { 2, FT_EDIT, L"file_depth", L"Sub-folder depth (0 = all levels)", L"0", nullptr, 0, nullptr },
    { 2, FT_EDIT, L"file_skip", L"Folder names to skip (separate with |)", L"node_modules|.git|$RECYCLE.BIN|System Volume Information", nullptr, 0, nullptr },
    { 2, FT_CHECK, L"file_include_hidden", L"Include hidden and system files", L"0", nullptr, 0, nullptr },
    // 3 Bangs & commands
    { 3, FT_EDIT, L"default_bang", L"Word of the search used by the 'Search the web' row", L"g", nullptr, 0, nullptr },
    { 3, FT_LIST, L"bang", L"Web searches: word|Name|address with {q}   e.g.  yt|YouTube|https://www.youtube.com/results?search_query={q}", L"", nullptr, 6, nullptr },
    { 3, FT_LIST, L"cmd", L"Your commands: Name|extra words|program, file, folder or address|arguments", L"", nullptr, 4, nullptr },
    { 3, FT_LIST, L"snippet", L"Snippets (Enter copies the text): Name|text", L"", nullptr, 3, nullptr },
    { 3, FT_LIST, L"alias", L"Aliases: word|Exact item name   e.g.  ps|PowerShell", L"", nullptr, 3, nullptr },
    // 4 Notes
    { 4, FT_LIST, L"note", L"Notes: word|Name|file path   ({date} in the path makes a daily note)", L"", nullptr, 5, nullptr },
    { 4, FT_EDIT, L"note_font", L"Note font", L"Segoe UI", nullptr, 0, nullptr },
    { 4, FT_EDIT, L"note_font_size", L"Note font size", L"15", nullptr, 0, nullptr },
    { 4, FT_EDIT, L"note_width", L"Note window width", L"1000", nullptr, 0, nullptr },
    { 4, FT_EDIT, L"note_height", L"Note window height", L"680", nullptr, 0, nullptr },
    // 5 Browser
    { 5, FT_EDIT, L"web_trigger", L"Word that starts a browser search", L"w", nullptr, 0, nullptr },
    { 5, FT_COMBO, L"web_search", L"Search engine for words typed in the browser",
      L"https://www.google.com/search?q={q}", L"https://www.google.com/search?q={q}|https://duckduckgo.com/?q={q}|https://www.bing.com/search?q={q}|https://search.brave.com/search?q={q}", 0, nullptr },
    { 5, FT_COMBO, L"url_open", L"A typed web address opens in (builtin = built-in browser, system = default browser)", L"builtin", L"builtin|system", 0, nullptr },
    { 5, FT_CHECK, L"web_https_only", L"HTTPS only (plain http is upgraded)", L"1", nullptr, 0, nullptr },
    { 5, FT_CHECK, L"web_block_downloads", L"Block downloads in the built-in browser", L"1", nullptr, 0, nullptr },
    { 5, FT_CHECK, L"web_private", L"Private: nothing is kept after the browser closes", L"1", nullptr, 0, nullptr },
    { 5, FT_CHECK, L"web_lowram", L"Low-RAM mode (fewer processes, weaker isolation)", L"0", nullptr, 0, nullptr },
    { 5, FT_EDIT, L"web_keepalive", L"Seconds the browser stays warm after closing (faster reopening)", L"60", nullptr, 0, nullptr },
    { 5, FT_EDIT, L"web_width", L"Browser window width", L"1100", nullptr, 0, nullptr },
    { 5, FT_EDIT, L"web_height", L"Browser window height", L"720", nullptr, 0, nullptr },
    { 5, FT_LIST, L"web_bookmark", L"Bookmarks shown after 'w ': Name|address", L"", nullptr, 4, nullptr },
    { 5, FT_LIST, L"web_block", L"Extra domains to block", L"", nullptr, 2, nullptr },
    { 5, FT_LIST, L"web_extension", L"Extension folders (for example an unzipped uBlock Origin Lite)", L"", nullptr, 2, nullptr },
};
static const int kSfN = (int)(sizeof(g_sf) / sizeof(g_sf[0]));
static const wchar_t* kTabNames[6] = { L"General", L"Appearance", L"Files", L"Searches and commands", L"Notes", L"Browser" };
static const wchar_t* kQfIds[] = { L"docs", L"downloads", L"desktop", L"pictures", L"music", L"videos", L"recyclebin", L"thispc", L"startupfolder", L"appdata", L"temp" };
static const wchar_t* kQfNames[] = { L"Documents", L"Downloads", L"Desktop", L"Pictures", L"Music", L"Videos", L"Recycle Bin", L"This PC", L"Startup folder", L"AppData", L"Temp" };
static const int kQfN = 11;
static HWND g_qfBox[11] = {};
enum { ID_SAVE = 1, ID_CANCEL = 2, ID_OPENCFG = 3, ID_ADDFOLDER = 4 };

// ------------------------------------------------------------------ hotkey capture box
static wstring KeyName(UINT k) {
    if (k >= 'A' && k <= 'Z') return wstring(1, (wchar_t)k);
    if (k >= '0' && k <= '9') return wstring(1, (wchar_t)k);
    if (k >= VK_F1 && k <= VK_F24) return L"F" + std::to_wstring(k - VK_F1 + 1);
    switch (k) {
        case VK_SPACE: return L"Space";
        case VK_RETURN: return L"Enter";
        case VK_TAB: return L"Tab";
        case VK_ESCAPE: return L"Esc";
        case VK_BACK: return L"Backspace";
    }
    return L"";
}
static LRESULT CALLBACK HotkeyProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_KEYDOWN || m == WM_SYSKEYDOWN) {
        UINT k = (UINT)w;
        if (k == VK_CONTROL || k == VK_MENU || k == VK_SHIFT || k == VK_LWIN || k == VK_RWIN) return 0;
        bool ctrl = GetKeyState(VK_CONTROL) < 0, alt = GetKeyState(VK_MENU) < 0, shift = GetKeyState(VK_SHIFT) < 0;
        bool win = GetKeyState(VK_LWIN) < 0 || GetKeyState(VK_RWIN) < 0;
        wstring name = KeyName(k);
        if ((!ctrl && !alt && !shift && !win) || name.empty()) return 0;   // needs a modifier key
        wstring s;
        if (ctrl) s += L"Ctrl+";
        if (alt) s += L"Alt+";
        if (shift) s += L"Shift+";
        if (win) s += L"Win+";
        SetWindowTextW(h, (s + name).c_str());
        return 0;
    }
    if (m == WM_CHAR || m == WM_SYSCHAR) return 0;
    return CallWindowProcW(g_oldHot, h, m, w, l);
}

// ------------------------------------------------------------------ loading / saving the values
static void SettingsLoad() {
    for (auto& f : g_sf) {
        if (!f.hw) continue;
        wstring key = f.key;
        switch (f.type) {
            case FT_CHECK: SendMessageW(f.hw, BM_SETCHECK, CfgGet(key, f.def) == L"1" ? BST_CHECKED : BST_UNCHECKED, 0); break;
            case FT_EDIT: case FT_HOTKEY: SetWindowTextW(f.hw, CfgGet(key, f.def).c_str()); break;
            case FT_COMBO: {
                wstring v = CfgGet(key, f.def);
                if (key == L"web_search") SetWindowTextW(f.hw, v.c_str());
                else {
                    LRESULT i = SendMessageW(f.hw, CB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)v.c_str());
                    if (i == CB_ERR) i = SendMessageW(f.hw, CB_ADDSTRING, 0, (LPARAM)v.c_str());
                    SendMessageW(f.hw, CB_SETCURSEL, (WPARAM)i, 0);
                }
                break;
            }
            case FT_LIST: {
                auto vals = CfgGetAll(key);
                if (key == L"folder" && vals.empty() && CfgGet(L"folder_none", L"0") != L"1")
                    vals = { L"{desktop}", L"{documents}", L"{downloads}" };   // what is used when the file has no folder lines
                wstring t;
                for (auto& v : vals) t += v + L"\r\n";
                SetWindowTextW(f.hw, t.c_str());
                break;
            }
        }
    }
    auto qf = CfgGetAll(L"quick_folders");
    std::unordered_set<wstring> on;
    bool all = qf.empty();
    if (!all) for (auto& id : Split(lower(qf.back()), L',')) on.insert(Trim(id));
    for (int i = 0; i < kQfN; i++) SendMessageW(g_qfBox[i], BM_SETCHECK, (all || on.count(kQfIds[i])) ? BST_CHECKED : BST_UNCHECKED, 0);
}

static void SettingsSave() {
    vector<std::pair<wstring, vector<wstring>>> ch;
    for (auto& f : g_sf) {
        if (!f.hw) continue;
        vector<wstring> vals;
        switch (f.type) {
            case FT_CHECK: vals.push_back(SendMessageW(f.hw, BM_GETCHECK, 0, 0) == BST_CHECKED ? L"1" : L"0"); break;
            case FT_EDIT: case FT_HOTKEY: case FT_COMBO: vals.push_back(Trim(GetEditText(f.hw))); break;
            case FT_LIST:
                for (auto& l : Split(GetEditText(f.hw), L'\n')) { wstring t = Trim(l); if (!t.empty()) vals.push_back(t); }
                break;
        }
        ch.push_back({ wstring(f.key), vals });
    }
    wstring qf;
    for (int i = 0; i < kQfN; i++)
        if (SendMessageW(g_qfBox[i], BM_GETCHECK, 0, 0) == BST_CHECKED) { if (!qf.empty()) qf += L","; qf += kQfIds[i]; }
    ch.push_back({ wstring(L"quick_folders"), vector<wstring>{ qf } });
    CfgWrite(ch);

    LoadConfig(); SyncStartup(); RegisterHotkey();
    g_catalogDirty = true; g_blockInit = false; g_lastCfgCheck = 0;
    FreeFiles(); StartFilesThread();
}

static void AddFolderDialog() {
    BROWSEINFOW bi = {};
    bi.hwndOwner = g_sw;
    bi.lpszTitle = L"Choose a folder to search for files";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&bi);
    if (!pidl) return;
    wchar_t path[MAX_PATH] = {};
    if (SHGetPathFromIDListW(pidl, path)) {
        for (auto& f : g_sf) {
            if (!f.hw || wcscmp(f.key, L"folder") != 0) continue;
            wstring t = GetEditText(f.hw);
            if (!t.empty() && t.back() != L'\n') t += L"\r\n";
            SetWindowTextW(f.hw, (t + path + L"\r\n").c_str());
        }
    }
    CoTaskMemFree(pidl);
}

// ------------------------------------------------------------------ window
static void ShowPage(int i) {
    for (int k = 0; k < 6; k++) if (g_swPage[k]) ShowWindow(g_swPage[k], k == i ? SW_SHOW : SW_HIDE);
}

static HWND MakeCtl(HWND parent, const wchar_t* cls, const wchar_t* text, DWORD style, DWORD ex, int x, int y, int w, int h, int id) {
    HWND c = CreateWindowExW(ex, cls, text, WS_CHILD | WS_VISIBLE | style, x, y, w, h, parent, (HMENU)(INT_PTR)id, g_inst, nullptr);
    if (c && g_fCtl) SendMessageW(c, WM_SETFONT, (WPARAM)g_fCtl, TRUE);
    return c;
}

static void BuildPage(int tab, HWND page, int pw) {
    int y = px(12);
    int lx = px(14), ex = px(330), ew = pw - px(330) - px(16);
    for (int fi = 0; fi < kSfN; fi++) {
        SField& f = g_sf[fi];
        if (f.tab != tab) continue;
        int id = 2000 + fi;
        switch (f.type) {
            case FT_CHECK:
                f.hw = MakeCtl(page, L"BUTTON", f.label, WS_TABSTOP | BS_AUTOCHECKBOX, 0, lx, y, pw - px(28), px(24), id);
                y += px(30);
                break;
            case FT_EDIT: case FT_HOTKEY: {
                MakeCtl(page, L"STATIC", f.label, SS_LEFT | SS_NOPREFIX, 0, lx, y + px(3), px(310), px(40), 0);
                f.hw = MakeCtl(page, L"EDIT", L"", WS_TABSTOP | ES_AUTOHSCROLL | (f.type == FT_HOTKEY ? ES_READONLY : 0), WS_EX_CLIENTEDGE, ex, y, ew, px(26), id);
                if (f.type == FT_HOTKEY) g_oldHot = (WNDPROC)SetWindowLongPtrW(f.hw, GWLP_WNDPROC, (LONG_PTR)HotkeyProc);
                y += px(38);
                break;
            }
            case FT_COMBO: {
                MakeCtl(page, L"STATIC", f.label, SS_LEFT | SS_NOPREFIX, 0, lx, y + px(3), px(310), px(40), 0);
                bool editable = wcscmp(f.key, L"web_search") == 0;
                f.hw = MakeCtl(page, L"COMBOBOX", L"", WS_TABSTOP | WS_VSCROLL | (editable ? CBS_DROPDOWN | CBS_AUTOHSCROLL : CBS_DROPDOWNLIST), 0, ex, y, ew, px(220), id);
                for (auto& p : Split(f.presets, L'|')) SendMessageW(f.hw, CB_ADDSTRING, 0, (LPARAM)p.c_str());
                y += px(38);
                break;
            }
            case FT_LIST: {
                int lh = px(20) * f.lines + px(10);
                MakeCtl(page, L"STATIC", f.label, SS_LEFT | SS_NOPREFIX, 0, lx, y, pw - px(28), px(40), 0);
                y += px(40);
                f.hw = MakeCtl(page, L"EDIT", L"", WS_TABSTOP | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN, WS_EX_CLIENTEDGE, lx, y, pw - px(28), lh, id);
                y += lh + px(6);
                if (wcscmp(f.key, L"folder") == 0) { MakeCtl(page, L"BUTTON", L"Add folder...", WS_TABSTOP | BS_PUSHBUTTON, 0, lx, y, px(130), px(28), ID_ADDFOLDER); y += px(36); }
                else y += px(6);
                break;
            }
        }
    }
    if (tab == 2) {   // quick folders
        y += px(4);
        MakeCtl(page, L"STATIC", L"Quick-folder commands (open Documents, Downloads, ...). Untick the ones you do not want:", SS_LEFT | SS_NOPREFIX, 0, lx, y, pw - px(28), px(22), 0);
        y += px(26);
        int colW = (pw - px(28)) / 3;
        for (int i = 0; i < kQfN; i++)
            g_qfBox[i] = MakeCtl(page, L"BUTTON", kQfNames[i], WS_TABSTOP | BS_AUTOCHECKBOX, 0, lx + (i % 3) * colW, y + (i / 3) * px(26), colW - px(8), px(24), 3000 + i);
    }
}

static LRESULT CALLBACK PageProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
        case WM_ERASEBKGND: { RECT rc; GetClientRect(h, &rc); FillRect((HDC)w, &rc, GetSysColorBrush(COLOR_WINDOW)); return 1; }
        case WM_COMMAND: return SendMessageW(g_sw, m, w, l);
        case WM_CTLCOLORSTATIC: { HDC dc = (HDC)w; SetBkColor(dc, GetSysColor(COLOR_WINDOW)); return (LRESULT)GetSysColorBrush(COLOR_WINDOW); }
    }
    return DefWindowProcW(h, m, w, l);
}

static LRESULT CALLBACK SettingsProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
        case WM_NOTIFY: {
            auto* n = (NMHDR*)l;
            if (n && n->hwndFrom == g_swTab && n->code == TCN_SELCHANGE) ShowPage(TabCtrl_GetCurSel(g_swTab));
            return 0;
        }
        case WM_COMMAND:
            switch (LOWORD(w)) {
                case ID_SAVE: SettingsSave(); DestroyWindow(h); break;
                case ID_CANCEL: DestroyWindow(h); break;
                case ID_OPENCFG: OpenFile(g_cfgPath); break;
                case ID_ADDFOLDER: AddFolderDialog(); break;
            }
            return 0;
        case WM_CLOSE: DestroyWindow(h); return 0;
        case WM_DESTROY:
            for (auto& f : g_sf) f.hw = nullptr;
            for (int i = 0; i < kQfN; i++) g_qfBox[i] = nullptr;
            for (int k = 0; k < 6; k++) g_swPage[k] = nullptr;
            g_swTab = nullptr; g_sw = nullptr;
            if (g_fCtl) { DeleteObject(g_fCtl); g_fCtl = nullptr; }
            return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

static void RegisterSettingsClasses(HINSTANCE inst) {
    WNDCLASSEXW c = { sizeof c };
    c.lpfnWndProc = SettingsProc; c.hInstance = inst; c.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    c.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    c.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(1));
    c.hIconSm = (HICON)LoadImageW(inst, MAKEINTRESOURCEW(1), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
    c.lpszClassName = kSetClass;
    RegisterClassExW(&c);
    WNDCLASSEXW p = { sizeof p };
    p.lpfnWndProc = PageProc; p.hInstance = inst; p.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    p.lpszClassName = kPageClass;
    RegisterClassExW(&p);
}

static void OpenSettingsWindow() {
    if (g_sw) {
        if (IsIconic(g_sw)) ShowWindow(g_sw, SW_RESTORE);
        SetForegroundWindow(g_sw);
        return;
    }
    INITCOMMONCONTROLSEX ic = { sizeof ic, ICC_TAB_CLASSES };
    InitCommonControlsEx(&ic);
    POINT pt; GetCursorPos(&pt);
    HMONITOR mon = MonitorFromPoint(pt, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO mi = { sizeof(mi) };
    GetMonitorInfoW(mon, &mi);
    UINT dx = 96, dy = 96;
    GetDpiForMonitor(mon, MDT_EFFECTIVE_DPI, &dx, &dy);
    if ((int)dx != g_dpi) SetDpi((int)dx, true);
    int ww = (int)(mi.rcWork.right - mi.rcWork.left), wh = (int)(mi.rcWork.bottom - mi.rcWork.top);
    int W = std::min<int>(px(860), ww - px(40)), H = std::min<int>(px(780), wh - px(40));
    g_fCtl = MakeFont(px(14), 400, g_uiFace);
    g_sw = CreateWindowExW(0, kSetClass, L"Quick Palette Settings", WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN,
                           (int)mi.rcWork.left + (ww - W) / 2, (int)mi.rcWork.top + (wh - H) / 2, W, H, nullptr, nullptr, g_inst, nullptr);
    if (!g_sw) return;
    RECT cr; GetClientRect(g_sw, &cr);
    int cw = cr.right, ch = cr.bottom;
    int bottom = px(54);

    g_swTab = CreateWindowExW(0, WC_TABCONTROLW, L"", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS, px(10), px(10), cw - px(20), ch - px(10) - bottom, g_sw, nullptr, g_inst, nullptr);
    SendMessageW(g_swTab, WM_SETFONT, (WPARAM)g_fCtl, TRUE);
    for (int i = 0; i < 6; i++) {
        TCITEMW ti = {}; ti.mask = TCIF_TEXT; ti.pszText = (LPWSTR)kTabNames[i];
        SendMessageW(g_swTab, TCM_INSERTITEMW, (WPARAM)i, (LPARAM)&ti);
    }
    RECT pr = { 0, 0, cw - px(20), ch - px(10) - bottom };
    TabCtrl_AdjustRect(g_swTab, FALSE, &pr);
    int px0 = px(10) + pr.left, py0 = px(10) + pr.top, pw = pr.right - pr.left, ph = pr.bottom - pr.top;
    for (int i = 0; i < 6; i++) {
        g_swPage[i] = CreateWindowExW(0, kPageClass, L"", WS_CHILD | WS_CLIPCHILDREN, px0, py0, pw, ph, g_sw, nullptr, g_inst, nullptr);
        BuildPage(i, g_swPage[i], pw);
    }
    MakeCtl(g_sw, L"BUTTON", L"Open config.ini", WS_TABSTOP | BS_PUSHBUTTON, 0, px(14), ch - px(42), px(150), px(30), ID_OPENCFG);
    MakeCtl(g_sw, L"BUTTON", L"Save", WS_TABSTOP | BS_DEFPUSHBUTTON, 0, cw - px(250), ch - px(42), px(110), px(30), ID_SAVE);
    MakeCtl(g_sw, L"BUTTON", L"Cancel", WS_TABSTOP | BS_PUSHBUTTON, 0, cw - px(130), ch - px(42), px(110), px(30), ID_CANCEL);
    SettingsLoad();
    ShowPage(0);
    ShowWindow(g_sw, SW_SHOW);
    SetForegroundWindow(g_sw);
}
