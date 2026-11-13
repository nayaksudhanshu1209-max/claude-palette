// ui.h - the palette window, shared drawing helpers, plumbing for the note/browser windows, tray and entry point.
// Notes live in note.h, the browser in web.h.
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

// ------------------------------------------------------------------ globals
static HWND g_hQuery = nullptr;
static WNDPROC g_oldQuery = nullptr;
static HBRUSH g_brBg = nullptr;
static HFONT g_fChip = nullptr;
static bool g_win11 = false, g_suppress = false, g_facesChecked = false, g_lastCompact = false;
static int g_lineH = 28, g_qLines = 1;
static RECT g_work = { 0, 0, 1920, 1080 };
static POINT g_anchor = { 0, 0 };
static double g_selY = -10000;               // animated highlight position
static ULONGLONG g_animStart = 0;
static int g_lastSig = -1, g_drag = 0, g_dragY = 0, g_dragScroll = 0;
static HDC g_bbDC = nullptr; static HBITMAP g_bbBmp = nullptr, g_bbOld = nullptr; static int g_bbW = 0, g_bbH = 0;
static wchar_t g_uiFace[64] = L"Segoe UI", g_iconFace[64] = L"Segoe MDL2 Assets";
static const UINT WM_PALETTE_PREFILL = WM_APP + 5;
static bool g_queryPending = false;

static void HidePalette();
static void EnterNote(int idx);
static void WebOpen(const wstring& text);
static void WebEnsure();
static void Balloon(const wchar_t* title, const wchar_t* msg, DWORD icon);

// ------------------------------------------------------------------ helpers
static wstring GetEditText(HWND h) {
    int n = GetWindowTextLengthW(h);
    wstring s((size_t)n, L'\0');
    if (n) GetWindowTextW(h, &s[0], n + 1);
    return s;
}
static HBRUSH Brush(COLORREF c) {   // a few solid brushes, created once and reused (no per-frame allocations)
    static std::unordered_map<COLORREF, HBRUSH> cache;
    auto it = cache.find(c);
    if (it != cache.end()) return it->second;
    HBRUSH b = CreateSolidBrush(c);
    cache[c] = b;
    return b;
}
static COLORREF Mix(COLORREF a, COLORREF b, int pct) {
    return RGB(GetRValue(a) + (GetRValue(b) - GetRValue(a)) * pct / 100,
               GetGValue(a) + (GetGValue(b) - GetGValue(a)) * pct / 100,
               GetBValue(a) + (GetBValue(b) - GetBValue(a)) * pct / 100);
}
static void FillRound(HDC dc, RECT r, COLORREF c, int rad) {
    HGDIOBJ ob = SelectObject(dc, Brush(c)), op = SelectObject(dc, GetStockObject(NULL_PEN));
    RoundRect(dc, r.left, r.top, r.right + 1, r.bottom + 1, rad, rad);
    SelectObject(dc, ob); SelectObject(dc, op);
}
static void FillRectC(HDC dc, RECT r, COLORREF c) { FillRect(dc, &r, Brush(c)); }
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
    g_brBg = CreateSolidBrush(t->bg);
    if (g_win11 && g_hwnd) {
        COLORREF bc = t->border; DwmSetWindowAttribute(g_hwnd, 34, &bc, sizeof bc);
        int dk = dark ? 1 : 0; DwmSetWindowAttribute(g_hwnd, 20, &dk, sizeof dk);
    }
}

static void SetDpi(int dpi, bool force = false) {
    if (!force && dpi == g_dpi && g_fTitle) return;
    g_dpi = dpi;
    if (!g_facesChecked) {
        g_facesChecked = true;
        if (FaceExists(L"Segoe UI Variable Text")) wcscpy_s(g_uiFace, L"Segoe UI Variable Text");
        if (FaceExists(L"Segoe Fluent Icons")) wcscpy_s(g_iconFace, L"Segoe Fluent Icons");
    }
    for (HFONT* f : { &g_fSearch, &g_fTitle, &g_fSub, &g_fHdr, &g_fIcon, &g_fChip })
        if (*f) { DeleteObject(*f); *f = nullptr; }
    g_fSearch = MakeFont(px(g_compact ? 18 : 22), 400, g_uiFace);
    g_fTitle = MakeFont(px(g_compact ? 14 : 15), 400, g_uiFace);
    g_fSub = MakeFont(px(12), 400, g_uiFace);
    g_fHdr = MakeFont(px(12), 600, g_uiFace);
    g_fIcon = MakeFont(px(g_compact ? 15 : 17), 400, g_iconFace);
    g_fChip = MakeFont(px(12), 400, L"Consolas");
    HDC dc = GetDC(nullptr);
    HGDIOBJ o = SelectObject(dc, g_fSearch);
    TEXTMETRICW tm; GetTextMetricsW(dc, &tm);
    g_lineH = tm.tmHeight + tm.tmExternalLeading;
    SelectObject(dc, o); ReleaseDC(nullptr, dc);
    g_lastCompact = g_compact;
    if (g_hQuery) SendMessageW(g_hQuery, WM_SETFONT, (WPARAM)g_fSearch, TRUE);
}

// ------------------------------------------------------------------ palette metrics
static int SearchPadV() { return px(g_compact ? 9 : 14); }
static int SearchH() { return std::max<int>(SearchPadV() * 2 + g_qLines * g_lineH, px(g_compact ? 44 : 60)); }
static int ItemH() { return px(g_compact ? 38 : 50); }
static int HdrH() { return px(g_compact ? 24 : 28); }
static int FootH() { return g_showFooter ? px(g_compact ? 24 : 28) : 0; }
static int PalW() { return std::min<int>(px(g_width), (int)(g_work.right - g_work.left) - px(8)); }
static int ContentH() { int h = 0; for (auto& r : g_rows) h += r.header ? HdrH() : ItemH(); return h; }
static int ListH() { return std::min<int>(ContentH(), px(g_listMax)); }
static int ListTop() { return SearchH() + 1 + px(4); }
static int PalH() { int h = SearchH(); if (!g_rows.empty()) h += 1 + px(4) + ListH() + px(g_showFooter ? 6 : 8) + FootH(); return h; }

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
    if (g_animations) SetTimer(g_hwnd, T_SEL, 16, nullptr); else SnapSel();
    RECT r = { 0, ListTop(), PalW(), ListTop() + ListH() };
    InvalidateRect(g_hwnd, &r, FALSE);
}
static void FreeBackBuffer() {
    if (!g_bbDC) return;
    SelectObject(g_bbDC, g_bbOld); DeleteObject(g_bbBmp); DeleteDC(g_bbDC);
    g_bbDC = nullptr; g_bbBmp = nullptr; g_bbW = g_bbH = 0;
}
static HDC GetBackBuffer(HDC ref, int w, int h) {   // one reusable off-screen bitmap
    if (g_bbDC && g_bbW == w && g_bbH == h) return g_bbDC;
    FreeBackBuffer();
    g_bbDC = CreateCompatibleDC(ref);
    g_bbBmp = CreateCompatibleBitmap(ref, w, h);
    g_bbOld = (HBITMAP)SelectObject(g_bbDC, g_bbBmp);
    g_bbW = w; g_bbH = h;
    return g_bbDC;
}

static void PlaceWindow(int w, int h, bool show) {
    int x = g_anchor.x, y = g_anchor.y;
    if (x + w > g_work.right) x = g_work.right - w;      // never cut off by the screen edge
    if (x < g_work.left) x = g_work.left;
    if (y + h > g_work.bottom) y = g_work.bottom - h;
    if (y < g_work.top) y = g_work.top;
    SetWindowPos(g_hwnd, show ? HWND_TOPMOST : nullptr, x, y, w, h, show ? SWP_SHOWWINDOW : (SWP_NOZORDER | SWP_NOACTIVATE));
}

static void LayoutChildren() {
    int eh = g_qLines * g_lineH;
    MoveWindow(g_hQuery, px(52), (SearchH() - eh) / 2, PalW() - px(52) - px(20), eh, TRUE);
}
static void ApplyLayout() {
    int w = PalW(), h = PalH();
    int sig = w * 7919 + h * 131 + g_qLines * 17;
    if (sig != g_lastSig) {   // only touch the window when its size really changed
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
    KillTimer(g_hwnd, T_QUERY); g_queryPending = false;
    g_results = RunQuery(g_query);
    BuildRows();
    g_sel = g_results.empty() ? -1 : 0;
    g_hover = -1; g_scroll = 0;
    ApplyLayout();
    SnapSel();
}
static void ClampListScroll() { g_scroll = std::max<int>(0, std::min<int>(g_scroll, std::max<int>(0, ContentH() - ListH()))); }
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
    if (wrap) s = (s % n + n) % n; else s = std::max<int>(0, std::min<int>(n - 1, s));
    g_sel = s;
    EnsureVisible();
    StartSelAnim();
}
static void RunSelected() {
    if (g_sel < 0 || g_sel >= (int)g_results.size()) return;
    Item it = g_results[g_sel];
    if (it.kind == K_BROWSE) { WebOpen(it.a); HidePalette(); return; }   // open first (we still own the foreground), then close the palette
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
    if (!g_results.empty()) { g_sel = std::min<int>(keep, (int)g_results.size() - 1); EnsureVisible(); SnapSel(); }
}

// ------------------------------------------------------------------ query box (a real multi-line EDIT control that grows)
static void OnQueryChanged() {
    if (g_suppress) return;
    wstring raw = GetEditText(g_hQuery);
    wstring low = lower(raw);
    for (size_t i = 0; i < g_notes.size(); i++)
        if (low == lower(g_notes[i].trig) + L" ") { EnterNote((int)i); HidePalette(); return; }   // "n " opens the note window
    bool heavy = false;
    size_t b = low.find_first_not_of(L' ');
    if (b != wstring::npos) {
        size_t e = low.find(L' ', b);
        if (e != wstring::npos) {
            wstring w0 = low.substr(b, e - b);
            heavy = (w0 == lower(g_fileTrigger));
            if (!g_webTrigger.empty() && w0 == lower(g_webTrigger)) WebEnsure();   // "w ": start the browser in the background now
        }
    }
    g_query = raw;
    for (auto& c : g_query) if (c == L'\r' || c == L'\n') c = L' ';
    int lines = (int)SendMessageW(g_hQuery, EM_GETLINECOUNT, 0, 0);
    lines = std::max<int>(1, std::min<int>(6, lines));
    bool changed = lines != g_qLines;
    g_qLines = lines;
    if (changed) { ApplyLayout(); SendMessageW(g_hQuery, EM_LINESCROLL, 0, -100); }
    g_queryPending = true;
    SetTimer(g_hwnd, T_QUERY, heavy ? 45 : 22, nullptr);   // results follow a moment after the last keystroke: typing never waits for a search
}

static LRESULT CALLBACK QueryProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
        case WM_KEYDOWN: {
            bool ctrl = GetKeyState(VK_CONTROL) < 0, shift = GetKeyState(VK_SHIFT) < 0;
            switch (w) {
                case VK_ESCAPE: HidePalette(); return 0;
                case VK_DOWN: MoveSel(1); return 0;
                case VK_UP: MoveSel(-1); return 0;
                case VK_NEXT: MoveSel(5, false); return 0;
                case VK_PRIOR: MoveSel(-5, false); return 0;
                case VK_TAB: MoveSel(shift ? -1 : 1); return 0;
                case VK_RETURN:
                    if (shift) break;
                    if (g_queryPending) Refresh();   // results are up to date before running
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
                        if (!g_results.empty()) { g_sel = std::min<int>(keep, (int)g_results.size() - 1); EnsureVisible(); SnapSel(); }
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
        case WM_MOUSEWHEEL: SendMessageW(g_hwnd, WM_MOUSEWHEEL, w, l); return 0;
        case WM_PAINT: {
            LRESULT r = CallWindowProcW(g_oldQuery, h, m, w, l);
            if (GetWindowTextLengthW(h) == 0) {
                HDC dc = GetDC(h);
                RECT rc; GetClientRect(h, &rc);
                rc.left += px(2);
                SetBkMode(dc, TRANSPARENT);
                TextAt(dc, g_fSearch, g_th->placeholder, L"Search apps, files, settings\u2026   f report \u00B7 g cats \u00B7 n notes \u00B7 w web", rc, DT_VCENTER);
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

// ------------------------------------------------------------------ list scrollbar (thin, draggable)
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
    FrameRect(dc, &all, Brush(T.border));
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
                FillRound(dc, tile, Mix(sel ? T.sel : T.bg, ac, T.dark ? 24 : 18), px(9));
                wchar_t gl[2] = { it.glyph, 0 };
                TextAt(dc, g_fIcon, ac, gl, tile, DT_CENTER | DT_VCENTER);
                int tx = tile.right + px(12);
                int right = W - px(ContentH() > lh ? 26 : 20);
                if (it.pinned) {
                    wchar_t pg[2] = { 0xE718, 0 };
                    TextAt(dc, g_fIcon, T.hdr, pg, { right - px(18), y, right, y + h }, DT_RIGHT | DT_VCENTER);
                    right -= px(22);
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
        TextAt(dc, g_fSub, T.footerText, L"\u21B5 Open    Ctrl+D Pin    Ctrl+\u21B5 Show in folder    Ctrl+H Hide    Esc Close", { px(20), fy, W - px(20), H - 1 }, DT_VCENTER);
    }
}

// ------------------------------------------------------------------ standalone windows (notes, browser): borderless, no title bar
// Normal top-level windows (taskbar button, not always-on-top). Esc / Ctrl+W close them; focus loss does not.
static const wchar_t* kToolClass = L"QuickPaletteTool";

static void EnsureDpiFor(HWND h) {
    int d = (int)GetDpiForWindow(h);
    if (d > 0 && d != g_dpi) SetDpi(d, true);
}
static bool ToolFrameMsg(HWND h, UINT msg, WPARAM wp, LPARAM lp, LRESULT& out) {
    switch (msg) {
        case WM_NCCALCSIZE:   // the whole window is client area: no title bar, no frame
            if (wp) {
                if (IsZoomed(h)) {   // maximized: stay inside the work area
                    auto* p = (NCCALCSIZE_PARAMS*)lp;
                    UINT dpi = GetDpiForWindow(h);
                    int fx = GetSystemMetricsForDpi(SM_CXFRAME, dpi) + GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);
                    int fy = GetSystemMetricsForDpi(SM_CYFRAME, dpi) + GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);
                    p->rgrc[0].left += fx; p->rgrc[0].right -= fx; p->rgrc[0].top += fy; p->rgrc[0].bottom -= fy;
                }
                out = 0;
                return true;
            }
            break;
        case WM_NCACTIVATE: out = DefWindowProcW(h, msg, wp, -1); return true;
        case WM_GETMINMAXINFO: {
            auto* mm = (MINMAXINFO*)lp;
            mm->ptMinTrackSize.x = px(360); mm->ptMinTrackSize.y = px(260);
            out = 0;
            return true;
        }
    }
    return false;
}
static int EdgeHit(HWND h, POINT p) {   // resize zones for a borderless window; 0 = none
    if (IsZoomed(h)) return 0;
    RECT rc; GetWindowRect(h, &rc);
    int b = px(6);
    bool L = p.x < rc.left + b, R = p.x >= rc.right - b, T = p.y < rc.top + b, B = p.y >= rc.bottom - b;
    if (T && L) return HTTOPLEFT; if (T && R) return HTTOPRIGHT; if (B && L) return HTBOTTOMLEFT; if (B && R) return HTBOTTOMRIGHT;
    if (L) return HTLEFT; if (R) return HTRIGHT; if (T) return HTTOP; if (B) return HTBOTTOM;
    return 0;
}
static HWND MakeToolWindow(HWND* slot, const wchar_t* title, int w96, int h96) {
    POINT pt; GetCursorPos(&pt);
    HMONITOR mon = MonitorFromPoint(pt, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO mi = { sizeof(mi) };
    GetMonitorInfoW(mon, &mi);
    UINT dx = 96, dy = 96;
    GetDpiForMonitor(mon, MDT_EFFECTIVE_DPI, &dx, &dy);
    if ((int)dx != g_dpi) SetDpi((int)dx, true);
    ApplyTheme();
    int ww = (int)(mi.rcWork.right - mi.rcWork.left), wh = (int)(mi.rcWork.bottom - mi.rcWork.top);
    int w = std::min<int>(px(w96), ww - px(40)), h = std::min<int>(px(h96), wh - px(40));
    int x = (int)mi.rcWork.left + (ww - w) / 2, y = (int)mi.rcWork.top + (wh - h) / 2;
    HWND hw = CreateWindowExW(WS_EX_APPWINDOW, kToolClass, title, WS_POPUP | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
                              x, y, w, h, nullptr, nullptr, g_inst, nullptr);
    *slot = hw;   // set before the first real message so the window procedure knows which window it is
    if (hw) {
        SetWindowPos(hw, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);   // re-run NCCALCSIZE
        int dk = g_th->dark ? 1 : 0;
        DwmSetWindowAttribute(hw, 20, &dk, sizeof dk);
        if (g_win11) {
            int round = 2; DwmSetWindowAttribute(hw, 33, &round, sizeof round);
            COLORREF brd = g_th->border; DwmSetWindowAttribute(hw, 34, &brd, sizeof brd);
        }
    }
    return hw;
}

#include "note.h"
#include "web.h"

static LRESULT CALLBACK ToolProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    if (h == g_nw) return NoteProc(h, msg, wp, lp);
    if (h == g_ww) return WebProc(h, msg, wp, lp);
    return DefWindowProcW(h, msg, wp, lp);
}

// ================================================================== show / hide / tray / window
static void ForceForeground(HWND h) { SetForegroundWindow(h); BringWindowToTop(h); SetFocus(h); }

static void ShowPalette() {
    if (IsWindowVisible(g_hwnd)) return;
    KillTimer(g_hwnd, T_FREE); KillTimer(g_hwnd, T_TRIM);
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
    PlaceWindow(w, PalH(), true);
    if (g_animations) SetTimer(g_hwnd, T_ANIM, 15, nullptr);   // quick fade-in
    ForceForeground(g_hwnd);
    SetFocus(g_hQuery);
}
static void ShowPalettePrefill(const wstring& text) {
    ShowPalette();
    SetWindowTextW(g_hQuery, text.c_str());
    SendMessageW(g_hQuery, EM_SETSEL, (WPARAM)text.size(), (LPARAM)text.size());
}

static void HidePalette() {
    if (!IsWindowVisible(g_hwnd)) return;
    KillTimer(g_hwnd, T_QUERY); KillTimer(g_hwnd, T_ANIM); KillTimer(g_hwnd, T_SEL);
    ShowWindow(g_hwnd, SW_HIDE);
    // memory is given back only after a long idle (trimming right away would make the next open slower on old PCs)
    SetTimer(g_hwnd, T_TRIM, 120000, nullptr);
    SetTimer(g_hwnd, T_FREE, 180000, nullptr);
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
        case WM_PALETTE_PREFILL: ShowPalettePrefill(L"w "); return 0;
        case WM_APPS_READY: {
            auto* v = (vector<AppEntry>*)lp;
            g_apps = std::move(*v); delete v;
            g_appsLoaded = true; g_catalogDirty = true;
            if (IsWindowVisible(h)) Refresh();
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
            if (HIWORD(wp) == EN_CHANGE && LOWORD(wp) == 101) OnQueryChanged();
            return 0;
        case WM_CTLCOLOREDIT: {
            HDC dc = (HDC)wp;
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
                if (g_webPrivate) SetTimer(h, T_WIPE, 3000, nullptr);       // clean up leftover browser data from last time
            } else if (wp == T_TRIM) {
                KillTimer(h, T_TRIM);
                if (!IsWindowVisible(h)) { FreeBackBuffer(); SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1); }
            } else if (wp == T_FREE) {
                KillTimer(h, T_FREE);
                if (!IsWindowVisible(h)) FreeFiles();
            } else if (wp == T_ANIM) {
                double t = std::min(1.0, (double)(GetTickCount64() - g_animStart) / 110.0);
                double e = 1.0 - (1.0 - t) * (1.0 - t);
                SetLayeredWindowAttributes(h, 0, (BYTE)(255.0 * e), LWA_ALPHA);
                if (t >= 1.0) KillTimer(h, T_ANIM);
            } else if (wp == T_SEL) {
                double tg = SelTargetY(), d = tg - g_selY;
                if (std::fabs(d) < 1.5) { g_selY = tg; KillTimer(h, T_SEL); } else g_selY += d * 0.45;
                RECT r = { 0, ListTop(), PalW(), ListTop() + ListH() };
                InvalidateRect(h, &r, FALSE);
            } else if (wp == T_QUERY) {
                Refresh();
            } else if (wp == T_WEBFREE) {
                KillTimer(h, T_WEBFREE);
                WebFreeIfIdle();
            } else if (wp == T_WIPE) {
                KillTimer(h, T_WIPE);
                if (!g_ww && g_webPrivate) WipeDir(g_dir + L"\\web");
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
            else if (IsWindowVisible(h) && g_hQuery) SetFocus(g_hQuery);
            return 0;
        case WM_QUERYENDSESSION: return TRUE;
        case WM_ENDSESSION: if (wp) SaveNote(); return 0;
        case WM_ERASEBKGND: return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(h, &ps);
            RECT rc; GetClientRect(h, &rc);
            HDC m = GetBackBuffer(dc, rc.right, rc.bottom);
            DrawPalette(m, rc.right, rc.bottom);
            BitBlt(dc, 0, 0, rc.right, rc.bottom, m, 0, 0, SRCCOPY);
            EndPaint(h, &ps);
            return 0;
        }
        case WM_SETCURSOR:
            if (LOWORD(lp) == HTCLIENT) {
                POINT p; GetCursorPos(&p); ScreenToClient(h, &p);
                SetCursor(LoadCursorW(nullptr, HitTest(p.y) >= 0 ? IDC_HAND : IDC_ARROW));
                return TRUE;
            }
            break;
        case WM_MOUSEMOVE: {
            int y = GET_Y_LPARAM(lp);
            TRACKMOUSEEVENT t = { sizeof t, TME_LEAVE, h, 0 };
            TrackMouseEvent(&t);
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
            return 0;
        case WM_MOUSEWHEEL: {
            int d = GET_WHEEL_DELTA_WPARAM(wp);
            g_scroll -= d * ItemH() / WHEEL_DELTA;
            ClampListScroll(); SnapSel();
            InvalidateRect(h, nullptr, FALSE);
            return 0;
        }
        case WM_LBUTTONDOWN: {
            int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
            RECT tr, th;
            if (ListThumb(tr, th) && x >= tr.left - px(5) && x < tr.right + px(5) && y >= tr.top && y < tr.bottom) {
                g_drag = 1; g_dragY = y; g_dragScroll = g_scroll; SetCapture(h);
            }
            return 0;
        }
        case WM_LBUTTONUP: {
            if (g_drag) { g_drag = 0; ReleaseCapture(); return 0; }
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

    WNDCLASSEXW tc = { sizeof tc };   // class for the note / browser windows
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
