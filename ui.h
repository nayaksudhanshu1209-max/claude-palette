// ui.h - palette window, drawing, query box, note editor. Included once from main.cpp.
#pragma once

// ------------------------------------------------------------------ theme
struct Theme {
    bool dark;
    COLORREF bg, border, text, dim, hdr, placeholder, sel, hov, divider, footerBg, footerText, editorBg, editorText,
             codeBg, link, bar, rule, hl, tableHead, tableLine, thumb, chip, chipHot;
};
static const Theme kDark = { true, RGB(31,31,35), RGB(64,64,70), RGB(245,245,247), RGB(130,130,138), RGB(140,140,148), RGB(105,105,112),
    RGB(62,62,69), RGB(46,46,52), RGB(52,52,58), RGB(26,26,30), RGB(110,110,118), RGB(24,24,28), RGB(230,230,235),
    RGB(48,48,56), RGB(96,165,250), RGB(90,90,100), RGB(70,70,78), RGB(113,92,10), RGB(40,40,46), RGB(70,70,78),
    RGB(95,95,105), RGB(44,44,50), RGB(66,66,76) };
static const Theme kLight = { false, RGB(250,250,252), RGB(205,205,212), RGB(28,28,32), RGB(110,110,120), RGB(110,110,120), RGB(150,150,158),
    RGB(224,228,240), RGB(236,238,244), RGB(225,225,232), RGB(240,240,244), RGB(110,110,120), RGB(255,255,255), RGB(30,30,34),
    RGB(236,238,244), RGB(29,78,216), RGB(180,180,190), RGB(210,210,218), RGB(255,240,160), RGB(238,240,246), RGB(210,210,218),
    RGB(170,170,180), RGB(234,236,242), RGB(214,218,232) };
static const Theme* g_th = &kDark;

// ------------------------------------------------------------------ ui globals
static HWND g_hQuery = nullptr, g_hEditor = nullptr;
static WNDPROC g_oldQuery = nullptr, g_oldEditor = nullptr;
static HBRUSH g_brBg = nullptr, g_brEd = nullptr, g_brChip = nullptr;
static bool g_webMode = false, g_addrDirty = false, g_webPage = false;   // g_noteMode/g_webMode: legacy, always false (notes and browser are separate windows now)
static HWND g_nw = nullptr, g_ww = nullptr, g_hAddr = nullptr;           // note window, browser window, address bar
static WNDPROC g_oldAddr = nullptr;
static bool g_noteOpen = false, g_webOpen = false;
static HFONT g_fChip = nullptr, g_fNoteHead = nullptr, g_fNote = nullptr;
static bool g_win11 = false, g_noteMode = false, g_suppress = false, g_facesChecked = false, g_lastCompact = false;
static int g_lineH = 28, g_qLines = 1, g_addrH = 22;
static RECT g_work = { 0, 0, 1920, 1080 };
static POINT g_anchor = { 0, 0 };
static wchar_t g_uiFace[64] = L"Segoe UI", g_iconFace[64] = L"Segoe MDL2 Assets";
static const UINT WM_NOTE_CLOSE = WM_APP + 5, WM_WEB_GO = WM_APP + 6, WM_WEB_BACK = WM_APP + 7, WM_WEB_FWD = WM_APP + 8, WM_WEB_RELOAD = WM_APP + 9, WM_WEB_FOCUSADDR = WM_APP + 10, WM_WEB_NAVURL = WM_APP + 11, WM_WEB_MARK = WM_APP + 12;

static void HidePalette();
static void LayoutChildren();
static void EnterNote(int idx);
static void EnterWeb(const wstring& url);
static void ExitWeb();
static void WebResize(int W, int H);
static void OnNoteChanged();
static void Balloon(const wchar_t* title, const wchar_t* msg, DWORD icon);

// ------------------------------------------------------------------ small helpers
static wstring GetEditText(HWND h) {
    int n = GetWindowTextLengthW(h);
    wstring s((size_t)n, L'\0');
    if (n) GetWindowTextW(h, &s[0], n + 1);
    return s;
}
static COLORREF Mix(COLORREF a, COLORREF b, int pct) {
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
static void FillRectC(HDC dc, RECT r, COLORREF c) { HBRUSH b = CreateSolidBrush(c); FillRect(dc, &r, b); DeleteObject(b); }
static void TextAt(HDC dc, HFONT f, COLORREF c, const wstring& s, RECT r, UINT flags = DT_LEFT) {
    SelectObject(dc, f);
    SetTextColor(dc, c);
    DrawTextW(dc, s.c_str(), (int)s.size(), &r, flags | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
}
static HFONT MakeFont(int h, int weight, const wchar_t* face) {
    return CreateFontW(-h, 0, 0, 0, weight, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, face);
}
static bool FaceExists(const wchar_t* face) {
    HDC dc = GetDC(nullptr);
    HFONT f = MakeFont(12, 400, face);
    HGDIOBJ o = SelectObject(dc, f);
    wchar_t got[64] = {};
    GetTextFaceW(dc, 64, got);
    SelectObject(dc, o); DeleteObject(f); ReleaseDC(nullptr, dc);
    return _wcsicmp(got, face) == 0;
}

static void ApplyTheme() {
    bool dark = true;
    if (g_themeMode == L"light") dark = false;
    else if (g_themeMode != L"dark") {
        DWORD v = 0, sz = 4;
        if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &v, &sz) == ERROR_SUCCESS)
            dark = (v == 0);
    }
    const Theme* t = dark ? &kDark : &kLight;
    if (t == g_th && g_brBg) return;
    g_th = t;
    if (g_brBg) DeleteObject(g_brBg);
    if (g_brEd) DeleteObject(g_brEd);
    if (g_brChip) DeleteObject(g_brChip);
    g_brChip = CreateSolidBrush(t->chip);
    g_brBg = CreateSolidBrush(t->bg);
    g_brEd = CreateSolidBrush(t->editorBg);
    if (g_win11 && g_hwnd) {
        COLORREF bc = t->border; DwmSetWindowAttribute(g_hwnd, 34, &bc, sizeof bc);
        int dk = dark ? 1 : 0; DwmSetWindowAttribute(g_hwnd, 20, &dk, sizeof dk);
    }
    if (g_hEditor) SetWindowTheme(g_hEditor, dark ? L"DarkMode_Explorer" : L"Explorer", nullptr);
}

static void SetDpi(int dpi, bool force = false) {
    if (!force && dpi == g_dpi && g_fTitle) return;
    g_dpi = dpi;
    if (!g_facesChecked) {
        g_facesChecked = true;
        if (FaceExists(L"Segoe UI Variable Text")) wcscpy_s(g_uiFace, L"Segoe UI Variable Text");
        if (FaceExists(L"Segoe Fluent Icons")) wcscpy_s(g_iconFace, L"Segoe Fluent Icons");
    }
    for (HFONT* f : { &g_fSearch, &g_fTitle, &g_fSub, &g_fHdr, &g_fIcon, &g_fChip, &g_fNoteHead })
        if (*f) { DeleteObject(*f); *f = nullptr; }
    g_fSearch = MakeFont(px(g_compact ? 18 : 22), 400, g_uiFace);
    g_fTitle = MakeFont(px(g_compact ? 14 : 15), 400, g_uiFace);
    g_fSub = MakeFont(px(12), 400, g_uiFace);
    g_fHdr = MakeFont(px(12), 600, g_uiFace);
    g_fIcon = MakeFont(px(g_compact ? 15 : 17), 400, g_iconFace);
    g_fChip = MakeFont(px(12), 400, L"Consolas");
    g_fNoteHead = MakeFont(px(15), 600, g_uiFace);
    HDC dc = GetDC(nullptr);
    HGDIOBJ o = SelectObject(dc, g_fSearch);
    TEXTMETRICW tm; GetTextMetricsW(dc, &tm);
    g_lineH = tm.tmHeight + tm.tmExternalLeading;
    SelectObject(dc, g_fTitle);
    GetTextMetricsW(dc, &tm);
    g_addrH = tm.tmHeight + tm.tmExternalLeading + px(2);
    SelectObject(dc, o); ReleaseDC(nullptr, dc);
    g_lastCompact = g_compact;
    if (g_hQuery) SendMessageW(g_hQuery, WM_SETFONT, (WPARAM)g_fSearch, TRUE);
    if (g_hAddr) SendMessageW(g_hAddr, WM_SETFONT, (WPARAM)g_fTitle, TRUE);
}

// ------------------------------------------------------------------ palette metrics
static int SearchPadV() { return px(g_compact ? 9 : 14); }
static int SearchH() { return std::max(SearchPadV() * 2 + g_qLines * g_lineH, px(g_compact ? 44 : 60)); }
static int ItemH() { return px(g_compact ? 38 : 50); }
static int HdrH() { return px(g_compact ? 24 : 28); }
static int FootH() { return g_showFooter ? px(g_compact ? 24 : 28) : 0; }
static int PalW() { return std::min<int>(px(g_width), (int)(g_work.right - g_work.left) - px(8)); }
static int ContentH() { int h = 0; for (auto& r : g_rows) h += r.header ? HdrH() : ItemH(); return h; }
static int ListH() { return std::min(ContentH(), px(g_listMax)); }
static int ListTop() { return SearchH() + 1 + px(4); }
static int PalH() { int h = SearchH(); if (!g_rows.empty()) h += 1 + px(4) + ListH() + px(g_showFooter ? 6 : 8) + FootH(); return h; }
static int NoteW() { return std::max<int>(px(480), std::min<int>(px(g_noteW), (int)(g_work.right - g_work.left) - px(24))); }
static int NoteH() { return std::max<int>(px(320), std::min<int>(px(g_noteH), (int)(g_work.bottom - g_work.top) - px(24))); }

static double g_selY = -10000;               // animated highlight position
static ULONGLONG g_animStart = 0;
static int g_winX = 0, g_winY = 0, g_lastSig = -1;
static HDC g_bbDC = nullptr; static HBITMAP g_bbBmp = nullptr, g_bbOld = nullptr; static int g_bbW = 0, g_bbH = 0;

static int SelTargetY() {   // window y of the selected row (top), or far off-screen
    int cy = ListTop() - g_scroll;
    for (auto& r : g_rows) {
        int h = r.header ? HdrH() : ItemH();
        if (!r.header && r.idx == g_sel) return cy;
        cy += h;
    }
    return -10000;
}
static void SnapSel() { g_selY = SelTargetY(); }
static void StartSelAnim() {
    if (g_animations) SetTimer(g_hwnd, T_SEL, 12, nullptr); else SnapSel();
    InvalidateRect(g_hwnd, nullptr, FALSE);
}
static void FreeBackBuffer() {
    if (!g_bbDC) return;
    SelectObject(g_bbDC, g_bbOld); DeleteObject(g_bbBmp); DeleteDC(g_bbDC);
    g_bbDC = nullptr; g_bbBmp = nullptr; g_bbW = g_bbH = 0;
}
static HDC GetBackBuffer(HDC ref, int w, int h) {   // one reusable off-screen bitmap instead of a new one per paint
    if (g_bbDC && g_bbW == w && g_bbH == h) return g_bbDC;
    FreeBackBuffer();
    g_bbDC = CreateCompatibleDC(ref);
    g_bbBmp = CreateCompatibleBitmap(ref, w, h);
    g_bbOld = (HBITMAP)SelectObject(g_bbDC, g_bbBmp);
    g_bbW = w; g_bbH = h;
    return g_bbDC;
}

static int WebHdr() { return px(52); }
static int WebW() { return std::max(px(520), std::min(px(g_webW), (int)(g_work.right - g_work.left) - px(24))); }
static int WebH() { return std::max(px(360), std::min(px(g_webH), (int)(g_work.bottom - g_work.top) - px(24))); }

static void PlaceWindow(int w, int h, bool show) {
    int x = g_anchor.x, y = g_anchor.y;
    if ((g_noteMode || g_webMode) && !g_atCursor) {
        x = g_work.left + ((g_work.right - g_work.left) - w) / 2;
        y = g_work.top + ((g_work.bottom - g_work.top) - h) / 2;
    }
    if (x + w > g_work.right) x = g_work.right - w;
    if (x < g_work.left) x = g_work.left;
    if (y + h > g_work.bottom) y = g_work.bottom - h;   // never cut off by the screen edge
    if (y < g_work.top) y = g_work.top;
    g_winX = x; g_winY = y;
    SetWindowPos(g_hwnd, show ? HWND_TOPMOST : nullptr, x, y, w, h, show ? SWP_SHOWWINDOW : (SWP_NOZORDER | SWP_NOACTIVATE));
}

static void ApplyLayout() {
    int w = g_webMode ? WebW() : g_noteMode ? NoteW() : PalW(), h = g_webMode ? WebH() : g_noteMode ? NoteH() : PalH();
    int sig = w * 7919 + h * 131 + g_qLines * 17 + (g_noteMode ? 3 : 0) + (g_webMode ? 5 : 0);
    if (sig != g_lastSig) {   // only touch the window when its size really changed (keeps typing smooth)
        g_lastSig = sig;
        PlaceWindow(w, h, false);
        if (!g_win11) SetWindowRgn(g_hwnd, CreateRoundRectRgn(0, 0, w + 1, h + 1, px(14), px(14)), TRUE);
        LayoutChildren();
    }
    InvalidateRect(g_hwnd, nullptr, FALSE);
}

static void BuildRows() {
    g_rows.clear();
    wstring last;
    for (size_t i = 0; i < g_results.size(); i++) {
        if (g_results[i].group != last) { last = g_results[i].group; g_rows.push_back({ true, -1, last }); }
        g_rows.push_back({ false, (int)i, L"" });
    }
}
static void Refresh() {
    g_results = RunQuery(g_query);
    BuildRows();
    g_sel = g_results.empty() ? -1 : 0;
    g_hover = -1; g_scroll = 0;
    ApplyLayout();
    SnapSel();
}
static void ClampListScroll() { g_scroll = std::max(0, std::min(g_scroll, std::max(0, ContentH() - ListH()))); }
static void EnsureVisible() {
    int cy = 0;
    for (size_t i = 0; i < g_rows.size(); i++) {
        int h = g_rows[i].header ? HdrH() : ItemH();
        if (!g_rows[i].header && g_rows[i].idx == g_sel) {
            int top = cy;
            if (i > 0 && g_rows[i - 1].header) top -= HdrH();
            int bot = cy + h, lh = ListH();
            if (top < g_scroll) g_scroll = top;
            else if (bot > g_scroll + lh) g_scroll = bot - lh;
            break;
        }
        cy += h;
    }
    ClampListScroll();
}
static int HitTest(int y) {
    int lt = ListTop();
    if (y < lt || y >= lt + ListH()) return -1;
    int cy = lt - g_scroll;
    for (auto& r : g_rows) {
        int h = r.header ? HdrH() : ItemH();
        if (y >= cy && y < cy + h) return r.header ? -1 : r.idx;
        cy += h;
    }
    return -1;
}
static void MoveSel(int d, bool wrap = true) {
    int n = (int)g_results.size();
    if (!n) return;
    int s = g_sel + d;
    if (wrap) s = (s % n + n) % n; else s = std::max(0, std::min(n - 1, s));
    g_sel = s;
    EnsureVisible();
    StartSelAnim();
}
static void RunSelected() {
    if (g_sel < 0 || g_sel >= (int)g_results.size()) return;
    Item it = g_results[g_sel];
    if (it.kind == K_WINDOW) {   // activate while we still own the foreground, then close
        HWND t = (HWND)(INT_PTR)_wtoi64(it.a.c_str());
        if (IsWindow(t)) { if (IsIconic(t)) ShowWindow(t, SW_RESTORE); SetForegroundWindow(t); }
        HidePalette();
        return;
    }
    if (it.kind == K_BROWSE) { EnterWeb(it.a); HidePalette(); return; }
    if (it.kind == K_NOTE) {
        for (size_t i = 0; i < g_notes.size(); i++)
            if (_wcsicmp(g_notes[i].trig.c_str(), it.a.c_str()) == 0) { EnterNote((int)i); HidePalette(); return; }
        return;
    }
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

// ------------------------------------------------------------------ query box (a real multi-line EDIT control that grows)
static void OnQueryChanged() {
    if (g_suppress || g_noteMode) return;
    if (g_webMode) { g_addrDirty = true; return; }
    wstring raw = GetEditText(g_hQuery);
    wstring low = lower(raw);
    if (!g_webTrigger.empty() && low == lower(g_webTrigger)) SetTimer(g_hwnd, T_PREWARM, 150, nullptr);   // paused on "w": warm the browser
    else KillTimer(g_hwnd, T_PREWARM);
    for (size_t i = 0; i < g_notes.size(); i++)
        if (low == lower(g_notes[i].trig) + L" ") { EnterNote((int)i); HidePalette(); return; }   // "n " turns the box into the note editor
    if (!g_webTrigger.empty() && low == lower(g_webTrigger) + L" ") { EnterWeb(L""); HidePalette(); return; }   // "w " opens the browser
    g_query = raw;
    for (auto& c : g_query) if (c == L'\r' || c == L'\n') c = L' ';
    int lines = (int)SendMessageW(g_hQuery, EM_GETLINECOUNT, 0, 0);
    lines = std::max(1, std::min(6, lines));
    bool changed = lines != g_qLines;
    g_qLines = lines;
    bool heavy = false;   // file / window search: wait a moment while you are still typing
    { wstring lw = lower(g_query); size_t b = lw.find_first_not_of(L' ');
      if (b != wstring::npos) {
          size_t e = lw.find(L' ', b);
          if (e != wstring::npos) { wstring w0 = lw.substr(b, e - b); heavy = (w0 == lower(g_fileTrigger)) || (!g_windowTrigger.empty() && w0 == lower(g_windowTrigger)); }
      } }
    if (heavy) { SetTimer(g_hwnd, T_QUERY, 45, nullptr); if (changed) ApplyLayout(); }
    else { KillTimer(g_hwnd, T_QUERY); Refresh(); }
    if (changed) SendMessageW(g_hQuery, EM_LINESCROLL, 0, -100);
}

static LRESULT CALLBACK QueryProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
        case WM_KEYDOWN: {
            bool ctrl = GetKeyState(VK_CONTROL) < 0, shift = GetKeyState(VK_SHIFT) < 0;
            if (g_webMode) {   // the box is the address bar
                if (w == VK_ESCAPE) { HidePalette(); return 0; }
                if (w == VK_RETURN) { PostMessageW(g_hwnd, WM_WEB_GO, 0, 0); return 0; }
                if (ctrl && w == 'A') { SendMessageW(h, EM_SETSEL, 0, -1); return 0; }
                if (w == VK_F5) { PostMessageW(g_hwnd, WM_WEB_RELOAD, 0, 0); return 0; }
                break;
            }
            switch (w) {
                case VK_ESCAPE: HidePalette(); return 0;
                case VK_DOWN: MoveSel(1); return 0;
                case VK_UP: MoveSel(-1); return 0;
                case VK_NEXT: MoveSel(5, false); return 0;
                case VK_PRIOR: MoveSel(-5, false); return 0;
                case VK_TAB: MoveSel(shift ? -1 : 1); return 0;
                case VK_RETURN:
                    if (shift) break;
                    if (ctrl && g_sel >= 0 && g_sel < (int)g_results.size() && !g_results[g_sel].alt.empty()) {
                        wstring p = g_results[g_sel].alt;
                        HidePalette();
                        Reveal(p);
                    } else RunSelected();
                    return 0;
                case 'H': if (ctrl) { HideIndex(g_sel); return 0; } break;
                case 'D':   // pin / unpin the selected item (shows first when the box is empty)
                    if (ctrl && g_sel >= 0 && g_sel < (int)g_results.size() && g_results[g_sel].track) {
                        wstring id = g_results[g_sel].id;
                        if (g_pins.count(id)) g_pins.erase(id); else g_pins.insert(id);
                        SavePins();
                        int keep = g_sel;
                        Refresh();
                        if (!g_results.empty()) { g_sel = std::min(keep, (int)g_results.size() - 1); EnsureVisible(); SnapSel(); }
                        return 0;
                    }
                    break;
                case 'C':   // Ctrl+Shift+C copies the path of a file result
                    if (ctrl && shift && g_sel >= 0 && g_sel < (int)g_results.size() && !g_results[g_sel].alt.empty()) {
                        SetClipboard(g_results[g_sel].alt);
                        return 0;
                    }
                    break;
                case 'A': if (ctrl) { SendMessageW(h, EM_SETSEL, 0, -1); return 0; } break;
            }
            break;
        }
        case WM_CHAR: {
            bool ctrl = GetKeyState(VK_CONTROL) < 0;
            if (w == 13) { if (GetKeyState(VK_SHIFT) < 0) break; return 0; }
            if (w == 27 || w == 9) return 0;
            if (ctrl && (w == 8 || w == 1 || w == 4)) return 0;
            break;
        }
        case WM_SYSKEYDOWN:   // Alt+1 ... Alt+9 runs that result (in the browser: Alt+Left / Alt+Right)
            if (g_webMode) {
                if (!g_webPage && w >= '1' && w <= '9') { PostMessageW(g_hwnd, WM_WEB_MARK, (WPARAM)(w - '1'), 0); return 0; }
                if (w == VK_LEFT) { PostMessageW(g_hwnd, WM_WEB_BACK, 0, 0); return 0; }
                if (w == VK_RIGHT) { PostMessageW(g_hwnd, WM_WEB_FWD, 0, 0); return 0; }
                break;
            }
            if (w >= '1' && w <= '9') {
                int i = (int)(w - '1');
                if (i < (int)g_results.size()) { g_sel = i; RunSelected(); }
                return 0;
            }
            break;
        case WM_SYSCHAR: if (w >= '1' && w <= '9') return 0; break;
        case WM_MOUSEWHEEL: SendMessageW(g_hwnd, WM_MOUSEWHEEL, w, l); return 0;
        case WM_PAINT: {
            LRESULT r = CallWindowProcW(g_oldQuery, h, m, w, l);
            if (GetWindowTextLengthW(h) == 0) {
                HDC dc = GetDC(h);
                RECT rc; GetClientRect(h, &rc);
                rc.left += px(2);
                SetBkMode(dc, TRANSPARENT);
                TextAt(dc, g_fSearch, g_th->placeholder, L"Search apps, files, settings\u2026   f report \u00B7 g cats \u00B7 n notes", rc, DT_VCENTER);
                ReleaseDC(h, dc);
            }
            return r;
        }
    }
    return CallWindowProcW(g_oldQuery, h, m, w, l);
}

static void CreateQueryEdit() {
    g_hQuery = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_LEFT,
                               0, 0, 100, g_lineH, g_hwnd, (HMENU)(INT_PTR)101, g_inst, nullptr);
    SendMessageW(g_hQuery, WM_SETFONT, (WPARAM)g_fSearch, TRUE);
    SendMessageW(g_hQuery, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, 0);
    SendMessageW(g_hQuery, EM_LIMITTEXT, 2000, 0);
    g_oldQuery = (WNDPROC)SetWindowLongPtrW(g_hQuery, GWLP_WNDPROC, (LONG_PTR)QueryProc);
}

// ------------------------------------------------------------------ scrollbar geometry (thin, draggable)
static bool ListThumb(RECT& track, RECT& thumb) {
    int ch = ContentH(), lh = ListH();
    if (ch <= lh) return false;
    int W = PalW();
    track = { W - px(9), ListTop() + px(2), W - px(4), ListTop() + lh - px(2) };
    int th = std::max<int>(px(24), (track.bottom - track.top) * lh / ch);
    int ty = track.top + (int)((long long)g_scroll * ((track.bottom - track.top) - th) / (ch - lh));
    thumb = { track.left, ty, track.right, ty + th };
    return true;
}

// ------------------------------------------------------------------ palette drawing
static void DrawPalette(HDC dc, int W, int H) {
    const Theme& T = *g_th;
    RECT all = { 0, 0, W, H };
    FillRectC(dc, all, T.bg);
    HBRUSH bd = CreateSolidBrush(T.border); FrameRect(dc, &all, bd); DeleteObject(bd);
    SetBkMode(dc, TRANSPARENT);

    wchar_t g[2] = { 0xE721, 0 };
    TextAt(dc, g_fIcon, T.hdr, g, { px(20), SearchPadV(), px(48), SearchPadV() + g_lineH }, DT_VCENTER);
    if (g_rows.empty()) return;

    FillRectC(dc, { 1, SearchH(), W - 1, SearchH() + 1 }, T.divider);

    int lt = ListTop(), lh = ListH();
    int sc = SaveDC(dc);
    IntersectClipRect(dc, 0, lt, W, lt + lh);
    if (g_sel >= 0) {   // selection highlight glides to the chosen row
        int tgt = SelTargetY();
        int sy = (g_animations && g_selY > -9000) ? (int)g_selY : tgt;
        if (tgt > -9000) {
            FillRound(dc, { px(8), sy + px(1), W - px(8), sy + ItemH() - px(1) }, T.sel, px(8));
            if (g_sel < (int)g_results.size()) FillRound(dc, { px(11), sy + px(11), px(14), sy + ItemH() - px(11) }, Accent(g_results[g_sel].cat), px(3));
        }
    }
    int y = lt - g_scroll;
    for (auto& r : g_rows) {
        int h = r.header ? HdrH() : ItemH();
        if (y + h > lt && y < lt + lh) {
            if (r.header) {
                TextAt(dc, g_fHdr, T.hdr, r.text, { px(22), y + px(g_compact ? 5 : 8), W - px(20), y + h }, 0);
            } else {
                const Item& it = g_results[r.idx];
                bool sel = r.idx == g_sel;
                RECT rr = { px(8), y + px(1), W - px(8), y + h - px(1) };
                if (!sel && r.idx == g_hover) FillRound(dc, rr, T.hov, px(8));
                COLORREF ac = Accent(it.cat);
                int ts = px(g_compact ? 26 : 34);
                RECT tile = { px(18), y + (h - ts) / 2, px(18) + ts, y + (h - ts) / 2 + ts };
                if (it.tint >= 0) FillRound(dc, tile, (COLORREF)it.tint, px(9));
                else FillRound(dc, tile, Mix(sel ? T.sel : T.bg, ac, T.dark ? 24 : 18), px(9));
                wchar_t gl[2] = { it.glyph, 0 };
                TextAt(dc, g_fIcon, ac, gl, tile, DT_CENTER | DT_VCENTER);
                int tx = tile.right + px(12);
                int right = W - px(g_hover >= 0 || ContentH() > lh ? 26 : 20);
                int hx = right;
                right -= ((g_showNumbers && r.idx < 9) ? px(48) : 0) + (it.pinned ? px(22) : 0);
                if (g_showNumbers && r.idx < 9) {
                    wchar_t nb[8]; swprintf(nb, 8, L"Alt+%d", r.idx + 1);
                    TextAt(dc, g_fSub, T.dim, nb, { hx - px(44), y, hx, y + h }, DT_RIGHT | DT_VCENTER);
                    hx -= px(48);
                }
                if (it.pinned) {
                    wchar_t pg[2] = { 0xE718, 0 };
                    TextAt(dc, g_fIcon, T.hdr, pg, { hx - px(18), y, hx, y + h }, DT_RIGHT | DT_VCENTER);
                }
                if (!g_compact) {
                    TextAt(dc, g_fTitle, T.text, it.title, { tx, y + px(7), right, y + px(27) }, 0);
                    TextAt(dc, g_fSub, T.dim, it.sub, { tx, y + px(27), right, y + px(45) }, 0);
                } else {
                    SelectObject(dc, g_fTitle);
                    SIZE sz; GetTextExtentPoint32W(dc, it.title.c_str(), (int)it.title.size(), &sz);
                    int tw = std::min<int>((int)sz.cx, (right - tx) * 65 / 100);
                    TextAt(dc, g_fTitle, T.text, it.title, { tx, y, tx + tw, y + h }, DT_VCENTER);
                    int sx = tx + tw + px(12);
                    if (sx < right - px(30)) TextAt(dc, g_fSub, T.dim, it.sub, { sx, y, right, y + h }, DT_VCENTER);
                }
            }
        }
        y += h;
    }
    RestoreDC(dc, sc);

    RECT tr, th;
    if (ListThumb(tr, th)) FillRound(dc, th, T.thumb, px(5));

    if (g_showFooter) {
        int fy = lt + lh + px(6);
        FillRectC(dc, { 1, fy, W - 1, H - 1 }, T.footerBg);
        TextAt(dc, g_fSub, T.footerText, L"\u21B5 Open    Alt+1-9 Quick run    Ctrl+D Pin    Ctrl+\u21B5 Show in folder    Ctrl+H Hide    Esc Close", { px(20), fy, W - px(20), H - 1 }, DT_VCENTER);
    }
}

// ================================================================== NOTE EDITOR (Markdown, live preview)
enum { CH_WRAP, CH_PREFIX, CH_HEAD, CH_INSERT, CH_DATE };
struct ChipDef { const wchar_t* id; const wchar_t* label; int kind; const wchar_t* a; const wchar_t* b; };
static const ChipDef kChips[] = {
    { L"h1", L"# Heading", CH_HEAD, L"# ", L"" },
    { L"h2", L"## Heading", CH_HEAD, L"## ", L"" },
    { L"h3", L"### Heading", CH_HEAD, L"### ", L"" },
    { L"bold", L"**bold**", CH_WRAP, L"**", L"**" },
    { L"italic", L"*italic*", CH_WRAP, L"*", L"*" },
    { L"strike", L"~~strike~~", CH_WRAP, L"~~", L"~~" },
    { L"mark", L"==mark==", CH_WRAP, L"==", L"==" },
    { L"code", L"`code`", CH_WRAP, L"`", L"`" },
    { L"codeblock", L"```block```", CH_WRAP, L"```\r\n", L"\r\n```" },
    { L"quote", L"> quote", CH_PREFIX, L"> ", L"" },
    { L"bullet", L"- bullet", CH_PREFIX, L"- ", L"" },
    { L"number", L"1. number", CH_PREFIX, L"1. ", L"" },
    { L"task", L"- [ ] task", CH_PREFIX, L"- [ ] ", L"" },
    { L"link", L"[link](url)", CH_WRAP, L"[", L"](url)" },
    { L"image", L"![img](url)", CH_WRAP, L"![", L"](url)" },
    { L"table", L"| table |", CH_INSERT, L"| Column | Column |\r\n| --- | --- |\r\n| Cell | Cell |\r\n", L"" },
    { L"hr", L"--- rule", CH_INSERT, L"\r\n---\r\n", L"" },
    { L"date", L"date", CH_DATE, L"", L"" },
};
static const int kChipN = (int)(sizeof(kChips) / sizeof(kChips[0]));

static int g_noteIdx = -1, g_noteView = 1;   // view: 0 edit, 1 split, 2 preview
static wstring g_notePath, g_noteStats;
static bool g_noteLF = false, g_noteDirty = false, g_swallowEnter = false;
static MdView g_md;
static int g_pvScroll = 0, g_pvH = 0;
static vector<int> g_chipOrder;
static std::unordered_map<wstring, int> g_chipCount;
static vector<RECT> g_chipRects;
static int g_chipScroll = 0, g_chipTotalW = 0, g_chipHot = -1;
static int g_drag = 0, g_dragY = 0, g_dragScroll = 0;   // 1 = list thumb, 2 = preview thumb

static int NHdr() { return px(46); }
static int NStat() { return px(40); }
static void ClientWH(int& W, int& H) { RECT rc; GetClientRect(g_hwnd, &rc); W = rc.right; H = rc.bottom; }
static void ClientWHw(HWND hh, int& W, int& H) { RECT rc; GetClientRect(hh, &rc); W = rc.right; H = rc.bottom; }
static RECT NoteBody(int W, int H) { return { 0, NHdr() + 1, W, H - NStat() }; }
static RECT EditorArea(int W, int H) { RECT r = NoteBody(W, H); if (g_noteView == 1) r.right = W / 2; return r; }
static RECT PreviewArea(int W, int H) { RECT r = NoteBody(W, H); if (g_noteView == 1) r.left = W / 2 + 1; return r; }
static void BtnRects(int W, RECT r[3]) {
    int bw = px(68), bh = px(28), x = W - px(14) - 3 * bw, y = (NHdr() - bh) / 2;
    for (int i = 0; i < 3; i++) r[i] = { x + i * bw, y, x + (i + 1) * bw, y + bh };
}

static wstring ExpandNotePath(const wstring& p) {
    wstring e = ExpandEnv(p);
    wchar_t d[32] = L"";
    GetDateFormatW(LOCALE_USER_DEFAULT, 0, nullptr, L"yyyy-MM-dd", d, 32);
    e = ReplaceAll(e, L"{date}", d);
    if (PathIsRelativeW(e.c_str())) {
        PWSTR docs = nullptr; wstring base;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents, 0, nullptr, &docs)) && docs) { base = docs; CoTaskMemFree(docs); }
        e = base + L"\\" + e;
    }
    return e;
}

// ---- per-note ordering of the syntax chips (most used first, remembered per file)
static wstring ChipFile() { return g_dir + L"\\note_usage.txt"; }
static wstring PathKey() { return lower(g_notePath); }
static void LoadChipCounts() {
    g_chipCount.clear();
    vector<wstring> ln;
    if (!ReadLines(ChipFile(), ln)) return;
    wstring key = PathKey();
    for (auto& l : ln) { auto p = Split(l, L'\t'); if (p.size() == 3 && p[0] == key) g_chipCount[p[1]] = _wtoi(p[2].c_str()); }
}
static void SaveChipCounts() {
    if (g_chipCount.empty()) return;
    vector<wstring> ln, keep;
    ReadLines(ChipFile(), ln);
    wstring key = PathKey();
    for (auto& l : ln) { auto p = Split(l, L'\t'); if (p.size() == 3 && p[0] != key) keep.push_back(l); }
    wstring out;
    for (auto& l : keep) out += l + L"\n";
    for (auto& kv : g_chipCount) if (kv.second > 0) out += key + L"\t" + kv.first + L"\t" + std::to_wstring(kv.second) + L"\n";
    WriteText(ChipFile(), out);
}
static void BuildChipOrder() {
    g_chipOrder.clear();
    for (int i = 0; i < kChipN; i++) g_chipOrder.push_back(i);
    std::stable_sort(g_chipOrder.begin(), g_chipOrder.end(), [](int a, int b) {
        int ca = g_chipCount.count(kChips[a].id) ? g_chipCount[kChips[a].id] : 0;
        int cb = g_chipCount.count(kChips[b].id) ? g_chipCount[kChips[b].id] : 0;
        return ca > cb;
    });
}
static void LayoutChips() {
    HDC dc = GetDC(g_nw);
    HGDIOBJ o = SelectObject(dc, g_fChip);
    g_chipRects.assign(g_chipOrder.size(), RECT{ 0, 0, 0, 0 });
    int x = px(12), h = px(26);
    for (size_t k = 0; k < g_chipOrder.size(); k++) {
        const wchar_t* lb = kChips[g_chipOrder[k]].label;
        SIZE sz; GetTextExtentPoint32W(dc, lb, (int)wcslen(lb), &sz);
        int w = sz.cx + px(18);
        g_chipRects[k] = { x, 0, x + w, h };
        x += w + px(6);
    }
    g_chipTotalW = x;
    SelectObject(dc, o); ReleaseDC(g_nw, dc);
    int W, H; ClientWHw(g_nw, W, H);
    g_chipScroll = std::max(0, std::min(g_chipScroll, std::max(0, g_chipTotalW - (W - px(12)))));
}

// ---- preview
static bool PvThumb(RECT& track, RECT& thumb) {
    int W, H; ClientWHw(g_nw, W, H);
    RECT pa = PreviewArea(W, H);
    int vh = pa.bottom - pa.top;
    if (g_pvH <= vh) return false;
    track = { pa.right - px(9), pa.top + px(4), pa.right - px(4), pa.bottom - px(4) };
    int th = std::max<int>(px(28), (track.bottom - track.top) * vh / g_pvH);
    int ty = track.top + (int)((long long)g_pvScroll * ((track.bottom - track.top) - th) / (g_pvH - vh));
    thumb = { track.left, ty, track.right, ty + th };
    return true;
}
static void ClampPv() {
    int W, H; ClientWHw(g_nw, W, H);
    RECT pa = PreviewArea(W, H);
    g_pvScroll = std::max<int>(0, std::min<int>(g_pvScroll, std::max<int>(0, g_pvH - (pa.bottom - pa.top))));
}
static void LayoutPreview() {
    if (!g_noteOpen || g_noteView == 0) return;
    int W, H; ClientWHw(g_nw, W, H);
    RECT pa = PreviewArea(W, H);
    HDC dc = GetDC(g_nw);
    g_md.Layout(dc, (pa.right - pa.left) - px(10), px(15));
    ReleaseDC(g_nw, dc);
    g_pvH = g_md.height;
    ClampPv();
}
static void RebuildPreview(bool follow) {
    if (!g_noteOpen || !g_hEditor) return;
    wstring t = GetEditText(g_hEditor);
    size_t words = 0; bool in = false;
    for (wchar_t c : t) { bool sp = iswspace(c) != 0; if (!sp && !in) words++; in = !sp; }
    g_noteStats = std::to_wstring(words) + L" words \u00B7 " + std::to_wstring(t.size()) + L" chars";
    if (g_noteView != 0) {
        g_md.Parse(t);
        LayoutPreview();
        if (follow && g_noteView == 1) {
            int W, H; ClientWHw(g_nw, W, H);
            RECT pa = PreviewArea(W, H);
            int vh = pa.bottom - pa.top;
            if (g_pvH > vh) {
                int total = (int)SendMessageW(g_hEditor, EM_GETLINECOUNT, 0, 0);
                int ln = (int)SendMessageW(g_hEditor, EM_LINEFROMCHAR, (WPARAM)-1, 0);
                double fr = total > 1 ? (double)ln / (total - 1) : 0.0;
                g_pvScroll = (int)(fr * (g_pvH - vh));
                ClampPv();
            }
        }
    }
    InvalidateRect(g_nw, nullptr, FALSE);
}
static void OnNoteChanged() {
    if (g_suppress || !g_noteOpen) return;
    g_noteDirty = true;
    SetTimer(g_nw, T_AUTOSAVE, 2500, nullptr);
    SetTimer(g_nw, T_PREVIEW, 150, nullptr);
}

static void LayoutChildren() {   // the palette's own layout (search box)
    int W, H; ClientWH(W, H);
    int eh = g_qLines * g_lineH;
    MoveWindow(g_hQuery, px(52), (SearchH() - eh) / 2, W - px(52) - px(20), eh, TRUE);
}
static void NoteLayout() {   // the note window's layout
    if (!g_noteOpen || !g_nw) return;
    int W, H; ClientWHw(g_nw, W, H);
    if (g_hEditor) {
        if (g_noteView == 2) ShowWindow(g_hEditor, SW_HIDE);
        else {
            RECT e = EditorArea(W, H);
            ShowWindow(g_hEditor, SW_SHOW);
            MoveWindow(g_hEditor, e.left + px(14), e.top + px(10), e.right - e.left - px(24), e.bottom - e.top - px(14), TRUE);
        }
    }
    LayoutChips();
    LayoutPreview();
    InvalidateRect(g_nw, nullptr, FALSE);
}
static void SetView(int v) {
    g_noteView = v;
    NoteLayout();
    RebuildPreview(false);
    if (v == 2) SetFocus(g_nw); else if (g_hEditor) SetFocus(g_hEditor);
}
static void CycleView() { SetView((g_noteView + 1) % 3); }

// ---- load / save
static void SaveNote() {
    if (!g_noteOpen || !g_hEditor) return;
    KillTimer(g_nw, T_AUTOSAVE);
    if (g_noteDirty) {
        wstring t = GetEditText(g_hEditor);
        if (g_noteLF) t = ReplaceAll(t, L"\r\n", L"\n");
        size_t sl = g_notePath.find_last_of(L'\\');
        if (sl != wstring::npos) SHCreateDirectoryExW(nullptr, g_notePath.substr(0, sl).c_str(), nullptr);
        bool exists = PathFileExistsW(g_notePath.c_str()) != 0;
        if (exists || !t.empty()) {
            if (WriteFileUtf8(g_notePath, t)) g_noteDirty = false;
            else Balloon(L"Quick Palette", L"Could not save the note. Check the note path in config.ini.", NIIF_WARNING);
        }
    }
    SaveChipCounts();
}

static LRESULT CALLBACK EditorProc(HWND h, UINT m, WPARAM w, LPARAM l);

static HWND MakeToolWindow(const wchar_t* title, int w96, int h96);

static void NoteContentClose() {   // free the editor, preview and fonts (the window itself stays)
    if (g_nw) { KillTimer(g_nw, T_AUTOSAVE); KillTimer(g_nw, T_PREVIEW); }
    if (g_hEditor) { DestroyWindow(g_hEditor); g_hEditor = nullptr; g_oldEditor = nullptr; }
    if (g_fNote) { DeleteObject(g_fNote); g_fNote = nullptr; }
    g_md.Clear(); g_chipRects.clear(); g_noteStats.clear(); g_drag = 0;
}

static void NoteLoad(int idx) {
    g_noteIdx = idx;
    g_noteView = g_noteViewCfg == L"edit" ? 0 : g_noteViewCfg == L"preview" ? 2 : 1;
    g_notePath = ExpandNotePath(g_notes[idx].path);

    wstring txt;
    bool had = ReadFileUtf8(g_notePath, txt, 4u * 1024 * 1024);
    g_noteLF = had && txt.find(L'\r') == wstring::npos && txt.find(L'\n') != wstring::npos;
    if (had) { txt = ReplaceAll(txt, L"\r\n", L"\n"); txt = ReplaceAll(txt, L"\n", L"\r\n"); }

    g_fNote = MakeFont(px(g_noteFontSize), 400, g_noteFont.c_str());
    g_hEditor = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | ES_NOHIDESEL,
                                0, 0, 100, 100, g_nw, (HMENU)(INT_PTR)102, g_inst, nullptr);
    SendMessageW(g_hEditor, WM_SETFONT, (WPARAM)g_fNote, TRUE);
    SendMessageW(g_hEditor, EM_LIMITTEXT, 4000000, 0);
    SetWindowTheme(g_hEditor, g_th->dark ? L"DarkMode_Explorer" : L"Explorer", nullptr);
    g_oldEditor = (WNDPROC)SetWindowLongPtrW(g_hEditor, GWLP_WNDPROC, (LONG_PTR)EditorProc);
    g_suppress = true; SetWindowTextW(g_hEditor, txt.c_str()); g_suppress = false;
    SendMessageW(g_hEditor, EM_SETSEL, (WPARAM)txt.size(), (LPARAM)txt.size());
    SendMessageW(g_hEditor, EM_SCROLLCARET, 0, 0);

    g_noteDirty = false;
    LoadChipCounts(); BuildChipOrder();
    g_md.Clear(); g_md.bodyFace = g_uiFace; g_md.monoFace = g_noteFont;
    g_pvScroll = 0; g_chipScroll = 0; g_chipHot = -1;
    SetWindowTextW(g_nw, (g_notes[idx].name + L" \u2014 Quick Palette").c_str());
    NoteLayout();
    RebuildPreview(false);
    if (g_noteView == 2) SetFocus(g_nw); else SetFocus(g_hEditor);
}

static void EnterNote(int idx) {
    if (idx < 0 || idx >= (int)g_notes.size()) return;
    if (g_noteOpen && g_nw) {   // already open: switch note if needed, then bring it forward
        if (g_noteIdx != idx) { SaveNote(); NoteContentClose(); NoteLoad(idx); }
        if (IsIconic(g_nw)) ShowWindow(g_nw, SW_RESTORE);
        SetForegroundWindow(g_nw);
        return;
    }
    g_nw = MakeToolWindow(L"Notes", g_noteW, g_noteH);
    if (!g_nw) return;
    g_noteOpen = true;
    NoteLoad(idx);
    ShowWindow(g_nw, SW_SHOW);
    SetForegroundWindow(g_nw);
    if (g_hEditor && g_noteView != 2) SetFocus(g_hEditor);
}

static void ExitNote() {   // the note window was destroyed
    NoteContentClose();
    g_noteOpen = false; g_noteIdx = -1; g_nw = nullptr;
}

// ---- inserting markdown syntax at the cursor (clicking a chip, or typing shortcuts)
static void InsertChip(int ci) {
    if (ci < 0 || ci >= kChipN || !g_hEditor) return;
    const ChipDef& c = kChips[ci];
    HWND e = g_hEditor;
    DWORD s = 0, en = 0;
    SendMessageW(e, EM_GETSEL, (WPARAM)&s, (LPARAM)&en);
    wstring all = GetEditText(e);
    if (s > all.size()) s = (DWORD)all.size();
    if (en > all.size()) en = (DWORD)all.size();
    wstring sel = all.substr(s, en - s);
    switch (c.kind) {
        case CH_WRAP: {
            wstring ins = wstring(c.a) + sel + c.b;
            SendMessageW(e, EM_REPLACESEL, TRUE, (LPARAM)ins.c_str());
            if (sel.empty()) { DWORD p = s + (DWORD)wcslen(c.a); SendMessageW(e, EM_SETSEL, p, p); }
            break;
        }
        case CH_INSERT:
            SendMessageW(e, EM_REPLACESEL, TRUE, (LPARAM)c.a);
            break;
        case CH_DATE: {
            wchar_t d[32] = L"";
            GetDateFormatW(LOCALE_USER_DEFAULT, 0, nullptr, L"yyyy-MM-dd", d, 32);
            SendMessageW(e, EM_REPLACESEL, TRUE, (LPARAM)d);
            break;
        }
        case CH_PREFIX: case CH_HEAD: {
            size_t ls = 0;
            if (s > 0) { size_t p = all.rfind(L'\n', s - 1); ls = (p == wstring::npos) ? 0 : p + 1; }
            size_t rm = 0, al = wcslen(c.a);
            if (c.kind == CH_HEAD) {
                while (ls + rm < all.size() && all[ls + rm] == L'#') rm++;
                if (rm && ls + rm < all.size() && all[ls + rm] == L' ') rm++;
                if (rm == al && all.compare(ls, rm, c.a) == 0) {      // same heading again: toggle off
                    SendMessageW(e, EM_SETSEL, (WPARAM)ls, (LPARAM)(ls + rm));
                    SendMessageW(e, EM_REPLACESEL, TRUE, (LPARAM)L"");
                    break;
                }
            } else if (all.compare(ls, al, c.a) == 0) {               // same prefix again: toggle off
                SendMessageW(e, EM_SETSEL, (WPARAM)ls, (LPARAM)(ls + al));
                SendMessageW(e, EM_REPLACESEL, TRUE, (LPARAM)L"");
                break;
            }
            SendMessageW(e, EM_SETSEL, (WPARAM)ls, (LPARAM)(ls + rm));
            SendMessageW(e, EM_REPLACESEL, TRUE, (LPARAM)c.a);
            size_t np = (s >= ls + rm) ? s + al - rm : ls + al;
            SendMessageW(e, EM_SETSEL, (WPARAM)np, (LPARAM)np);
            break;
        }
    }
    g_chipCount[c.id]++;
    SendMessageW(e, EM_SCROLLCARET, 0, 0);
}
static void InsertChipId(const wchar_t* id) { for (int i = 0; i < kChipN; i++) if (!wcscmp(kChips[i].id, id)) { InsertChip(i); return; } }

// Enter inside a list/quote continues it ("- " -> next "- "), Enter on an empty item ends the list
static bool ListContinue() {
    HWND e = g_hEditor;
    DWORD s = 0, en = 0;
    SendMessageW(e, EM_GETSEL, (WPARAM)&s, (LPARAM)&en);
    if (s != en) return false;
    wstring all = GetEditText(e);
    if (s > all.size()) return false;
    size_t ls = 0;
    if (s > 0) { size_t p = all.rfind(L'\n', s - 1); ls = (p == wstring::npos) ? 0 : p + 1; }
    wstring line = all.substr(ls, s - ls);
    size_t p = 0;
    while (p < line.size() && line[p] == L' ') p++;
    wstring indent = line.substr(0, p), rest = line.substr(p), next;
    size_t used = 0;
    if (rest.compare(0, 2, L"> ") == 0) { next = L"> "; used = 2; }
    else if (rest.size() >= 6 && (rest.compare(0, 6, L"- [ ] ") == 0 || rest.compare(0, 6, L"- [x] ") == 0 || rest.compare(0, 6, L"- [X] ") == 0)) { next = L"- [ ] "; used = 6; }
    else if (rest.size() >= 2 && (rest[0] == L'-' || rest[0] == L'*' || rest[0] == L'+') && rest[1] == L' ') { next = rest.substr(0, 2); used = 2; }
    else {
        size_t q = 0; while (q < rest.size() && iswdigit(rest[q])) q++;
        if (q > 0 && q < 10 && q + 1 < rest.size() && rest[q] == L'.' && rest[q + 1] == L' ') {
            next = std::to_wstring(_wtoi(rest.substr(0, q).c_str()) + 1) + L". "; used = q + 2;
        } else return false;
    }
    if (rest.size() == used) {   // empty item: stop the list
        SendMessageW(e, EM_SETSEL, (WPARAM)ls, (LPARAM)s);
        SendMessageW(e, EM_REPLACESEL, TRUE, (LPARAM)L"");
        return true;
    }
    wstring ins = L"\r\n" + indent + next;
    SendMessageW(e, EM_REPLACESEL, TRUE, (LPARAM)ins.c_str());
    SendMessageW(e, EM_SCROLLCARET, 0, 0);
    return true;
}

static LRESULT CALLBACK EditorProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
        case WM_KEYDOWN: {
            bool ctrl = GetKeyState(VK_CONTROL) < 0, shift = GetKeyState(VK_SHIFT) < 0;

            if (ctrl) {
                if (w == 'S') { SaveNote(); return 0; }
                if (w == 'P') { CycleView(); return 0; }
                if (w == 'B') { InsertChipId(L"bold"); return 0; }
                if (w == 'I') { InsertChipId(L"italic"); return 0; }
                if (w == 'K') { InsertChipId(L"link"); return 0; }
                if (w == 'A') { SendMessageW(h, EM_SETSEL, 0, -1); return 0; }
            }
            if (w == VK_RETURN && !ctrl && !shift && ListContinue()) { g_swallowEnter = true; return 0; }
            break;
        }
        case WM_CHAR: {
            if (w == 13 && g_swallowEnter) { g_swallowEnter = false; return 0; }
            if (w == 27) return 0;
            if (GetKeyState(VK_CONTROL) < 0 && (w == 1 || w == 2 || w == 9 || w == 11 || w == 16 || w == 19)) return 0;
            break;
        }
    }
    return CallWindowProcW(g_oldEditor, h, m, w, l);
}

// ---- drawing
static void DrawNote(HDC dc, int W, int H) {
    const Theme& T = *g_th;
    RECT all = { 0, 0, W, H };
    FillRectC(dc, all, T.bg);
    SetBkMode(dc, TRANSPARENT);
    if (g_noteView != 2) FillRectC(dc, EditorArea(W, H), T.editorBg);
    if (g_noteView == 1) FillRectC(dc, { W / 2, NHdr() + 1, W / 2 + 1, H - NStat() }, T.divider);

    // header
    wchar_t gl[2] = { 0xE8A5, 0 };
    TextAt(dc, g_fIcon, Accent(C_FILE), gl, { px(16), 0, px(40), NHdr() }, DT_VCENTER);
    wstring name = (g_noteIdx >= 0 && g_noteIdx < (int)g_notes.size()) ? g_notes[g_noteIdx].name : L"Note";
    SelectObject(dc, g_fNoteHead);
    SIZE nsz; GetTextExtentPoint32W(dc, name.c_str(), (int)name.size(), &nsz);
    RECT bt[3]; BtnRects(W, bt);
    int titleRight = bt[0].left - px(150);
    TextAt(dc, g_fNoteHead, T.text, name, { px(44), 0, std::max((int)px(80), titleRight), NHdr() }, DT_VCENTER);
    int px0 = px(44) + nsz.cx + px(12);
    if (px0 < titleRight - px(40)) TextAt(dc, g_fSub, T.dim, g_notePath, { px0, 0, titleRight, NHdr() }, DT_VCENTER | DT_PATH_ELLIPSIS);
    TextAt(dc, g_fSub, T.dim, g_noteStats, { bt[0].left - px(150), 0, bt[0].left - px(10), NHdr() }, DT_VCENTER | DT_RIGHT);
    static const wchar_t* lbl[3] = { L"Edit", L"Split", L"Preview" };
    for (int i = 0; i < 3; i++) {
        bool on = (i == g_noteView);
        FillRound(dc, bt[i], on ? T.sel : T.chip, px(7));
        TextAt(dc, g_fSub, on ? T.text : T.dim, lbl[i], bt[i], DT_CENTER | DT_VCENTER);
    }
    FillRectC(dc, { 0, NHdr(), W, NHdr() + 1 }, T.divider);

    // preview
    if (g_noteView != 0) {
        RECT pa = PreviewArea(W, H);
        RECT inner = { pa.left, pa.top, pa.right - px(10), pa.bottom };
        MdTheme mt = { T.text, T.dim, T.link, T.codeBg, T.bar, T.rule, T.hl, T.tableHead, T.tableLine, Accent(C_APP) };
        g_md.Draw(dc, inner, g_pvScroll, mt);
        RECT tr, th;
        if (PvThumb(tr, th)) FillRound(dc, th, T.thumb, px(5));
    }

    // status bar with syntax chips
    int sy = H - NStat();
    FillRectC(dc, { 0, sy, W, H }, T.footerBg);
    FillRectC(dc, { 0, sy, W, sy + 1 }, T.divider);
    int sc = SaveDC(dc);
    IntersectClipRect(dc, px(6), sy, W - px(6), H);
    int ch = px(26), cy = sy + (NStat() - ch) / 2;
    for (size_t k = 0; k < g_chipRects.size(); k++) {
        RECT r = g_chipRects[k];
        r = { r.left - g_chipScroll, cy, r.right - g_chipScroll, cy + ch };
        if (r.right < 0 || r.left > W) continue;
        FillRound(dc, r, (int)k == g_chipHot ? T.chipHot : T.chip, px(7));
        TextAt(dc, g_fChip, T.text, kChips[g_chipOrder[k]].label, r, DT_CENTER | DT_VCENTER);
    }
    RestoreDC(dc, sc);
}

// ---- mouse in note mode
static bool InRect(const RECT& r, int x, int y) { return x >= r.left && x < r.right && y >= r.top && y < r.bottom; }
static int ChipAt(int x, int y) {
    int W, H; ClientWHw(g_nw, W, H);
    int sy = H - NStat(), ch = px(26), cy = sy + (NStat() - ch) / 2;
    if (y < cy || y >= cy + ch) return -1;
    for (size_t k = 0; k < g_chipRects.size(); k++)
        if (x >= g_chipRects[k].left - g_chipScroll && x < g_chipRects[k].right - g_chipScroll) return (int)k;
    return -1;
}
static bool OpenLinkSafe(const wstring& u) {
    if (u.rfind(L"http://", 0) == 0 || u.rfind(L"https://", 0) == 0 || u.rfind(L"mailto:", 0) == 0) {
        ShellExecuteW(nullptr, L"open", u.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        return true;
    }
    return false;
}
static void ToggleTask(int pos) {
    if (!g_hEditor) return;
    wstring t = GetEditText(g_hEditor);
    if (pos < 0 || (size_t)pos + 2 >= t.size() || t[pos] != L'[' || t[pos + 2] != L']') return;
    wchar_t c = t[pos + 1];
    if (c != L' ' && c != L'x' && c != L'X') return;
    SendMessageW(g_hEditor, EM_SETSEL, (WPARAM)(pos + 1), (LPARAM)(pos + 2));
    SendMessageW(g_hEditor, EM_REPLACESEL, TRUE, (LPARAM)(c == L' ' ? L"x" : L" "));
}
static bool PvPoint(int x, int y, int& lx, int& ly) {   // client -> layout coordinates, false if outside the preview
    if (g_noteView == 0) return false;
    int W, H; ClientWHw(g_nw, W, H);
    RECT pa = PreviewArea(W, H);
    if (!InRect(pa, x, y)) return false;
    lx = x - pa.left; ly = y - pa.top + g_pvScroll;
    return true;
}
static void NoteMouseDown(int x, int y) {
    RECT tr, th;
    if (PvThumb(tr, th) && x >= tr.left - px(4) && x < tr.right + px(4) && y >= tr.top && y < tr.bottom) {
        g_drag = 2; g_dragY = y; g_dragScroll = g_pvScroll; SetCapture(g_nw);
        if (!InRect(th, x, y)) {   // clicked the track: jump there
            int vh; { int W, H; ClientWHw(g_nw, W, H); RECT pa = PreviewArea(W, H); vh = pa.bottom - pa.top; }
            int thH = th.bottom - th.top;
            g_pvScroll = (int)((long long)(y - tr.top - thH / 2) * (g_pvH - vh) / std::max<int>(1, (tr.bottom - tr.top) - thH));
            ClampPv(); g_dragY = y; g_dragScroll = g_pvScroll;
            InvalidateRect(g_nw, nullptr, FALSE);
        }
    }
}
static void NoteMouseUp(int x, int y) {
    if (g_drag) { g_drag = 0; ReleaseCapture(); return; }
    int W, H; ClientWHw(g_nw, W, H);
    RECT bt[3]; BtnRects(W, bt);
    for (int i = 0; i < 3; i++) if (InRect(bt[i], x, y)) { if (g_noteView != i) SetView(i); return; }
    int k = ChipAt(x, y);
    if (k >= 0) { InsertChip(g_chipOrder[k]); if (g_hEditor && g_noteView != 2) SetFocus(g_hEditor); return; }
    int lx, ly;
    if (PvPoint(x, y, lx, ly)) {
        int tp = g_md.HitTask(lx, ly);
        if (tp >= 0) { ToggleTask(tp); return; }
        int li = g_md.HitLink(lx, ly);
        if (li >= 0 && li < (int)g_md.links.size()) OpenLinkSafe(g_md.links[li]);
    }
}
static void NoteMouseMove(int x, int y) {
    if (g_drag == 2) {
        RECT tr, th;
        if (PvThumb(tr, th)) {
            int W, H; ClientWHw(g_nw, W, H);
            RECT pa = PreviewArea(W, H);
            int vh = pa.bottom - pa.top, thH = th.bottom - th.top;
            g_pvScroll = g_dragScroll + (int)((long long)(y - g_dragY) * (g_pvH - vh) / std::max<int>(1, (tr.bottom - tr.top) - thH));
            ClampPv();
            InvalidateRect(g_nw, nullptr, FALSE);
        }
        return;
    }
    int hot = ChipAt(x, y);
    if (hot != g_chipHot) { g_chipHot = hot; InvalidateRect(g_nw, nullptr, FALSE); }
}
static void NoteWheel(int delta, int x, int y) {
    int W, H; ClientWHw(g_nw, W, H);
    if (y >= H - NStat()) {
        g_chipScroll -= delta * px(60) / WHEEL_DELTA;
        g_chipScroll = std::max(0, std::min(g_chipScroll, std::max(0, g_chipTotalW - (W - px(12)))));
        InvalidateRect(g_nw, nullptr, FALSE);
    } else if (g_noteView != 0 && InRect(PreviewArea(W, H), x, y)) {
        g_pvScroll -= delta * px(60) / WHEEL_DELTA;
        ClampPv();
        InvalidateRect(g_nw, nullptr, FALSE);
    }
}


// ================================================================== BUILT-IN BROWSER (Microsoft WebView2)
// Start page is drawn by us (instant). The real browser warms up in the background while you type,
// stays warm briefly after closing, then exits completely so it uses no RAM.
static int g_webLoading = 0, g_webHover = -1, g_btnHover = -1, g_progT = 0;
static bool g_webSecure = false, g_webWarm = false, g_webReady = false, g_webCreating = false;

static RECT WebAddrBox(int W) { return { px(150), px(8), W - px(12), WebHdr() - px(8) }; }
static void WebBtnRects(RECT r[4]) {
    int sz = px(32), y = (WebHdr() - sz) / 2;
    for (int i = 0; i < 4; i++) r[i] = { px(8) + i * (sz + px(2)), y, px(8) + i * (sz + px(2)) + sz, y + sz };
}
static RECT WebMarkRect(int i, int W) {
    int cols = (W < px(760)) ? 3 : 4, tw = px(132), th = px(96), gap = px(16);
    int total = cols * tw + (cols - 1) * gap, x0 = (W - total) / 2, y0 = WebHdr() + px(132);
    int c = i % cols, r = i / cols;
    return { x0 + c * (tw + gap), y0 + r * (th + gap), x0 + c * (tw + gap) + tw, y0 + r * (th + gap) + th };
}
static int WebMarkAt(int x, int y) {
    if (g_webPage) return -1;
    int W, H; ClientWHw(g_ww, W, H);
    for (int i = 0; i < (int)g_webMarks.size() && i < 8; i++) { RECT r = WebMarkRect(i, W); if (InRect(r, x, y)) return i; }
    return -1;
}
static bool WebHandAt(int x, int y) {
    RECT br[4]; WebBtnRects(br);
    for (int i = 0; i < 4; i++) if (InRect(br[i], x, y)) return true;
    return WebMarkAt(x, y) >= 0;
}

static wstring WebNormalize(const wstring& in) {   // address or search words -> a URL
    wstring t = Trim(in);
    for (auto& c : t) if (c == L'\r' || c == L'\n') c = L' ';
    if (t.empty()) return L"";
    wstring l = lower(t);
    if (l.rfind(L"http://", 0) == 0 || l.rfind(L"https://", 0) == 0) return t;
    if (t.find(L' ') == wstring::npos) {
        if (l.rfind(L"localhost", 0) == 0) return L"http://" + t;
        size_t slash = l.find(L'/');
        wstring host = l.substr(0, slash);
        size_t colon = host.find(L':');
        if (colon != wstring::npos) host = host.substr(0, colon);
        size_t dot = host.rfind(L'.');
        if (dot != wstring::npos && dot > 0 && host.size() - dot - 1 >= 2) {
            bool alpha = true, ip = true;
            for (size_t i = dot + 1; i < host.size(); i++) if (!iswalpha(host[i])) alpha = false;
            for (wchar_t c : host) if (!iswdigit(c) && c != L'.') ip = false;
            if (alpha || ip) return (ip ? L"http://" : L"https://") + t;
        }
    }
    return ReplaceAll(g_webSearchUrl, L"{q}", UrlEncode(t));
}

// ---- built-in ad / tracker blocking (no extension needed, almost no RAM)
static const wchar_t* kAdHosts[] = {
    L"doubleclick.net", L"googlesyndication.com", L"googleadservices.com", L"adservice.google.com", L"google-analytics.com",
    L"googletagservices.com", L"2mdn.net", L"adnxs.com", L"adsrvr.org", L"advertising.com", L"amazon-adsystem.com", L"criteo.com",
    L"criteo.net", L"taboola.com", L"taboolasyndication.com", L"outbrain.com", L"outbrainimg.com", L"scorecardresearch.com",
    L"quantserve.com", L"rubiconproject.com", L"pubmatic.com", L"openx.net", L"casalemedia.com", L"moatads.com", L"smartadserver.com",
    L"serving-sys.com", L"adform.net", L"bidswitch.net", L"contextweb.com", L"lijit.com", L"sharethrough.com", L"teads.tv",
    L"yieldmo.com", L"zedo.com", L"revcontent.com", L"mgid.com", L"popads.net", L"propellerads.com", L"exoclick.com", L"hotjar.com",
    L"mixpanel.com", L"fullstory.com", L"mouseflow.com", L"crazyegg.com", L"chartbeat.com", L"ads-twitter.com", L"analytics.twitter.com",
    L"px.ads.linkedin.com", L"analytics.tiktok.com", L"bat.bing.com", L"clarity.ms", L"adsymptotic.com", L"tapad.com", L"bluekai.com",
    L"demdex.net", L"everesttech.net", L"krxd.net", L"agkn.com", L"rlcdn.com", L"liadm.com", L"id5-sync.com", L"mathtag.com",
    L"yieldlab.net", L"smaato.net", L"inmobi.com", L"applovin.com", L"ads.yahoo.com", L"adtechus.com", L"media.net", L"gumgum.com",
    L"triplelift.com", L"3lift.com", L"indexww.com", L"adsafeprotected.com", L"doubleverify.com", L"imrworldwide.com", L"pixel.facebook.com",
    L"stats.g.doubleclick.net", L"ad.doubleclick.net", L"pagead2.googlesyndication.com", L"securepubads.g.doubleclick.net",
};
static std::unordered_set<wstring> g_blockSet;
static bool g_blockInit = false;
static wstring HostOf(const wchar_t* uri) {
    const wchar_t* p = wcsstr(uri, L"://");
    if (!p) return L"";
    p += 3;
    const wchar_t* e = p;
    while (*e && *e != L'/' && *e != L':' && *e != L'?' && *e != L'#') e++;
    wstring h(p, (size_t)(e - p));
    for (auto& c : h) c = (wchar_t)towlower(c);
    return h;
}
static bool HostBlocked(const wstring& host) {
    if (!g_blockInit) {
        g_blockInit = true;
        for (auto h : kAdHosts) g_blockSet.insert(h);
        for (auto& h : g_webBlock) g_blockSet.insert(lower(h));
    }
    wstring h = host;
    for (;;) {
        if (g_blockSet.count(h)) return true;
        size_t d = h.find(L'.');
        if (d == wstring::npos) return false;
        h = h.substr(d + 1);
    }
}
static bool IsLocalHost(const wstring& h) {
    return h == L"localhost" || h.rfind(L"127.", 0) == 0 || h.rfind(L"192.168.", 0) == 0 || h.rfind(L"10.", 0) == 0 || h == L"[::1]";
}

#ifdef QP_WEBVIEW
using Microsoft::WRL::ComPtr;
using Microsoft::WRL::Callback;
static const UINT WM_WEB_NAVURL_ = WM_WEB_NAVURL;
static ComPtr<ICoreWebView2Environment> g_wvEnv;
static ComPtr<ICoreWebView2Controller> g_wvCtl;
static ComPtr<ICoreWebView2> g_wv;
static wstring g_webPending;
static int g_extPending = 0;

static bool UriAllowed(const wchar_t* uri) {
    return uri && (_wcsnicmp(uri, L"https://", 8) == 0 || _wcsnicmp(uri, L"http://", 7) == 0 || _wcsnicmp(uri, L"about:blank", 11) == 0);
}
static void WebNavigate(const wstring& text) {
    if (!g_wv || !g_webReady) { g_webPending = text; return; }
    wstring u = WebNormalize(text);
    if (!u.empty()) g_wv->Navigate(u.c_str());
}
static void WebResize(int W, int H) {
    if (!g_wvCtl) return;
    RECT r = { 0, WebHdr() + 1, W, H };
    g_wvCtl->put_Bounds(r);
}
static void WebStartNavigation() {
    if (!g_webReady || !g_webOpen) return;
    wstring u = g_webPending;
    g_webPending.clear();
    if (!u.empty()) { WebNavigate(u); if (g_wvCtl) g_wvCtl->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC); }
    else { SetFocus(g_hAddr); SendMessageW(g_hAddr, EM_SETSEL, 0, -1); }
}
static void WebGo() {
    wstring t = GetEditText(g_hAddr);
    g_addrDirty = false;
    WebNavigate(t);
    if (g_wvCtl && g_webReady) g_wvCtl->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
}
static void WebBack() { if (g_wv && g_webPage) g_wv->GoBack(); }
static void WebFwd() { if (g_wv && g_webPage) g_wv->GoForward(); }
static void WebReload() { if (g_wv && g_webPage) { if (g_webLoading) g_wv->Stop(); else g_wv->Reload(); } }
static void WebHome() {
    if (g_wv) g_wv->Navigate(L"about:blank");
    if (g_wvCtl) g_wvCtl->put_IsVisible(FALSE);
    g_webPage = false; g_webLoading = 0; g_webSecure = false;
    KillTimer(g_ww, T_PROG);
    g_suppress = true; SetWindowTextW(g_hAddr, L""); g_suppress = false;
    SetFocus(g_hAddr);
    InvalidateRect(g_ww, nullptr, FALSE);
}

static void WebFinishSetup() {
    g_webReady = true;
    g_webCreating = false;
    if (g_webOpen) WebStartNavigation();
}

static void WebSetup() {
    if (!g_wvCtl) return;
    g_wvCtl->get_CoreWebView2(&g_wv);
    if (!g_wv) { g_webCreating = false; return; }
    { int W, H; ClientWHw(g_ww, W, H); WebResize(W, H); }
    g_wvCtl->put_IsVisible(FALSE);   // our own start page shows until the first navigation
    {   // match the theme so there is no white flash
        ComPtr<ICoreWebView2Controller2> c2;
        if (SUCCEEDED(g_wvCtl.As(&c2))) {
            COREWEBVIEW2_COLOR col = { 255, GetRValue(g_th->bg), GetGValue(g_th->bg), GetBValue(g_th->bg) };
            c2->put_DefaultBackgroundColor(col);
        }
    }

    ComPtr<ICoreWebView2Settings> st;
    g_wv->get_Settings(&st);
    if (st) {
        st->put_AreDevToolsEnabled(FALSE);
        st->put_IsStatusBarEnabled(TRUE);
        st->put_IsZoomControlEnabled(TRUE);
        st->put_IsWebMessageEnabled(FALSE);
        st->put_AreHostObjectsAllowed(FALSE);
        ComPtr<ICoreWebView2Settings4> s4;
        if (SUCCEEDED(st.As(&s4))) { s4->put_IsPasswordAutosaveEnabled(FALSE); s4->put_IsGeneralAutofillEnabled(FALSE); }
        ComPtr<ICoreWebView2Settings8> s8;
        if (SUCCEEDED(st.As(&s8))) s8->put_IsReputationCheckingRequired(TRUE);   // SmartScreen on
    }
    ComPtr<ICoreWebView2_19> w19;
    if (SUCCEEDED(g_wv.As(&w19))) w19->put_MemoryUsageTargetLevel(COREWEBVIEW2_MEMORY_USAGE_TARGET_LEVEL_LOW);

    ComPtr<ICoreWebView2_13> w13; ComPtr<ICoreWebView2Profile> prof; ComPtr<ICoreWebView2Profile7> p7;
    if (SUCCEEDED(g_wv.As(&w13)) && SUCCEEDED(w13->get_Profile(&prof))) {
        ComPtr<ICoreWebView2Profile3> p3;
        if (SUCCEEDED(prof.As(&p3))) p3->put_PreferredTrackingPreventionLevel(COREWEBVIEW2_TRACKING_PREVENTION_LEVEL_STRICT);
        prof.As(&p7);
    }

    EventRegistrationToken tk;
    g_wv->add_NavigationStarting(Callback<ICoreWebView2NavigationStartingEventHandler>(
        [](ICoreWebView2*, ICoreWebView2NavigationStartingEventArgs* a) -> HRESULT {
            PWSTR uri = nullptr;
            a->get_Uri(&uri);
            wstring u = uri ? uri : L"";
            if (uri) CoTaskMemFree(uri);
            if (!UriAllowed(u.c_str())) { a->put_Cancel(TRUE); return S_OK; }   // only http / https
            if (g_webHttpsOnly && _wcsnicmp(u.c_str(), L"http://", 7) == 0 && !IsLocalHost(HostOf(u.c_str()))) {
                a->put_Cancel(TRUE);   // upgrade to https instead of loading plain http
                PostMessageW(g_ww, WM_WEB_NAVURL_, 0, (LPARAM)new wstring(L"https://" + u.substr(7)));
                return S_OK;
            }
            if (_wcsnicmp(u.c_str(), L"about:blank", 11) != 0) {
                g_webPage = true;
                if (g_wvCtl && g_webOpen) g_wvCtl->put_IsVisible(TRUE);
                g_webLoading = 1; g_progT = 0;
                SetTimer(g_ww, T_PROG, 16, nullptr);
                InvalidateRect(g_ww, nullptr, FALSE);
            }
            return S_OK;
        }).Get(), &tk);
    g_wv->add_NavigationCompleted(Callback<ICoreWebView2NavigationCompletedEventHandler>(
        [](ICoreWebView2*, ICoreWebView2NavigationCompletedEventArgs*) -> HRESULT {
            g_webLoading = 0; KillTimer(g_ww, T_PROG); InvalidateRect(g_ww, nullptr, FALSE); return S_OK;
        }).Get(), &tk);
    g_wv->add_SourceChanged(Callback<ICoreWebView2SourceChangedEventHandler>(
        [](ICoreWebView2*, ICoreWebView2SourceChangedEventArgs*) -> HRESULT {
            PWSTR src = nullptr;
            if (g_wv && SUCCEEDED(g_wv->get_Source(&src)) && src) {
                bool blank = _wcsnicmp(src, L"about:blank", 11) == 0;
                g_webSecure = _wcsnicmp(src, L"https://", 8) == 0;
                if (!blank && !(g_addrDirty && GetFocus() == g_hAddr)) {
                    g_suppress = true; SetWindowTextW(g_hAddr, src); g_suppress = false; g_addrDirty = false;
                }
                CoTaskMemFree(src);
                InvalidateRect(g_ww, nullptr, FALSE);
            }
            return S_OK;
        }).Get(), &tk);
    g_wv->add_NewWindowRequested(Callback<ICoreWebView2NewWindowRequestedEventHandler>(   // no pop-up windows: open in this view
        [](ICoreWebView2*, ICoreWebView2NewWindowRequestedEventArgs* a) -> HRESULT {
            a->put_Handled(TRUE);
            PWSTR uri = nullptr;
            a->get_Uri(&uri);
            if (UriAllowed(uri) && g_wv) g_wv->Navigate(uri);
            if (uri) CoTaskMemFree(uri);
            return S_OK;
        }).Get(), &tk);
    g_wv->add_PermissionRequested(Callback<ICoreWebView2PermissionRequestedEventHandler>(   // camera, mic, location, notifications...: always no
        [](ICoreWebView2*, ICoreWebView2PermissionRequestedEventArgs* a) -> HRESULT {
            a->put_State(COREWEBVIEW2_PERMISSION_STATE_DENY); return S_OK;
        }).Get(), &tk);
    ComPtr<ICoreWebView2_4> w4;
    if (SUCCEEDED(g_wv.As(&w4)))
        w4->add_DownloadStarting(Callback<ICoreWebView2DownloadStartingEventHandler>(
            [](ICoreWebView2*, ICoreWebView2DownloadStartingEventArgs* a) -> HRESULT {
                if (g_webBlockDl) { a->put_Cancel(TRUE); Balloon(L"Quick Palette", L"Downloads are blocked in the palette browser. Use your normal browser for downloads.", NIIF_INFO); }
                return S_OK;
            }).Get(), &tk);
    ComPtr<ICoreWebView2_14> w14;   // never let anyone click through a certificate error
    if (SUCCEEDED(g_wv.As(&w14)))
        w14->add_ServerCertificateErrorDetected(Callback<ICoreWebView2ServerCertificateErrorDetectedEventHandler>(
            [](ICoreWebView2*, ICoreWebView2ServerCertificateErrorDetectedEventArgs* a) -> HRESULT {
                a->put_Action(COREWEBVIEW2_SERVER_CERTIFICATE_ERROR_ACTION_CANCEL); return S_OK;
            }).Get(), &tk);
    ComPtr<ICoreWebView2_18> w18;   // pages may not launch other programs (mailto:, ms-msdt:, ...)
    if (SUCCEEDED(g_wv.As(&w18)))
        w18->add_LaunchingExternalUriScheme(Callback<ICoreWebView2LaunchingExternalUriSchemeEventHandler>(
            [](ICoreWebView2*, ICoreWebView2LaunchingExternalUriSchemeEventArgs* a) -> HRESULT {
                a->put_Cancel(TRUE); return S_OK;
            }).Get(), &tk);
    g_wv->AddWebResourceRequestedFilter(L"*", COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL);
    g_wv->add_WebResourceRequested(Callback<ICoreWebView2WebResourceRequestedEventHandler>(   // ad / tracker blocking
        [](ICoreWebView2*, ICoreWebView2WebResourceRequestedEventArgs* a) -> HRESULT {
            ComPtr<ICoreWebView2WebResourceRequest> req;
            a->get_Request(&req);
            if (!req) return S_OK;
            PWSTR uri = nullptr;
            req->get_Uri(&uri);
            bool blk = uri && HostBlocked(HostOf(uri));
            if (uri) CoTaskMemFree(uri);
            if (blk && g_wvEnv) {
                ComPtr<ICoreWebView2WebResourceResponse> resp;
                if (SUCCEEDED(g_wvEnv->CreateWebResourceResponse(nullptr, 403, L"Blocked", L"", &resp))) a->put_Response(resp.Get());
            }
            return S_OK;
        }).Get(), &tk);
    g_wvCtl->add_AcceleratorKeyPressed(Callback<ICoreWebView2AcceleratorKeyPressedEventHandler>(
        [](ICoreWebView2Controller*, ICoreWebView2AcceleratorKeyPressedEventArgs* a) -> HRESULT {
            COREWEBVIEW2_KEY_EVENT_KIND kind;
            UINT vk = 0;
            a->get_KeyEventKind(&kind);
            a->get_VirtualKey(&vk);
            if (kind == COREWEBVIEW2_KEY_EVENT_KIND_KEY_DOWN || kind == COREWEBVIEW2_KEY_EVENT_KIND_SYSTEM_KEY_DOWN) {
                bool ctrl = GetKeyState(VK_CONTROL) < 0, alt = GetKeyState(VK_MENU) < 0;
                UINT cmd = 0;
                if (ctrl && vk == 'L') cmd = WM_WEB_FOCUSADDR;
                else if (alt && vk == VK_LEFT) cmd = WM_WEB_BACK;
                else if (alt && vk == VK_RIGHT) cmd = WM_WEB_FWD;
                else if (vk == VK_F5 || (ctrl && vk == 'R')) cmd = WM_WEB_RELOAD;
                if (cmd) { PostMessageW(g_ww, cmd, 0, 0); a->put_Handled(TRUE); }
            }
            return S_OK;
        }).Get(), &tk);

    // optional extensions (e.g. uBlock Origin Lite), loaded before the first page
    if (!g_webExt.empty() && p7) {
        g_extPending = (int)g_webExt.size();
        for (auto& e : g_webExt) {
            wstring path = ExpandEnv(e);
            HRESULT hr = p7->AddBrowserExtension(path.c_str(), Callback<ICoreWebView2ProfileAddBrowserExtensionCompletedHandler>(
                [](HRESULT, ICoreWebView2BrowserExtension*) -> HRESULT {
                    if (--g_extPending <= 0) WebFinishSetup();
                    return S_OK;
                }).Get());
            if (FAILED(hr) && --g_extPending <= 0) WebFinishSetup();
        }
        return;
    }
    WebFinishSetup();
}

static void WebCreateController() {
    if (!g_wvEnv || !g_ww) { g_webCreating = false; return; }
    g_wvEnv->CreateCoreWebView2Controller(g_ww, Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
        [](HRESULT r2, ICoreWebView2Controller* ctl) -> HRESULT {
            if (!g_webOpen || !g_ww) { g_webCreating = false; if (ctl) ctl->Close(); return S_OK; }   // window closed meanwhile
            if (FAILED(r2) || !ctl) { g_webCreating = false; Balloon(L"Quick Palette", L"The browser could not start.", NIIF_WARNING); return S_OK; }
            g_wvCtl = ctl;
            WebSetup();
            return S_OK;
        }).Get());
}
static void WebCreate() {
    if (g_wvCtl && g_webReady) { WebStartNavigation(); return; }
    if (g_webCreating) return;   // engine or controller already on its way
    g_webCreating = true;
    if (g_wvEnv) { if (g_webOpen) WebCreateController(); else g_webCreating = false; return; }
    wstring udf = g_dir + L"\\web";
    CreateDirectoryW(udf.c_str(), nullptr);
    auto opts = Microsoft::WRL::Make<CoreWebView2EnvironmentOptions>();
    wstring args = L"--disable-background-networking --disable-sync --disable-breakpad --disable-component-update --no-first-run "
                   L"--disable-default-apps --disable-domain-reliability --disable-features=Translate,OptimizationHints,MediaRouter --disk-cache-size=33554432";
    if (g_webLowRam) args += L" --renderer-process-limit=2 --enable-features=NetworkServiceInProcess --js-flags=--max-old-space-size=512";
    opts->put_AdditionalBrowserArguments(args.c_str());
    if (!g_webExt.empty()) opts->put_AreBrowserExtensionsEnabled(TRUE);
    HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(nullptr, udf.c_str(), opts.Get(),
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [](HRESULT r, ICoreWebView2Environment* env) -> HRESULT {
                if (FAILED(r) || !env) { g_webCreating = false; return S_OK; }
                g_wvEnv = env;
                if (g_webOpen && g_ww) WebCreateController(); else g_webCreating = false;   // warm-up only: no window yet
                return S_OK;
            }).Get());
    if (FAILED(hr)) {
        g_webCreating = false;
        if (g_webOpen) Balloon(L"Quick Palette", L"The Microsoft Edge WebView2 Runtime was not found. Install it to use the built-in browser.", NIIF_WARNING);
    }
}
static void WebPrewarm() {   // start the engine while the user is still typing "w" (no window, no page)
    if (!g_webPrewarm || g_wvEnv || g_webCreating || g_webOpen) return;
    g_webWarm = true;
    WebCreate();
    SetTimer(g_hwnd, T_WEBFREE, 10000, nullptr);   // not used? let it go again
}
static void WebTeardown() {
    g_webReady = false; g_webCreating = false; g_webWarm = false;
    if (g_wvCtl) { g_wvCtl->Close(); g_wvCtl.Reset(); }
    g_wv.Reset(); g_wvEnv.Reset();
    g_webPending.clear();
}
#else
static void WebNavigate(const wstring&) {}
static void WebGo() {}
static void WebBack() {}
static void WebFwd() {}
static void WebReload() {}
static void WebHome() {}
static void WebResize(int, int) {}
static void WebPrewarm() {}
static void WebCreate() { Balloon(L"Quick Palette", L"This build has no browser (the build step could not get the WebView2 SDK).", NIIF_WARNING); }
static void WebTeardown() {}
#endif

static void WebOpenMark(int i) {
    if (i < 0 || i >= (int)g_webMarks.size()) return;
    g_suppress = true; SetWindowTextW(g_hAddr, g_webMarks[i].url.c_str()); g_suppress = false;
    WebNavigate(g_webMarks[i].url);
    InvalidateRect(g_ww, nullptr, FALSE);
}

static LRESULT CALLBACK AddrProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
        case WM_KEYDOWN: {
            bool ctrl = GetKeyState(VK_CONTROL) < 0;
            if (w == VK_RETURN) { PostMessageW(g_ww, WM_WEB_GO, 0, 0); return 0; }
            if (ctrl && w == 'A') { SendMessageW(h, EM_SETSEL, 0, -1); return 0; }
            if (w == VK_F5) { PostMessageW(g_ww, WM_WEB_RELOAD, 0, 0); return 0; }
            break;
        }
        case WM_SYSKEYDOWN:
            if (w == VK_LEFT) { PostMessageW(g_ww, WM_WEB_BACK, 0, 0); return 0; }
            if (w == VK_RIGHT) { PostMessageW(g_ww, WM_WEB_FWD, 0, 0); return 0; }
            if (!g_webPage && w >= '1' && w <= '9') { PostMessageW(g_ww, WM_WEB_MARK, (WPARAM)(w - '1'), 0); return 0; }
            break;
        case WM_CHAR:
            if (w == 13 || w == 27) return 0;
            if (GetKeyState(VK_CONTROL) < 0 && w == 1) return 0;
            break;
        case WM_SYSCHAR: if (w >= '1' && w <= '9') return 0; break;
    }
    return CallWindowProcW(g_oldAddr, h, m, w, l);
}

static void WebLayout() {
    if (!g_webOpen || !g_ww) return;
    int W, H; ClientWHw(g_ww, W, H);
    if (g_hAddr) MoveWindow(g_hAddr, WebEditX(), (WebHdr() - g_addrH) / 2, std::max(60, W - WebEditX() - px(24)), g_addrH, TRUE);
    WebResize(W, H);
    InvalidateRect(g_ww, nullptr, FALSE);
}

static void EnterWeb(const wstring& url) {
    KillTimer(g_hwnd, T_WEBFREE); KillTimer(g_hwnd, T_WIPE);
    g_webWarm = false;
    if (g_webOpen && g_ww) {   // already open: bring it forward
        if (IsIconic(g_ww)) ShowWindow(g_ww, SW_RESTORE);
        SetForegroundWindow(g_ww);
        if (!url.empty()) { g_suppress = true; SetWindowTextW(g_hAddr, url.c_str()); g_suppress = false; WebNavigate(url); }
        else { SetFocus(g_hAddr); SendMessageW(g_hAddr, EM_SETSEL, 0, -1); }
        return;
    }
    g_ww = MakeToolWindow(L"Browser \u2014 Quick Palette", g_webW, g_webH);
    if (!g_ww) return;
    g_webOpen = true; g_addrDirty = false; g_webLoading = 0; g_webPage = false; g_webHover = -1; g_btnHover = -1;
    g_suppress = true;
    g_hAddr = CreateWindowExW(0, L"EDIT", url.c_str(), WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_LEFT,
                              0, 0, 100, g_addrH, g_ww, (HMENU)(INT_PTR)103, g_inst, nullptr);
    g_suppress = false;
    SendMessageW(g_hAddr, WM_SETFONT, (WPARAM)g_fTitle, TRUE);
    SendMessageW(g_hAddr, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, 0);
    SendMessageW(g_hAddr, EM_LIMITTEXT, 2000, 0);
    g_oldAddr = (WNDPROC)SetWindowLongPtrW(g_hAddr, GWLP_WNDPROC, (LONG_PTR)AddrProc);
    WebLayout();
    ShowWindow(g_ww, SW_SHOW);
    SetForegroundWindow(g_ww);
    SetFocus(g_hAddr);
    SendMessageW(g_hAddr, EM_SETSEL, 0, -1);
#ifdef QP_WEBVIEW
    g_webPending = url;
#endif
    WebCreate();
}
static void ExitWeb() {   // the browser window was destroyed
    KillTimer(g_ww, T_PROG);
#ifdef QP_WEBVIEW
    if (g_wvCtl) { g_wvCtl->Close(); g_wvCtl.Reset(); }
    g_wv.Reset();
    g_webReady = false; g_webCreating = false; g_webPending.clear();
#endif
    g_webOpen = false; g_webLoading = 0; g_addrDirty = false; g_webPage = false;
    g_hAddr = nullptr; g_oldAddr = nullptr; g_ww = nullptr;
    // keep the engine warm briefly so the next open is instant, then it exits completely and frees its RAM
    SetTimer(g_hwnd, T_WEBFREE, (UINT)std::max(1, g_webKeepAlive) * 1000, nullptr);
}

static void DrawWeb(HDC dc, int W, int H) {
    const Theme& T = *g_th;
    RECT all = { 0, 0, W, H };
    FillRectC(dc, all, T.bg);
    HBRUSH bd = CreateSolidBrush(T.border); FrameRect(dc, &all, bd); DeleteObject(bd);
    SetBkMode(dc, TRANSPARENT);
    FillRectC(dc, { 1, 1, W - 1, WebHdr() }, T.footerBg);   // toolbar band

    // address pill (the edit control sits inside it)
    RECT pill = WebAddrBox(W);
    FillRound(dc, pill, T.border, px(12));
    RECT pin = { pill.left + 1, pill.top + 1, pill.right - 1, pill.bottom - 1 };
    FillRound(dc, pin, T.chip, px(11));
    wchar_t sec[2] = { (wchar_t)(g_webPage ? (g_webSecure ? 0xE72E : 0xE7BA) : 0xE721), 0 };
    COLORREF secCol = g_webPage ? (g_webSecure ? RGB(52, 168, 83) : RGB(234, 179, 8)) : T.hdr;
    TextAt(dc, g_fIcon, secCol, sec, { pill.left + px(10), pill.top, pill.left + px(34), pill.bottom }, DT_CENTER | DT_VCENTER);

    // toolbar buttons
    RECT br[4]; WebBtnRects(br);
    bool canBack = false, canFwd = false;
#ifdef QP_WEBVIEW
    if (g_wv && g_webPage) { BOOL b = FALSE, f = FALSE; g_wv->get_CanGoBack(&b); g_wv->get_CanGoForward(&f); canBack = b != FALSE; canFwd = f != FALSE; }
#endif
    wchar_t gl[4][2] = { { 0xE72B, 0 }, { 0xE72A, 0 }, { (wchar_t)(g_webLoading ? 0xE711 : 0xE72C), 0 }, { 0xE80F, 0 } };
    bool enabled[4] = { canBack, canFwd, g_webPage, g_webPage };
    for (int i = 0; i < 4; i++) {
        if (i == g_btnHover && enabled[i]) FillRound(dc, br[i], T.chipHot, px(9));
        TextAt(dc, g_fIcon, enabled[i] ? T.text : T.thumb, gl[i], br[i], DT_CENTER | DT_VCENTER);
    }
    FillRectC(dc, { 1, WebHdr(), W - 1, WebHdr() + 1 }, T.divider);
    if (g_webLoading) {   // sliding progress bar
        int seg = W / 3, x = (g_progT * 14) % (W + seg) - seg;
        int l = std::max(1, x), r = std::min(W - 1, x + seg);
        if (r > l) FillRectC(dc, { l, WebHdr() - 2, r, WebHdr() + 1 }, Accent(C_APP));
    }

    if (g_webPage) {
        bool ready =
#ifdef QP_WEBVIEW
            g_wvCtl.Get() != nullptr;
#else
            false;
#endif
        if (!ready) TextAt(dc, g_fTitle, T.dim, L"Starting the browser\u2026", { 0, WebHdr() + 1, W, H }, DT_CENTER | DT_VCENTER);
        return;
    }

    // start page: drawn by us, so it appears instantly
    TextAt(dc, g_fNoteHead, T.text, L"Where to?", { 0, WebHdr() + px(40), W, WebHdr() + px(78) }, DT_CENTER | DT_VCENTER);
    TextAt(dc, g_fSub, T.dim, L"Type an address or search words above and press Enter.   Ads and trackers are blocked.",
           { px(20), WebHdr() + px(80), W - px(20), WebHdr() + px(104) }, DT_CENTER | DT_VCENTER);
    static const COLORREF pal[8] = { RGB(59,130,246), RGB(239,68,68), RGB(245,158,11), RGB(20,184,166), RGB(168,85,247), RGB(236,72,153), RGB(34,197,94), RGB(100,116,139) };
    for (int i = 0; i < (int)g_webMarks.size() && i < 8; i++) {
        RECT r = WebMarkRect(i, W);
        FillRound(dc, r, i == g_webHover ? T.chipHot : T.chip, px(14));
        int d = px(40);
        RECT c = { (r.left + r.right) / 2 - d / 2, r.top + px(12), (r.left + r.right) / 2 + d / 2, r.top + px(12) + d };
        FillRound(dc, c, pal[i % 8], d);
        wstring letter(1, (wchar_t)towupper(g_webMarks[i].name.empty() ? L'?' : g_webMarks[i].name[0]));
        TextAt(dc, g_fNoteHead, RGB(255, 255, 255), letter, c, DT_CENTER | DT_VCENTER);
        TextAt(dc, g_fTitle, T.text, g_webMarks[i].name, { r.left + px(6), c.bottom + px(6), r.right - px(6), r.bottom - px(18) }, DT_CENTER | DT_VCENTER);
        wchar_t hint[8]; swprintf(hint, 8, L"Alt+%d", i + 1);
        TextAt(dc, g_fSub, T.dim, hint, { r.left, r.bottom - px(20), r.right, r.bottom - px(4) }, DT_CENTER | DT_VCENTER);
    }
}
static void WebMouseUp(int x, int y) {
    RECT br[4]; WebBtnRects(br);
    if (InRect(br[0], x, y)) WebBack();
    else if (InRect(br[1], x, y)) WebFwd();
    else if (InRect(br[2], x, y)) WebReload();
    else if (InRect(br[3], x, y)) WebHome();
    else { int m = WebMarkAt(x, y); if (m >= 0) WebOpenMark(m); }
}
static void WebMouseMove(int x, int y) {
    int m = WebMarkAt(x, y), b = -1;
    RECT br[4]; WebBtnRects(br);
    for (int i = 0; i < 4; i++) if (InRect(br[i], x, y)) b = i;
    if (m != g_webHover || b != g_btnHover) { g_webHover = m; g_btnHover = b; InvalidateRect(g_ww, nullptr, FALSE); }
}

// ================================================================== standalone windows (notes + browser)
// Normal top-level windows: own title bar with minimize / maximize / close, resizable, not always-on-top,
// they do not close when they lose focus, and the palette can be opened over them at any time.
static const wchar_t* kToolClass = L"QuickPaletteTool";

static void EnsureDpiFor(HWND h) {
    int d = (int)GetDpiForWindow(h);
    if (d > 0 && d != g_dpi) SetDpi(d, true);
}
static void StyleToolWindow(HWND h) {
    int dk = g_th->dark ? 1 : 0;
    DwmSetWindowAttribute(h, 20, &dk, sizeof dk);   // dark title bar
    if (g_win11) {
        COLORREF cap = g_th->footerBg, txt = g_th->text, brd = g_th->border;
        DwmSetWindowAttribute(h, 35, &cap, sizeof cap);   // title bar color
        DwmSetWindowAttribute(h, 36, &txt, sizeof txt);   // title text color
        DwmSetWindowAttribute(h, 34, &brd, sizeof brd);   // border color
    }
}
static HWND MakeToolWindow(const wchar_t* title, int w96, int h96) {
    POINT pt; GetCursorPos(&pt);
    HMONITOR mon = MonitorFromPoint(pt, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO mi = { sizeof(mi) };
    GetMonitorInfoW(mon, &mi);
    UINT dx = 96, dy = 96;
    GetDpiForMonitor(mon, MDT_EFFECTIVE_DPI, &dx, &dy);
    if ((int)dx != g_dpi) SetDpi((int)dx, true);
    ApplyTheme();
    int ww = (int)(mi.rcWork.right - mi.rcWork.left), wh = (int)(mi.rcWork.bottom - mi.rcWork.top);
    int w = std::min(px(w96), ww - px(40)), h = std::min(px(h96), wh - px(40));
    int x = (int)mi.rcWork.left + (ww - w) / 2, y = (int)mi.rcWork.top + (wh - h) / 2;
    HWND hw = CreateWindowExW(0, kToolClass, title, WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, x, y, w, h, nullptr, nullptr, g_inst, nullptr);
    if (hw) StyleToolWindow(hw);
    return hw;
}

static LRESULT CALLBACK ToolProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    bool isNote = (h == g_nw), isWeb = (h == g_ww);
    switch (msg) {
        case WM_ERASEBKGND: return 1;
        case WM_GETMINMAXINFO: {
            auto* mm = (MINMAXINFO*)lp;
            mm->ptMinTrackSize.x = px(520); mm->ptMinTrackSize.y = px(340);
            return 0;
        }
        case WM_SIZE:
            if (wp != SIZE_MINIMIZED) { EnsureDpiFor(h); if (isNote) NoteLayout(); else if (isWeb) WebLayout(); }
            return 0;
        case WM_DPICHANGED: {
            const RECT* r = (const RECT*)lp;
            SetWindowPos(h, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE);
            SetDpi((int)LOWORD(wp), true);
            if (isNote && g_hEditor) {
                HFONT nf = MakeFont(px(g_noteFontSize), 400, g_noteFont.c_str());
                SendMessageW(g_hEditor, WM_SETFONT, (WPARAM)nf, TRUE);
                if (g_fNote) DeleteObject(g_fNote);
                g_fNote = nf;
            }
            if (isNote) NoteLayout(); else if (isWeb) WebLayout();
            return 0;
        }
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(h, &ps);
            RECT rc; GetClientRect(h, &rc);
            HDC m = CreateCompatibleDC(dc);
            HBITMAP bmp = CreateCompatibleBitmap(dc, rc.right, rc.bottom);
            HGDIOBJ old = SelectObject(m, bmp);
            if (isWeb) DrawWeb(m, rc.right, rc.bottom); else if (isNote) DrawNote(m, rc.right, rc.bottom);
            BitBlt(dc, 0, 0, rc.right, rc.bottom, m, 0, 0, SRCCOPY);
            SelectObject(m, old); DeleteObject(bmp); DeleteDC(m);
            EndPaint(h, &ps);
            return 0;
        }
        case WM_ACTIVATE:
            if (LOWORD(wp) != WA_INACTIVE) {
                if (isNote && g_hEditor && g_noteView != 2) SetFocus(g_hEditor);
                else if (isWeb && g_hAddr && !g_webPage) SetFocus(g_hAddr);
#ifdef QP_WEBVIEW
                else if (isWeb && g_wvCtl && g_webPage) g_wvCtl->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
#endif
            }
            return 0;
        case WM_COMMAND:
            if (HIWORD(wp) == EN_CHANGE) {
                if (LOWORD(wp) == 102) OnNoteChanged();
                else if (LOWORD(wp) == 103 && !g_suppress) g_addrDirty = true;
            }
            return 0;
        case WM_CTLCOLOREDIT: {
            HDC dc = (HDC)wp;
            if ((HWND)lp == g_hEditor) { SetTextColor(dc, g_th->editorText); SetBkColor(dc, g_th->editorBg); return (LRESULT)g_brEd; }
            SetTextColor(dc, g_th->text); SetBkColor(dc, g_th->chip);
            return (LRESULT)g_brChip;   // address bar
        }
        case WM_TIMER:
            if (isNote) {
                if (wp == T_AUTOSAVE) SaveNote();
                else if (wp == T_PREVIEW) { KillTimer(h, T_PREVIEW); RebuildPreview(true); }
            } else if (isWeb && wp == T_PROG) {
                g_progT++;
                RECT hb = { 0, 0, 8000, WebHdr() + 3 };
                InvalidateRect(h, &hb, FALSE);
            }
            return 0;
        case WM_MOUSEMOVE: {
            int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
            TRACKMOUSEEVENT t = { sizeof t, TME_LEAVE, h, 0 };
            TrackMouseEvent(&t);
            if (isNote) NoteMouseMove(x, y); else if (isWeb) WebMouseMove(x, y);
            return 0;
        }
        case WM_MOUSELEAVE:
            if (isNote && g_chipHot != -1) { g_chipHot = -1; InvalidateRect(h, nullptr, FALSE); }
            if (isWeb && (g_webHover != -1 || g_btnHover != -1)) { g_webHover = -1; g_btnHover = -1; InvalidateRect(h, nullptr, FALSE); }
            return 0;
        case WM_LBUTTONDOWN:
            if (isNote) NoteMouseDown(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
            return 0;
        case WM_LBUTTONUP:
            if (isNote) NoteMouseUp(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
            else if (isWeb) WebMouseUp(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
            return 0;
        case WM_MOUSEWHEEL:
            if (isNote) {
                POINT p = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
                ScreenToClient(h, &p);
                NoteWheel(GET_WHEEL_DELTA_WPARAM(wp), p.x, p.y);
            }
            return 0;
        case WM_SETCURSOR:
            if (LOWORD(lp) == HTCLIENT) {
                POINT p; GetCursorPos(&p); ScreenToClient(h, &p);
                bool hand = false;
                if (isWeb) hand = WebHandAt(p.x, p.y);
                else if (isNote) {
                    int lx, ly, W, H; RECT bt[3];
                    if (PvPoint(p.x, p.y, lx, ly)) hand = g_md.HitLink(lx, ly) >= 0 || g_md.HitTask(lx, ly) >= 0;
                    if (!hand && ChipAt(p.x, p.y) >= 0) hand = true;
                    ClientWHw(h, W, H); BtnRects(W, bt);
                    for (int i = 0; i < 3; i++) if (InRect(bt[i], p.x, p.y)) hand = true;
                }
                SetCursor(LoadCursorW(nullptr, hand ? IDC_HAND : IDC_ARROW));
                return TRUE;
            }
            break;
        case WM_KEYDOWN:   // only when the window itself (not a text box) has focus, e.g. preview-only note view
            if (isNote) {
                bool ctrl = GetKeyState(VK_CONTROL) < 0;
                int W, H; ClientWHw(h, W, H);
                RECT pa = PreviewArea(W, H);
                int vh = pa.bottom - pa.top;
                switch (wp) {
                    case VK_DOWN: g_pvScroll += px(40); break;
                    case VK_UP: g_pvScroll -= px(40); break;
                    case VK_NEXT: g_pvScroll += vh * 9 / 10; break;
                    case VK_PRIOR: g_pvScroll -= vh * 9 / 10; break;
                    case VK_HOME: g_pvScroll = 0; break;
                    case VK_END: g_pvScroll = g_pvH; break;
                    case 'P': if (ctrl) { CycleView(); return 0; } break;
                    case 'S': if (ctrl) { SaveNote(); return 0; } break;
                    default: return 0;
                }
                ClampPv();
                InvalidateRect(h, nullptr, FALSE);
                return 0;
            }
            break;
        case WM_WEB_GO: WebGo(); return 0;
        case WM_WEB_BACK: WebBack(); return 0;
        case WM_WEB_FWD: WebFwd(); return 0;
        case WM_WEB_RELOAD: WebReload(); return 0;
        case WM_WEB_FOCUSADDR: if (g_hAddr) { SetFocus(g_hAddr); SendMessageW(g_hAddr, EM_SETSEL, 0, -1); } return 0;
        case WM_WEB_NAVURL: { auto* u = (wstring*)lp; if (u) { WebNavigate(*u); delete u; } return 0; }
        case WM_WEB_MARK: WebOpenMark((int)wp); return 0;
        case WM_CLOSE:
            if (isNote) SaveNote();
            DestroyWindow(h);
            return 0;
        case WM_DESTROY:
            if (isNote) ExitNote(); else if (isWeb) ExitWeb();
            return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

// ================================================================== show / hide / tray / window
static void ForceForeground(HWND h) { SetForegroundWindow(h); BringWindowToTop(h); SetFocus(h); }

static void ShowPalette() {
    if (IsWindowVisible(g_hwnd)) return;
    KillTimer(g_hwnd, T_FREE);
    if (Stamp(g_cfgPath) != g_cfgStamp) {
        LoadConfig(); SyncStartup(); RegisterHotkey();
        FreeFiles();
        if (!FilesCacheFresh()) StartFilesThread();
    }
    ApplyTheme();
    POINT pt; GetCursorPos(&pt);
    HMONITOR mon = MonitorFromPoint(pt, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO mi = { sizeof(mi) };
    GetMonitorInfoW(mon, &mi);
    g_work = mi.rcWork;
    UINT dx = 96, dy = 96;
    GetDpiForMonitor(mon, MDT_EFFECTIVE_DPI, &dx, &dy);
    SetDpi((int)dx, g_compact != g_lastCompact);

    g_noteMode = false;
    g_suppress = true; SetWindowTextW(g_hQuery, L""); g_suppress = false;
    g_query.clear(); g_qLines = 1;

    int w = PalW();
    if (g_atCursor) { g_anchor.x = pt.x - w / 2; g_anchor.y = pt.y + px(10); }
    else {
        g_anchor.x = g_work.left + ((g_work.right - g_work.left) - w) / 2;
        g_anchor.y = g_work.top + (g_work.bottom - g_work.top) * 16 / 100;
    }
    g_lastSig = -1;
    Refresh();
    KillTimer(g_hwnd, T_ANIM);
    if (g_animations) { SetLayeredWindowAttributes(g_hwnd, 0, 0, LWA_ALPHA); g_animStart = GetTickCount64(); }
    else SetLayeredWindowAttributes(g_hwnd, 0, 255, LWA_ALPHA);
    PlaceWindow(w, PalH(), true);   // always clamped inside the screen
    if (g_animations) SetTimer(g_hwnd, T_ANIM, 10, nullptr);
    ForceForeground(g_hwnd);
    SetFocus(g_hQuery);
}

static void HidePalette() {
    if (!IsWindowVisible(g_hwnd)) return;
    ShowWindow(g_hwnd, SW_HIDE);
    SetTimer(g_hwnd, T_TRIM, 800, nullptr);
    SetTimer(g_hwnd, T_FREE, 180000, nullptr);   // drop the file index from RAM after 3 idle minutes
}

static void AddTray() {
    ZeroMemory(&g_nid, sizeof g_nid);
    g_nid.cbSize = sizeof g_nid; g_nid.hWnd = g_hwnd; g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAY;
    g_nid.hIcon = (HICON)LoadImageW(g_inst, MAKEINTRESOURCEW(1), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
    if (!g_nid.hIcon) g_nid.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wcscpy_s(g_nid.szTip, L"Quick Palette");
    Shell_NotifyIconW(NIM_ADD, &g_nid);
}
static void Balloon(const wchar_t* title, const wchar_t* msg, DWORD icon) {
    NOTIFYICONDATAW n = g_nid;
    n.uFlags = NIF_INFO; n.dwInfoFlags = icon;
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
    else if (c == 2) OpenFile(g_cfgPath);
    else if (c == 3) StartAppsThread();
    else if (c == 4) DestroyWindow(g_hwnd);
}

static LRESULT CALLBACK WndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_HOTKEY:
            if (IsWindowVisible(h)) HidePalette(); else ShowPalette();
            return 0;
        case WM_SHOWME: ShowPalette(); return 0;
        case WM_NOTE_CLOSE: HidePalette(); return 0;
        case WM_WEB_GO: WebGo(); return 0;
        case WM_WEB_NAVURL: { auto* u = (wstring*)lp; if (u) { WebNavigate(*u); delete u; } return 0; }
        case WM_WEB_MARK: WebOpenMark((int)wp); return 0;
        case WM_WEB_BACK: WebBack(); return 0;
        case WM_WEB_FWD: WebFwd(); return 0;
        case WM_WEB_RELOAD: WebReload(); return 0;
        case WM_WEB_FOCUSADDR: SetFocus(g_hQuery); SendMessageW(g_hQuery, EM_SETSEL, 0, -1); return 0;
        case WM_APPS_READY: {
            auto* v = (vector<AppEntry>*)lp;
            g_apps = std::move(*v); delete v;
            g_appsLoaded = true; g_catalogDirty = true;
            if (IsWindowVisible(h) && !g_noteMode) Refresh();
            return 0;
        }
        case WM_FILES_READY: {
            auto* p = (FilePool*)lp;
            g_files = std::move(*p); delete p;
            g_filesLoaded = true;
            return 0;
        }
        case WM_TRAY:
            if (LOWORD(lp) == WM_LBUTTONUP) ShowPalette();
            else if (LOWORD(lp) == WM_RBUTTONUP) TrayMenu();
            return 0;
        case WM_COMMAND:
            if (HIWORD(wp) == EN_CHANGE) {
                if (LOWORD(wp) == 101) OnQueryChanged();
                else if (LOWORD(wp) == 102) OnNoteChanged();
            }
            return 0;
        case WM_CTLCOLOREDIT: {
            HDC dc = (HDC)wp;
            if ((HWND)lp == g_hEditor) { SetTextColor(dc, g_th->editorText); SetBkColor(dc, g_th->editorBg); return (LRESULT)g_brEd; }
            if (g_webMode && (HWND)lp == g_hQuery) { SetTextColor(dc, g_th->text); SetBkColor(dc, g_th->chip); return (LRESULT)g_brChip; }
            SetTextColor(dc, g_th->text); SetBkColor(dc, g_th->bg);
            return (LRESULT)g_brBg;
        }
        case WM_TIMER:
            if (wp == T_BOOT) {
                KillTimer(h, T_BOOT);
                if (g_boot) SetPriorityClass(GetCurrentProcess(), PROCESS_MODE_BACKGROUND_END);
                FILETIME now; GetSystemTimeAsFileTime(&now);
                ULONGLONG n = ((ULONGLONG)now.dwHighDateTime << 32) | now.dwLowDateTime, s = Stamp(g_appsPath);
                if (s == 0 || n - s > 864000000000ULL) StartAppsThread();   // older than 24h
                if (!FilesCacheFresh()) StartFilesThread();
                if (g_webPrivate) SetTimer(h, T_WIPE, 3000, nullptr);   // clean up any leftover browser data from last time
                SetTimer(h, T_TRIM, 1500, nullptr);
            } else if (wp == T_TRIM) {
                KillTimer(h, T_TRIM);
                if (!IsWindowVisible(h)) { FreeBackBuffer(); SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1); }
            } else if (wp == T_AUTOSAVE) {
                SaveNote();
            } else if (wp == T_PREVIEW) {
                KillTimer(h, T_PREVIEW);
                RebuildPreview(true);
            } else if (wp == T_FREE) {
                KillTimer(h, T_FREE);
                if (!IsWindowVisible(h)) { FreeFiles(); SetTimer(h, T_TRIM, 500, nullptr); }
            } else if (wp == T_ANIM) {
                double t = std::min(1.0, (double)(GetTickCount64() - g_animStart) / 140.0);
                double e = 1.0 - (1.0 - t) * (1.0 - t) * (1.0 - t);
                SetLayeredWindowAttributes(h, 0, (BYTE)(255.0 * e), LWA_ALPHA);
                SetWindowPos(h, nullptr, g_winX, g_winY + (int)((1.0 - e) * px(12)), 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
                if (t >= 1.0) KillTimer(h, T_ANIM);
            } else if (wp == T_SEL) {
                double tg = SelTargetY(), d = tg - g_selY;
                if (std::fabs(d) < 1.5) { g_selY = tg; KillTimer(h, T_SEL); } else g_selY += d * 0.4;
                InvalidateRect(h, nullptr, FALSE);
            } else if (wp == T_PREWARM) {
                KillTimer(h, T_PREWARM);
                if (!g_webOpen && !g_webTrigger.empty() && lower(Trim(GetEditText(g_hQuery))) == lower(g_webTrigger)) WebPrewarm();
            } else if (wp == T_PROG) {
                g_progT++;
                RECT hb = { 0, 0, 4000, WebHdr() + 3 };
                InvalidateRect(h, &hb, FALSE);
            } else if (wp == T_WEBFREE) {
                KillTimer(h, T_WEBFREE);
                if (!g_webOpen) { WebTeardown(); if (g_webPrivate) SetTimer(h, T_WIPE, 4000, nullptr); }
            } else if (wp == T_WIPE) {
                KillTimer(h, T_WIPE);
                if (!g_webOpen && g_webPrivate) WipeDir(g_dir + L"\\web");
            } else if (wp == T_QUERY) {
                KillTimer(h, T_QUERY);
                if (!g_noteMode) Refresh();
            } else if (wp >= 100) {   // user timers / reminders
                KillTimer(h, (UINT_PTR)wp);
                for (size_t i = 0; i < g_timers.size(); i++)
                    if (g_timers[i].id == wp) {
                        Balloon(L"Timer finished", g_timers[i].label.c_str(), NIIF_INFO);
                        MessageBeep(MB_ICONASTERISK);
                        g_timers.erase(g_timers.begin() + (std::ptrdiff_t)i);
                        break;
                    }
            }
            return 0;
        case WM_ACTIVATE:
            if (LOWORD(wp) == WA_INACTIVE) { if (!g_menuOpen && g_hideOnFocusLoss) HidePalette(); }
            else if (IsWindowVisible(h)) {
                if (g_webMode) {}   // keep focus wherever the user put it (address bar or page)
                else if (g_noteMode) { if (g_hEditor && g_noteView != 2) SetFocus(g_hEditor); }
                else if (g_hQuery) SetFocus(g_hQuery);
            }
            return 0;
        case WM_QUERYENDSESSION: return TRUE;
        case WM_ENDSESSION: if (wp) SaveNote(); return 0;
        case WM_ERASEBKGND: return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(h, &ps);
            RECT rc; GetClientRect(h, &rc);
            HDC m = GetBackBuffer(dc, rc.right, rc.bottom);
            if (g_webMode) DrawWeb(m, rc.right, rc.bottom); else if (g_noteMode) DrawNote(m, rc.right, rc.bottom); else DrawPalette(m, rc.right, rc.bottom);
            BitBlt(dc, 0, 0, rc.right, rc.bottom, m, 0, 0, SRCCOPY);
            EndPaint(h, &ps);
            return 0;
        }
        case WM_SETCURSOR:
            if (LOWORD(lp) == HTCLIENT) {
                POINT p; GetCursorPos(&p); ScreenToClient(h, &p);
                bool hand = false;
                if (g_webMode) {
                    hand = WebHandAt(p.x, p.y);
                } else if (g_noteMode) {
                    int lx, ly, W, H; RECT bt[3];
                    if (PvPoint(p.x, p.y, lx, ly)) hand = g_md.HitLink(lx, ly) >= 0 || g_md.HitTask(lx, ly) >= 0;
                    if (!hand && ChipAt(p.x, p.y) >= 0) hand = true;
                    ClientWH(W, H); BtnRects(W, bt);
                    for (int i = 0; i < 3; i++) if (InRect(bt[i], p.x, p.y)) hand = true;
                } else hand = HitTest(p.y) >= 0;
                SetCursor(LoadCursorW(nullptr, hand ? IDC_HAND : IDC_ARROW));
                return TRUE;
            }
            break;
        case WM_KEYDOWN:   // only reached when the main window (not a text box) has focus, e.g. preview-only note view
            if (g_noteMode) {
                bool ctrl = GetKeyState(VK_CONTROL) < 0;
                int W, H; ClientWH(W, H);
                RECT pa = PreviewArea(W, H);
                int vh = pa.bottom - pa.top;
                switch (wp) {
                    case VK_ESCAPE: HidePalette(); return 0;
                    case VK_DOWN: g_pvScroll += px(40); break;
                    case VK_UP: g_pvScroll -= px(40); break;
                    case VK_NEXT: g_pvScroll += vh * 9 / 10; break;
                    case VK_PRIOR: g_pvScroll -= vh * 9 / 10; break;
                    case VK_HOME: g_pvScroll = 0; break;
                    case VK_END: g_pvScroll = g_pvH; break;
                    case 'P': if (ctrl) { CycleView(); return 0; } break;
                    case 'S': if (ctrl) { SaveNote(); return 0; } break;
                    default: return 0;
                }
                ClampPv();
                InvalidateRect(h, nullptr, FALSE);
                return 0;
            }
            break;
        case WM_MOUSEMOVE: {
            int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
            TRACKMOUSEEVENT t = { sizeof t, TME_LEAVE, h, 0 };
            TrackMouseEvent(&t);
            if (g_webMode) { WebMouseMove(x, y); return 0; }
            if (g_noteMode) { NoteMouseMove(x, y); return 0; }
            if (g_drag == 1) {
                RECT tr, th;
                if (ListThumb(tr, th)) {
                    int thH = th.bottom - th.top;
                    g_scroll = g_dragScroll + (int)((long long)(y - g_dragY) * (ContentH() - ListH()) / std::max<int>(1, (tr.bottom - tr.top) - thH));
                    ClampListScroll();
                    SnapSel();
                    InvalidateRect(h, nullptr, FALSE);
                }
                return 0;
            }
            int i = HitTest(y);
            if (i != g_hover) { g_hover = i; InvalidateRect(h, nullptr, FALSE); }
            return 0;
        }
        case WM_MOUSELEAVE:
            if (g_hover != -1) { g_hover = -1; InvalidateRect(h, nullptr, FALSE); }
            if (g_chipHot != -1) { g_chipHot = -1; InvalidateRect(h, nullptr, FALSE); }
            return 0;
        case WM_MOUSEWHEEL: {
            int d = GET_WHEEL_DELTA_WPARAM(wp);
            POINT p = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
            ScreenToClient(h, &p);
            if (g_noteMode) NoteWheel(d, p.x, p.y);
            else { g_scroll -= d * ItemH() / WHEEL_DELTA; ClampListScroll(); SnapSel(); InvalidateRect(h, nullptr, FALSE); }
            return 0;
        }
        case WM_LBUTTONDOWN: {
            int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
            if (g_noteMode) { NoteMouseDown(x, y); return 0; }
            RECT tr, th;
            if (ListThumb(tr, th) && x >= tr.left - px(5) && x < tr.right + px(5) && y >= tr.top && y < tr.bottom) {
                g_drag = 1; g_dragY = y; g_dragScroll = g_scroll; SetCapture(h);
            }
            return 0;
        }
        case WM_LBUTTONUP: {
            int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
            if (g_webMode) { WebMouseUp(x, y); return 0; }
            if (g_noteMode) { NoteMouseUp(x, y); return 0; }
            if (g_drag) { g_drag = 0; ReleaseCapture(); return 0; }
            int i = HitTest(y);
            if (i >= 0) { g_sel = i; RunSelected(); }
            return 0;
        }
        case WM_RBUTTONUP: {
            if (g_noteMode) return 0;
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
                if (g_hQuery) SetFocus(g_hQuery);
            }
            return 0;
        }
        case WM_DESTROY:
            SaveNote();
            if (g_ww) DestroyWindow(g_ww);
            if (g_nw) DestroyWindow(g_nw);
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
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    PWSTR roam = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &roam))) { g_dir = wstring(roam) + L"\\QuickPalette"; CoTaskMemFree(roam); }
    else g_dir = L".";
    CreateDirectoryW(g_dir.c_str(), nullptr);
    g_cfgPath = g_dir + L"\\config.ini";
    g_usagePath = g_dir + L"\\usage.txt";
    g_hiddenPath = g_dir + L"\\hidden.txt";
    g_appsPath = g_dir + L"\\apps.txt";
    g_filesPath = g_dir + L"\\files.txt";
    g_pinsPath = g_dir + L"\\pins.txt";
    SetUnhandledExceptionFilter(OnCrash);

    HANDLE mtx = CreateMutexW(nullptr, TRUE, L"Local\\QuickPalette.v2");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {   // already running: just open the palette
        HWND w = FindWindowW(kClass, nullptr);
        if (w) PostMessageW(w, WM_SHOWME, 0, 0);
        return 0;
    }

    g_boot = cmd && wcsstr(cmd, L"--boot") != nullptr;
    if (g_boot) SetPriorityClass(GetCurrentProcess(), PROCESS_MODE_BACKGROUND_BEGIN);   // low CPU + low disk priority while Windows boots

    LoadConfig();
    SyncStartup();
    SetDpi(96, true);

    WNDCLASSEXW wc = { sizeof wc };
    wc.style = CS_DROPSHADOW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(1));
    wc.hIconSm = (HICON)LoadImageW(inst, MAKEINTRESOURCEW(1), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
    wc.lpszClassName = kClass;
    RegisterClassExW(&wc);

    WNDCLASSEXW tc = { sizeof tc };   // class for the standalone note / browser windows
    tc.style = CS_HREDRAW | CS_VREDRAW;
    tc.lpfnWndProc = ToolProc;
    tc.hInstance = inst;
    tc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    tc.hIcon = wc.hIcon; tc.hIconSm = wc.hIconSm;
    tc.lpszClassName = kToolClass;
    RegisterClassExW(&tc);

    g_hwnd = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_LAYERED, kClass, L"Quick Palette", WS_POPUP | WS_CLIPCHILDREN, 0, 0, px(640), px(60), nullptr, nullptr, inst, nullptr);
    if (!g_hwnd) return 1;
    SetLayeredWindowAttributes(g_hwnd, 0, 255, LWA_ALPHA);
    int pref = 2;   // rounded corners (only succeeds on Windows 11)
    g_win11 = SUCCEEDED(DwmSetWindowAttribute(g_hwnd, 33, &pref, sizeof pref));
    ApplyTheme();
    CreateQueryEdit();

    g_wmTaskbar = RegisterWindowMessageW(L"TaskbarCreated");
    AddTray();
    if (!RegisterHotkey())
        Balloon(L"Quick Palette", L"The shortcut in config.ini is invalid or used by another app. Edit it via the tray icon > Edit settings.", NIIF_WARNING);

    if (g_firstRun) {   // ask once, explicitly - nothing is added to startup without a Yes
        int r = MessageBoxW(nullptr,
            L"Start Quick Palette automatically when Windows starts?\n\nIt waits about 45 seconds and runs at low priority, so it will not slow your startup. "
            L"You can change this any time: type \"start with windows\" in the palette.",
            L"Quick Palette", MB_YESNO | MB_ICONQUESTION | MB_TOPMOST);
        SetConfigValue(L"startup", r == IDYES ? L"1" : L"0");
    }

    SetTimer(g_hwnd, T_BOOT, g_boot ? 45000 : 2500, nullptr);   // heavy work waits

    MSG m;
    while (GetMessageW(&m, nullptr, 0, 0) > 0) { TranslateMessage(&m); DispatchMessageW(&m); }
    if (mtx) CloseHandle(mtx);
    return 0;
}
