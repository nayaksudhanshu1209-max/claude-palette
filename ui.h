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
static HBRUSH g_brBg = nullptr, g_brEd = nullptr;
static HFONT g_fChip = nullptr, g_fNoteHead = nullptr, g_fNote = nullptr;
static bool g_win11 = false, g_noteMode = false, g_suppress = false, g_facesChecked = false, g_lastCompact = false;
static int g_lineH = 28, g_qLines = 1;
static RECT g_work = { 0, 0, 1920, 1080 };
static POINT g_anchor = { 0, 0 };
static wchar_t g_uiFace[64] = L"Segoe UI", g_iconFace[64] = L"Segoe MDL2 Assets";
static const UINT WM_NOTE_CLOSE = WM_APP + 5;

static void HidePalette();
static void LayoutChildren();
static void EnterNote(int idx);
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
    SelectObject(dc, o); ReleaseDC(nullptr, dc);
    g_lineH = tm.tmHeight + tm.tmExternalLeading;
    g_lastCompact = g_compact;
    if (g_hQuery) SendMessageW(g_hQuery, WM_SETFONT, (WPARAM)g_fSearch, TRUE);
}

// ------------------------------------------------------------------ palette metrics
static int SearchPadV() { return px(g_compact ? 9 : 14); }
static int SearchH() { return std::max(SearchPadV() * 2 + g_qLines * g_lineH, px(g_compact ? 44 : 60)); }
static int ItemH() { return px(g_compact ? 38 : 50); }
static int HdrH() { return px(g_compact ? 24 : 28); }
static int FootH() { return g_showFooter ? px(g_compact ? 24 : 28) : 0; }
static int PalW() { return std::min(px(g_width), (int)(g_work.right - g_work.left) - px(8)); }
static int ContentH() { int h = 0; for (auto& r : g_rows) h += r.header ? HdrH() : ItemH(); return h; }
static int ListH() { return std::min(ContentH(), px(g_listMax)); }
static int ListTop() { return SearchH() + 1 + px(4); }
static int PalH() { int h = SearchH(); if (!g_rows.empty()) h += 1 + px(4) + ListH() + px(g_showFooter ? 6 : 8) + FootH(); return h; }
static int NoteW() { return std::max(px(480), std::min(px(g_noteW), (int)(g_work.right - g_work.left) - px(24))); }
static int NoteH() { return std::max(px(320), std::min(px(g_noteH), (int)(g_work.bottom - g_work.top) - px(24))); }

static void PlaceWindow(int w, int h, bool show) {
    int x = g_anchor.x, y = g_anchor.y;
    if (g_noteMode && !g_atCursor) {
        x = g_work.left + ((g_work.right - g_work.left) - w) / 2;
        y = g_work.top + ((g_work.bottom - g_work.top) - h) / 2;
    }
    if (x + w > g_work.right) x = g_work.right - w;
    if (x < g_work.left) x = g_work.left;
    if (y + h > g_work.bottom) y = g_work.bottom - h;   // never cut off by the screen edge
    if (y < g_work.top) y = g_work.top;
    SetWindowPos(g_hwnd, show ? HWND_TOPMOST : nullptr, x, y, w, h, show ? SWP_SHOWWINDOW : (SWP_NOZORDER | SWP_NOACTIVATE));
}

static void ApplyLayout() {
    int w = g_noteMode ? NoteW() : PalW(), h = g_noteMode ? NoteH() : PalH();
    PlaceWindow(w, h, false);
    if (!g_win11) SetWindowRgn(g_hwnd, CreateRoundRectRgn(0, 0, w + 1, h + 1, px(14), px(14)), TRUE);
    LayoutChildren();
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
    InvalidateRect(g_hwnd, nullptr, FALSE);
}
static void RunSelected() {
    if (g_sel < 0 || g_sel >= (int)g_results.size()) return;
    Item it = g_results[g_sel];
    if (it.kind == K_NOTE) {
        for (size_t i = 0; i < g_notes.size(); i++)
            if (_wcsicmp(g_notes[i].trig.c_str(), it.a.c_str()) == 0) { EnterNote((int)i); return; }
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
    wstring raw = GetEditText(g_hQuery);
    wstring low = lower(raw);
    for (size_t i = 0; i < g_notes.size(); i++)
        if (low == lower(g_notes[i].trig) + L" ") { EnterNote((int)i); return; }   // "n " turns the box into the note editor
    g_query = raw;
    for (auto& c : g_query) if (c == L'\r' || c == L'\n') c = L' ';
    int lines = (int)SendMessageW(g_hQuery, EM_GETLINECOUNT, 0, 0);
    lines = std::max(1, std::min(6, lines));
    bool changed = lines != g_qLines;
    g_qLines = lines;
    Refresh();
    if (changed) SendMessageW(g_hQuery, EM_LINESCROLL, 0, -100);
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
                    if (ctrl && g_sel >= 0 && g_sel < (int)g_results.size() && !g_results[g_sel].alt.empty()) {
                        wstring p = g_results[g_sel].alt;
                        HidePalette();
                        Reveal(p);
                    } else RunSelected();
                    return 0;
                case 'H': if (ctrl) { HideIndex(g_sel); return 0; } break;
                case 'A': if (ctrl) { SendMessageW(h, EM_SETSEL, 0, -1); return 0; } break;
            }
            break;
        }
        case WM_CHAR: {
            bool ctrl = GetKeyState(VK_CONTROL) < 0;
            if (w == 13) { if (GetKeyState(VK_SHIFT) < 0) break; return 0; }
            if (w == 27 || w == 9) return 0;
            if (ctrl && (w == 8 || w == 1)) return 0;
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
    int th = std::max(px(24), (track.bottom - track.top) * lh / ch);
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
                if (sel) FillRound(dc, rr, T.sel, px(8));
                else if (r.idx == g_hover) FillRound(dc, rr, T.hov, px(8));
                COLORREF ac = Accent(it.cat);
                int ts = px(g_compact ? 26 : 34);
                RECT tile = { px(18), y + (h - ts) / 2, px(18) + ts, y + (h - ts) / 2 + ts };
                FillRound(dc, tile, Mix(sel ? T.sel : T.bg, ac, T.dark ? 24 : 18), px(9));
                wchar_t gl[2] = { it.glyph, 0 };
                TextAt(dc, g_fIcon, ac, gl, tile, DT_CENTER | DT_VCENTER);
                int tx = tile.right + px(12);
                int right = W - px(g_hover >= 0 || ContentH() > lh ? 26 : 20);
                if (!g_compact) {
                    TextAt(dc, g_fTitle, T.text, it.title, { tx, y + px(7), right, y + px(27) }, 0);
                    TextAt(dc, g_fSub, T.dim, it.sub, { tx, y + px(27), right, y + px(45) }, 0);
                } else {
                    SelectObject(dc, g_fTitle);
                    SIZE sz; GetTextExtentPoint32W(dc, it.title.c_str(), (int)it.title.size(), &sz);
                    int tw = std::min((int)sz.cx, (right - tx) * 65 / 100);
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
        TextAt(dc, g_fSub, T.footerText, L"\u21B5 Open      Ctrl+\u21B5 Show in folder      Ctrl+H Hide item      Esc Close", { px(20), fy, W - px(20), H - 1 }, DT_VCENTER);
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
    HDC dc = GetDC(g_hwnd);
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
    SelectObject(dc, o); ReleaseDC(g_hwnd, dc);
    int W, H; ClientWH(W, H);
    g_chipScroll = std::max(0, std::min(g_chipScroll, std::max(0, g_chipTotalW - (W - px(12)))));
}

// ---- preview
static bool PvThumb(RECT& track, RECT& thumb) {
    int W, H; ClientWH(W, H);
    RECT pa = PreviewArea(W, H);
    int vh = pa.bottom - pa.top;
    if (g_pvH <= vh) return false;
    track = { pa.right - px(9), pa.top + px(4), pa.right - px(4), pa.bottom - px(4) };
    int th = std::max(px(28), (track.bottom - track.top) * vh / g_pvH);
    int ty = track.top + (int)((long long)g_pvScroll * ((track.bottom - track.top) - th) / (g_pvH - vh));
    thumb = { track.left, ty, track.right, ty + th };
    return true;
}
static void ClampPv() {
    int W, H; ClientWH(W, H);
    RECT pa = PreviewArea(W, H);
    g_pvScroll = std::max(0, std::min(g_pvScroll, std::max(0, g_pvH - (pa.bottom - pa.top))));
}
static void LayoutPreview() {
    if (!g_noteMode || g_noteView == 0) return;
    int W, H; ClientWH(W, H);
    RECT pa = PreviewArea(W, H);
    HDC dc = GetDC(g_hwnd);
    g_md.Layout(dc, (pa.right - pa.left) - px(10), px(15));
    ReleaseDC(g_hwnd, dc);
    g_pvH = g_md.height;
    ClampPv();
}
static void RebuildPreview(bool follow) {
    if (!g_noteMode || !g_hEditor) return;
    wstring t = GetEditText(g_hEditor);
    size_t words = 0; bool in = false;
    for (wchar_t c : t) { bool sp = iswspace(c) != 0; if (!sp && !in) words++; in = !sp; }
    g_noteStats = std::to_wstring(words) + L" words \u00B7 " + std::to_wstring(t.size()) + L" chars";
    if (g_noteView != 0) {
        g_md.Parse(t);
        LayoutPreview();
        if (follow && g_noteView == 1) {
            int W, H; ClientWH(W, H);
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
    InvalidateRect(g_hwnd, nullptr, FALSE);
}
static void OnNoteChanged() {
    if (g_suppress || !g_noteMode) return;
    g_noteDirty = true;
    SetTimer(g_hwnd, T_AUTOSAVE, 2500, nullptr);
    SetTimer(g_hwnd, T_PREVIEW, 150, nullptr);
}

static void LayoutChildren() {
    int W, H; ClientWH(W, H);
    if (!g_noteMode) {
        int eh = g_qLines * g_lineH;
        MoveWindow(g_hQuery, px(52), (SearchH() - eh) / 2, W - px(52) - px(20), eh, TRUE);
        return;
    }
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
}

static void SetView(int v) {
    g_noteView = v;
    ApplyLayout();
    RebuildPreview(false);
    if (v == 2) SetFocus(g_hwnd); else if (g_hEditor) SetFocus(g_hEditor);
}
static void CycleView() { SetView((g_noteView + 1) % 3); }

// ---- load / save
static void SaveNote() {
    if (!g_noteMode || !g_hEditor) return;
    KillTimer(g_hwnd, T_AUTOSAVE);
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

static void EnterNote(int idx) {
    if (idx < 0 || idx >= (int)g_notes.size()) return;
    g_noteIdx = idx; g_noteMode = true;
    g_noteView = g_noteViewCfg == L"edit" ? 0 : g_noteViewCfg == L"preview" ? 2 : 1;
    g_notePath = ExpandNotePath(g_notes[idx].path);
    g_suppress = true; SetWindowTextW(g_hQuery, L""); g_suppress = false;
    g_query.clear(); g_qLines = 1; g_results.clear(); g_rows.clear();
    ShowWindow(g_hQuery, SW_HIDE);

    wstring txt;
    bool had = ReadFileUtf8(g_notePath, txt, 4u * 1024 * 1024);
    g_noteLF = had && txt.find(L'\r') == wstring::npos && txt.find(L'\n') != wstring::npos;
    if (had) { txt = ReplaceAll(txt, L"\r\n", L"\n"); txt = ReplaceAll(txt, L"\n", L"\r\n"); }

    g_fNote = MakeFont(px(g_noteFontSize), 400, g_noteFont.c_str());
    g_hEditor = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | ES_NOHIDESEL,
                                0, 0, 100, 100, g_hwnd, (HMENU)(INT_PTR)102, g_inst, nullptr);
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
    ApplyLayout();
    RebuildPreview(false);
    if (g_noteView == 2) SetFocus(g_hwnd); else SetFocus(g_hEditor);
    InvalidateRect(g_hwnd, nullptr, FALSE);
}

static void ExitNote() {
    KillTimer(g_hwnd, T_AUTOSAVE); KillTimer(g_hwnd, T_PREVIEW);
    if (g_hEditor) { DestroyWindow(g_hEditor); g_hEditor = nullptr; g_oldEditor = nullptr; }
    if (g_fNote) { DeleteObject(g_fNote); g_fNote = nullptr; }
    g_md.Clear();
    g_noteMode = false; g_noteIdx = -1; g_chipRects.clear(); g_noteStats.clear(); g_drag = 0;
    ShowWindow(g_hQuery, SW_SHOW);
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
            if (w == VK_ESCAPE) { PostMessageW(g_hwnd, WM_NOTE_CLOSE, 0, 0); return 0; }   // save + close palette
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
    int W, H; ClientWH(W, H);
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
    int W, H; ClientWH(W, H);
    RECT pa = PreviewArea(W, H);
    if (!InRect(pa, x, y)) return false;
    lx = x - pa.left; ly = y - pa.top + g_pvScroll;
    return true;
}
static void NoteMouseDown(int x, int y) {
    RECT tr, th;
    if (PvThumb(tr, th) && x >= tr.left - px(4) && x < tr.right + px(4) && y >= tr.top && y < tr.bottom) {
        g_drag = 2; g_dragY = y; g_dragScroll = g_pvScroll; SetCapture(g_hwnd);
        if (!InRect(th, x, y)) {   // clicked the track: jump there
            int vh; { int W, H; ClientWH(W, H); RECT pa = PreviewArea(W, H); vh = pa.bottom - pa.top; }
            int thH = th.bottom - th.top;
            g_pvScroll = (int)((long long)(y - tr.top - thH / 2) * (g_pvH - vh) / std::max(1, (tr.bottom - tr.top) - thH));
            ClampPv(); g_dragY = y; g_dragScroll = g_pvScroll;
            InvalidateRect(g_hwnd, nullptr, FALSE);
        }
    }
}
static void NoteMouseUp(int x, int y) {
    if (g_drag) { g_drag = 0; ReleaseCapture(); return; }
    int W, H; ClientWH(W, H);
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
            int W, H; ClientWH(W, H);
            RECT pa = PreviewArea(W, H);
            int vh = pa.bottom - pa.top, thH = th.bottom - th.top;
            g_pvScroll = g_dragScroll + (int)((long long)(y - g_dragY) * (g_pvH - vh) / std::max(1, (tr.bottom - tr.top) - thH));
            ClampPv();
            InvalidateRect(g_hwnd, nullptr, FALSE);
        }
        return;
    }
    int hot = ChipAt(x, y);
    if (hot != g_chipHot) { g_chipHot = hot; InvalidateRect(g_hwnd, nullptr, FALSE); }
}
static void NoteWheel(int delta, int x, int y) {
    int W, H; ClientWH(W, H);
    if (y >= H - NStat()) {
        g_chipScroll -= delta * px(60) / WHEEL_DELTA;
        g_chipScroll = std::max(0, std::min(g_chipScroll, std::max(0, g_chipTotalW - (W - px(12)))));
        InvalidateRect(g_hwnd, nullptr, FALSE);
    } else if (g_noteView != 0 && InRect(PreviewArea(W, H), x, y)) {
        g_pvScroll -= delta * px(60) / WHEEL_DELTA;
        ClampPv();
        InvalidateRect(g_hwnd, nullptr, FALSE);
    }
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
    Refresh();
    PlaceWindow(w, PalH(), true);   // always clamped inside the screen
    ForceForeground(g_hwnd);
    SetFocus(g_hQuery);
}

static void HidePalette() {
    if (!IsWindowVisible(g_hwnd)) return;
    if (g_noteMode) { SaveNote(); ExitNote(); }
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
                SetTimer(h, T_TRIM, 1500, nullptr);
            } else if (wp == T_TRIM) {
                KillTimer(h, T_TRIM);
                if (!IsWindowVisible(h)) SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);
            } else if (wp == T_AUTOSAVE) {
                SaveNote();
            } else if (wp == T_PREVIEW) {
                KillTimer(h, T_PREVIEW);
                RebuildPreview(true);
            } else if (wp == T_FREE) {
                KillTimer(h, T_FREE);
                if (!IsWindowVisible(h)) { FreeFiles(); SetTimer(h, T_TRIM, 500, nullptr); }
            }
            return 0;
        case WM_ACTIVATE:
            if (LOWORD(wp) == WA_INACTIVE) { if (!g_menuOpen && g_hideOnFocusLoss) HidePalette(); }
            else if (IsWindowVisible(h)) {
                if (g_noteMode) { if (g_hEditor && g_noteView != 2) SetFocus(g_hEditor); }
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
            HDC m = CreateCompatibleDC(dc);
            HBITMAP bmp = CreateCompatibleBitmap(dc, rc.right, rc.bottom);
            HGDIOBJ old = SelectObject(m, bmp);
            if (g_noteMode) DrawNote(m, rc.right, rc.bottom); else DrawPalette(m, rc.right, rc.bottom);
            BitBlt(dc, 0, 0, rc.right, rc.bottom, m, 0, 0, SRCCOPY);
            SelectObject(m, old); DeleteObject(bmp); DeleteDC(m);
            EndPaint(h, &ps);
            return 0;
        }
        case WM_SETCURSOR:
            if (LOWORD(lp) == HTCLIENT) {
                POINT p; GetCursorPos(&p); ScreenToClient(h, &p);
                bool hand = false;
                if (g_noteMode) {
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
            if (g_noteMode) { NoteMouseMove(x, y); return 0; }
            if (g_drag == 1) {
                RECT tr, th;
                if (ListThumb(tr, th)) {
                    int thH = th.bottom - th.top;
                    g_scroll = g_dragScroll + (int)((long long)(y - g_dragY) * (ContentH() - ListH()) / std::max(1, (tr.bottom - tr.top) - thH));
                    ClampListScroll();
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
            else { g_scroll -= d * ItemH() / WHEEL_DELTA; ClampListScroll(); InvalidateRect(h, nullptr, FALSE); }
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

    g_hwnd = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST, kClass, L"Quick Palette", WS_POPUP | WS_CLIPCHILDREN, 0, 0, px(640), px(60), nullptr, nullptr, inst, nullptr);
    if (!g_hwnd) return 1;
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
