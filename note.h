// note.h - the note window: ONE live Markdown editor (Windows Rich Edit control), no title bar, closed with Esc.
// Formatting is applied as you type. Syntax marks (** ~~ ` [](url) # ...) are hidden while the caret is elsewhere and
// shown dimmed while the caret is inside that piece of text.
#pragma once
#include <ole2.h>
#include <richedit.h>
#include <richole.h>
#include <tom.h>

static HWND g_nw = nullptr, g_hEd = nullptr;
static WNDPROC g_oldEd = nullptr;
static bool g_noteOpen = false, g_noteDirty = false, g_noteLF = false, g_noteBusy = false, g_noteLoading = false, g_swallowEnter = false;
static int g_noteIdx = -1, g_prevLine = 0;
static wstring g_notePath;
static HMODULE g_hRich = nullptr;
static ITextDocument* g_tom = nullptr;
static vector<char> g_fmtState;

static int NStat() { return px(40); }
static int NMargin() { return px(18); }
static void ClientWHw(HWND hh, int& W, int& H) { RECT rc; GetClientRect(hh, &rc); W = rc.right; H = rc.bottom; }

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

// ------------------------------------------------------------------ syntax chips (bottom bar, scrolls sideways)
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
    { L"codeblock", L"```block```", CH_WRAP, L"```\r", L"\r```" },
    { L"quote", L"> quote", CH_PREFIX, L"> ", L"" },
    { L"bullet", L"- bullet", CH_PREFIX, L"- ", L"" },
    { L"number", L"1. number", CH_PREFIX, L"1. ", L"" },
    { L"task", L"- [ ] task", CH_PREFIX, L"- [ ] ", L"" },
    { L"link", L"[link](url)", CH_WRAP, L"[", L"](url)" },
    { L"image", L"![img](url)", CH_WRAP, L"![", L"](url)" },
    { L"table", L"| table |", CH_INSERT, L"| Column | Column |\r| --- | --- |\r| Cell | Cell |\r", L"" },
    { L"hr", L"--- rule", CH_INSERT, L"\r---\r", L"" },
    { L"date", L"date", CH_DATE, L"", L"" },
};
static const int kChipN = (int)(sizeof(kChips) / sizeof(kChips[0]));
static vector<int> g_chipOrder;
static std::unordered_map<wstring, int> g_chipCount;
static vector<RECT> g_chipRects;
static int g_chipScroll = 0, g_chipTotalW = 0, g_chipHot = -1;
static int g_chipDownX = 0, g_chipDownScroll = 0, g_chipDownIdx = -1, g_chipDownArrow = 0;
static bool g_chipDown = false, g_chipPanned = false;

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
static void BuildChipOrder() {   // most-used first, per note file
    g_chipOrder.clear();
    for (int i = 0; i < kChipN; i++) g_chipOrder.push_back(i);
    std::stable_sort(g_chipOrder.begin(), g_chipOrder.end(), [](int a, int b) {
        int ca = g_chipCount.count(kChips[a].id) ? g_chipCount[kChips[a].id] : 0;
        int cb = g_chipCount.count(kChips[b].id) ? g_chipCount[kChips[b].id] : 0;
        return ca > cb;
    });
}
static void ChipViewport(int W, int& l, int& r) {
    l = px(10); r = W - px(10);
    if (g_chipTotalW > r - l) { l = px(34); r = W - px(34); }   // room for the scroll arrows
}
static void ClampChipScroll(int W) {
    int l, r; ChipViewport(W, l, r);
    g_chipScroll = std::max<int>(0, std::min<int>(g_chipScroll, std::max<int>(0, g_chipTotalW - (r - l))));
}
static void LayoutChips() {
    HDC dc = GetDC(g_nw);
    HGDIOBJ o = SelectObject(dc, g_fChip);
    g_chipRects.assign(g_chipOrder.size(), RECT{ 0, 0, 0, 0 });
    int x = 0, h = px(26);
    for (size_t k = 0; k < g_chipOrder.size(); k++) {
        const wchar_t* lb = kChips[g_chipOrder[k]].label;
        SIZE sz; GetTextExtentPoint32W(dc, lb, (int)wcslen(lb), &sz);
        int w = sz.cx + px(18);
        g_chipRects[k] = { x, 0, x + w, h };
        x += w + px(6);
    }
    g_chipTotalW = std::max<int>(0, x - px(6));
    SelectObject(dc, o); ReleaseDC(g_nw, dc);
    int W, H; ClientWHw(g_nw, W, H);
    ClampChipScroll(W);
}

// ------------------------------------------------------------------ Rich Edit helpers (paragraphs are separated by '\r')
static wstring RichGet(HWND h) {
    GETTEXTLENGTHEX gl = { GTL_DEFAULT, 1200 };
    int n = (int)SendMessageW(h, EM_GETTEXTLENGTHEX, (WPARAM)&gl, 0);
    if (n <= 0) return L"";
    wstring s((size_t)n + 1, L'\0');
    GETTEXTEX gt = {};
    gt.cb = (DWORD)((n + 1) * sizeof(wchar_t)); gt.flags = GT_DEFAULT; gt.codepage = 1200;
    int got = (int)SendMessageW(h, EM_GETTEXTEX, (WPARAM)&gt, (LPARAM)&s[0]);
    s.resize((size_t)std::max<int>(0, got));
    return s;
}
static void RichSet(HWND h, const wstring& s) {
    SETTEXTEX st = { ST_DEFAULT, 1200 };
    SendMessageW(h, EM_SETTEXTEX, (WPARAM)&st, (LPARAM)s.c_str());
}
static void GetSel(LONG& a, LONG& b) { CHARRANGE r = { 0, 0 }; SendMessageW(g_hEd, EM_EXGETSEL, 0, (LPARAM)&r); a = r.cpMin; b = r.cpMax; }
static void SetSel(LONG a, LONG b) { CHARRANGE r = { a, b }; SendMessageW(g_hEd, EM_EXSETSEL, 0, (LPARAM)&r); }
static void ReplaceSel(const wstring& s) { SendMessageW(g_hEd, EM_REPLACESEL, TRUE, (LPARAM)s.c_str()); }
static wchar_t CharAt(LONG pos) {
    if (pos < 0) return 0;
    wchar_t buf[4] = {};
    TEXTRANGEW tr; tr.chrg.cpMin = pos; tr.chrg.cpMax = pos + 1; tr.lpstrText = buf;
    LRESULT n = SendMessageW(g_hEd, EM_GETTEXTRANGE, 0, (LPARAM)&tr);
    return n > 0 ? buf[0] : 0;
}
static wstring SelText(LONG a, LONG b) {
    if (b <= a) return L"";
    wstring s((size_t)(b - a) + 1, L'\0');
    SendMessageW(g_hEd, EM_GETSELTEXT, 0, (LPARAM)&s[0]);
    s.resize(wcslen(s.c_str()));
    return s;
}

// ------------------------------------------------------------------ live formatting
static void MakeBase(CHARFORMAT2W& cf) {
    ZeroMemory(&cf, sizeof cf); cf.cbSize = sizeof cf;
    cf.dwMask = CFM_FACE | CFM_SIZE | CFM_COLOR | CFM_BOLD | CFM_ITALIC | CFM_STRIKEOUT | CFM_UNDERLINE | CFM_HIDDEN | CFM_BACKCOLOR;
    cf.dwEffects = CFE_AUTOBACKCOLOR;
    cf.yHeight = MulDiv(g_noteFontSize, 1440, 96);
    cf.crTextColor = g_th->editorText;
    wcscpy_s(cf.szFaceName, g_noteFont.c_str());
}
static void ApplyFmt(LONG a, LONG b, DWORD mask, DWORD eff, COLORREF fg = 0, COLORREF bg = 0, int heightPct = 100, const wchar_t* face = nullptr) {
    if (b <= a) return;
    CHARFORMAT2W cf; ZeroMemory(&cf, sizeof cf); cf.cbSize = sizeof cf;
    cf.dwMask = mask; cf.dwEffects = eff;
    cf.crTextColor = fg; cf.crBackColor = bg;
    cf.yHeight = MulDiv(g_noteFontSize, 1440, 96) * heightPct / 100;
    if (face) wcscpy_s(cf.szFaceName, face);
    SetSel(a, b);
    SendMessageW(g_hEd, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
}
static void ApplyBase(LONG a, LONG b) {
    CHARFORMAT2W cf; MakeBase(cf);
    SetSel(a, b);
    SendMessageW(g_hEd, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
}
static void ParaFmt(LONG a, LONG b, int indentTwips) {
    PARAFORMAT2 pf; ZeroMemory(&pf, sizeof pf); pf.cbSize = sizeof pf;
    pf.dwMask = PFM_STARTINDENT | PFM_OFFSET | PFM_NUMBERING;
    pf.dxStartIndent = indentTwips;
    SetSel(a, b);
    SendMessageW(g_hEd, EM_SETPARAFORMAT, 0, (LPARAM)&pf);
}

enum { FK_HIDE = 1, FK_BOLD, FK_ITAL, FK_STRIKE, FK_HL, FK_CODE, FK_LINK, FK_DIM };
struct FSeg { int a, b, kind, gs, ge, arg; };   // offsets inside the line; gs..ge = the whole formatted piece (markers included)

static void ScanInline(const wstring& t, int from, int to, vector<FSeg>& out) {
    struct Open { wchar_t ch; int len; int pos; };
    vector<Open> st;
    int i = from;
    while (i < to) {
        wchar_t c = t[i];
        if (c == L'\\' && i + 1 < to) { i += 2; continue; }
        if (c == L'`') {
            size_t e0 = t.find(L'`', (size_t)i + 1);
            int e = e0 == wstring::npos ? -1 : (int)e0;
            if (e > i + 1 && e < to) {
                out.push_back({ i, i + 1, FK_HIDE, i, e + 1, 0 });
                out.push_back({ i + 1, e, FK_CODE, i, e + 1, 0 });
                out.push_back({ e, e + 1, FK_HIDE, i, e + 1, 0 });
                i = e + 1; continue;
            }
            i++; continue;
        }
        if (c == L'!' && i + 1 < to && t[i + 1] == L'[') {
            size_t rb0 = t.find(L"](", (size_t)i + 2);
            int rb = rb0 == wstring::npos ? -1 : (int)rb0;
            if (rb > 0 && rb < to) {
                size_t rp0 = t.find(L')', (size_t)rb + 2);
                int rp = rp0 == wstring::npos ? -1 : (int)rp0;
                if (rp > 0 && rp < to) {
                    out.push_back({ i, i + 2, FK_HIDE, i, rp + 1, 0 });
                    out.push_back({ i + 2, rb, FK_DIM, i, rp + 1, 0 });
                    out.push_back({ rb, rp + 1, FK_HIDE, i, rp + 1, 0 });
                    i = rp + 1; continue;
                }
            }
        }
        if (c == L'[') {
            size_t rb0 = t.find(L"](", (size_t)i + 1);
            int rb = rb0 == wstring::npos ? -1 : (int)rb0;
            if (rb > 0 && rb < to) {
                size_t rp0 = t.find(L')', (size_t)rb + 2);
                int rp = rp0 == wstring::npos ? -1 : (int)rp0;
                if (rp > 0 && rp < to) {
                    out.push_back({ i, i + 1, FK_HIDE, i, rp + 1, 0 });
                    out.push_back({ i + 1, rb, FK_LINK, i, rp + 1, rb });
                    out.push_back({ rb, rp + 1, FK_HIDE, i, rp + 1, 0 });
                    ScanInline(t, i + 1, rb, out);
                    i = rp + 1; continue;
                }
            }
        }
        if (c == L'h' && (t.compare((size_t)i, 7, L"http://") == 0 || t.compare((size_t)i, 8, L"https://") == 0) && (i == 0 || !iswalnum(t[i - 1]))) {
            int e = i;
            while (e < to && !iswspace(t[e])) e++;
            while (e > i && wcschr(L".,;:!?)\"'", t[e - 1])) e--;
            out.push_back({ i, e, FK_LINK, i, e, -1 });
            i = e; continue;
        }
        int mlen = 0;
        if (c == L'*' || c == L'_' || c == L'~' || c == L'=') {
            if (i + 1 < to && t[i + 1] == c) mlen = 2;
            else if (c == L'*' || c == L'_') mlen = 1;
        }
        if (mlen) {
            int oi = -1;
            for (int k = (int)st.size() - 1; k >= 0; k--) if (st[k].ch == c && st[k].len == mlen) { oi = k; break; }
            bool wordOK = true;
            if (c == L'_') {
                bool prevAl = i > from && iswalnum(t[i - 1]), nextAl = i + mlen < to && iswalnum(t[i + mlen]);
                wordOK = (oi >= 0) ? !nextAl : !prevAl;
            }
            if (oi >= 0 && wordOK && i > st[oi].pos + st[oi].len && !iswspace(t[i - 1])) {
                Open o = st[oi]; st.erase(st.begin() + oi);
                int gs = o.pos, ge = i + mlen;
                int kind = (c == L'~') ? FK_STRIKE : (c == L'=') ? FK_HL : (mlen == 2 ? FK_BOLD : FK_ITAL);
                out.push_back({ o.pos, o.pos + o.len, FK_HIDE, gs, ge, 0 });
                out.push_back({ o.pos + o.len, i, kind, gs, ge, 0 });
                out.push_back({ i, i + mlen, FK_HIDE, gs, ge, 0 });
                i += mlen; continue;
            }
            size_t f0 = (i + mlen < to) ? t.find(wstring((size_t)mlen, c), (size_t)i + mlen) : wstring::npos;
            if (wordOK && i + mlen < to && !iswspace(t[i + mlen]) && f0 != wstring::npos && (int)f0 < to) { st.push_back({ c, mlen, i }); i += mlen; continue; }
            i += mlen; continue;
        }
        i++;
    }
}

static bool IsFenceLine(const wstring& t) {
    size_t p = 0;
    while (p < t.size() && t[p] == L' ' && p < 3) p++;
    return t.compare(p, 3, L"```") == 0 || t.compare(p, 3, L"~~~") == 0;
}
static bool IsHrLine(const wstring& t) {
    wchar_t c = 0; int n = 0;
    for (wchar_t x : t) {
        if (x == L' ') continue;
        if (x != L'-' && x != L'*' && x != L'_') return false;
        if (c && x != c) return false;
        c = x; n++;
    }
    return n >= 3;
}

static void FormatLine(LONG ls, const wstring& t, int state, LONG selMin, LONG selMax) {
    LONG len = (LONG)t.size(), le = ls + len, endAll = le + 1;
    const Theme& T = *g_th;
    bool lineSel = selMax >= ls && selMin <= le;
    ApplyBase(ls, endAll);
    ParaFmt(ls, endAll, 0);
    auto reveal = [&](int gs, int ge) { return selMax >= ls + gs && selMin <= ls + ge; };
    auto hideOrDim = [&](int a, int b, bool show) {
        if (show) ApplyFmt(ls + a, ls + b, CFM_COLOR, 0, T.dim);
        else ApplyFmt(ls + a, ls + b, CFM_HIDDEN, CFE_HIDDEN);
    };
    if (state == 2) { ApplyFmt(ls, le, CFM_COLOR | CFM_FACE | CFM_BACKCOLOR, 0, T.dim, T.codeBg, 100, L"Consolas"); return; }
    if (state == 1) { ApplyFmt(ls, endAll, CFM_FACE | CFM_BACKCOLOR, 0, 0, T.codeBg, 100, L"Consolas"); return; }
    if (t.empty()) return;
    if (IsHrLine(t)) { ApplyFmt(ls, le, CFM_COLOR, 0, T.dim); return; }

    int from = 0;
    size_t p = 0;
    while (p < t.size() && t[p] == L' ' && p < 3) p++;
    size_t q = p;
    while (q < t.size() && t[q] == L'#') q++;
    int hl = (int)(q - p);
    if (hl >= 1 && hl <= 6 && q < t.size() && t[q] == L' ') {          // heading
        static const int pct[7] = { 100, 200, 165, 140, 120, 108, 100 };
        ApplyFmt(ls, endAll, CFM_SIZE | CFM_BOLD, CFE_BOLD, 0, 0, pct[hl]);
        hideOrDim((int)p, (int)q + 1, lineSel);
        from = (int)q + 1;
    } else if (p < t.size() && t[p] == L'>') {                          // quote
        size_t e = p;
        while (e < t.size() && (t[e] == L'>' || t[e] == L' ')) e++;
        hideOrDim((int)p, (int)e, lineSel);
        ParaFmt(ls, endAll, 360);
        ApplyFmt(ls + (LONG)e, le, CFM_COLOR | CFM_ITALIC, CFE_ITALIC, T.dim);
        from = (int)e;
    } else {
        size_t lp = p;                                                   // list / task
        while (lp < t.size() && t[lp] == L' ') lp++;
        size_t me = 0;
        if (lp + 1 < t.size() && (t[lp] == L'-' || t[lp] == L'*' || t[lp] == L'+') && t[lp + 1] == L' ') me = lp + 2;
        else {
            size_t d = lp;
            while (d < t.size() && iswdigit(t[d])) d++;
            if (d > lp && d + 1 < t.size() && (t[d] == L'.' || t[d] == L')') && t[d + 1] == L' ') me = d + 2;
        }
        if (me) {
            ApplyFmt(ls + (LONG)lp, ls + (LONG)me, CFM_COLOR | CFM_BOLD, CFE_BOLD, T.link);
            from = (int)me;
            if (me + 2 < t.size() && t[me] == L'[' && t[me + 2] == L']' && (t[me + 1] == L' ' || t[me + 1] == L'x' || t[me + 1] == L'X')) {
                ApplyFmt(ls + (LONG)me, ls + (LONG)me + 3, CFM_COLOR | CFM_BOLD, CFE_BOLD, T.link);
                from = (int)me + 3;
                if (from < (int)t.size() && t[from] == L' ') from++;
                if (t[me + 1] != L' ') ApplyFmt(ls + from, le, CFM_COLOR | CFM_STRIKEOUT, CFE_STRIKEOUT, T.dim);   // done tasks
            }
        } else if (p < t.size() && t[p] == L'|') {
            ApplyFmt(ls, le, CFM_FACE, 0, 0, 0, 100, L"Consolas");       // table row
        }
    }
    vector<FSeg> segs;
    ScanInline(t, from, (int)t.size(), segs);
    for (auto& s : segs) {
        LONG a = ls + s.a, b = ls + s.b;
        switch (s.kind) {
            case FK_HIDE: hideOrDim(s.a, s.b, reveal(s.gs, s.ge)); break;
            case FK_BOLD: ApplyFmt(a, b, CFM_BOLD, CFE_BOLD); break;
            case FK_ITAL: ApplyFmt(a, b, CFM_ITALIC, CFE_ITALIC); break;
            case FK_STRIKE: ApplyFmt(a, b, CFM_STRIKEOUT, CFE_STRIKEOUT); break;
            case FK_HL: ApplyFmt(a, b, CFM_BACKCOLOR, 0, 0, T.hl); break;
            case FK_CODE: ApplyFmt(a, b, CFM_FACE | CFM_BACKCOLOR, 0, 0, T.codeBg, 100, L"Consolas"); break;
            case FK_LINK: ApplyFmt(a, b, CFM_COLOR | CFM_UNDERLINE, CFE_UNDERLINE, T.link); break;
            case FK_DIM: ApplyFmt(a, b, CFM_COLOR | CFM_ITALIC, CFE_ITALIC, T.dim); break;
        }
    }
}

static int LineOf(const vector<LONG>& ls, LONG pos) {
    int lo = 0, hi = (int)ls.size() - 1;
    while (lo < hi) { int mid = (lo + hi + 1) / 2; if (ls[mid] <= pos) lo = mid; else hi = mid - 1; }
    return lo;
}

// Re-formats only the lines that need it: the caret line, the line the caret just left, and lines whose code-fence state changed.
static void NoteFormat(bool full) {
    if (!g_hEd || g_noteBusy || g_noteLoading) return;
    g_noteBusy = true;
    wstring txt = RichGet(g_hEd);
    vector<LONG> ls; ls.push_back(0);
    for (size_t i = 0; i < txt.size(); i++) if (txt[i] == L'\r') ls.push_back((LONG)i + 1);
    int nl = (int)ls.size();
    auto lineText = [&](int i) { LONG s = ls[i], e = (i + 1 < nl) ? ls[i + 1] - 1 : (LONG)txt.size(); return txt.substr((size_t)s, (size_t)(e - s)); };

    vector<char> st((size_t)nl, 0);
    bool in = false;
    for (int i = 0; i < nl; i++) { bool f = IsFenceLine(lineText(i)); st[i] = f ? 2 : (in ? 1 : 0); if (f) in = !in; }

    CHARRANGE sel = { 0, 0 };
    SendMessageW(g_hEd, EM_EXGETSEL, 0, (LPARAM)&sel);
    int cur = LineOf(ls, sel.cpMin);
    vector<char> dirty((size_t)nl, 0);
    if (full || g_fmtState.empty()) std::fill(dirty.begin(), dirty.end(), (char)1);
    else {
        size_t on = g_fmtState.size(), n = (size_t)nl, p = 0, q = 0;
        while (p < n && p < on && st[p] == g_fmtState[p]) p++;
        while (q < n - p && q < on - p && st[n - 1 - q] == g_fmtState[on - 1 - q]) q++;
        for (size_t i = p; i + q < n; i++) dirty[i] = 1;
        auto mark = [&](int i) { if (i >= 0 && i < nl) dirty[(size_t)i] = 1; };
        mark(cur); mark(g_prevLine); mark(cur - 1); mark(cur + 1);
        if (nl != (int)on) { int a = std::min(cur, g_prevLine), b = std::max(cur, g_prevLine); for (int i = a - 1; i <= b + 1; i++) mark(i); }
    }

    int firstVis = (int)SendMessageW(g_hEd, EM_GETFIRSTVISIBLELINE, 0, 0);
    SendMessageW(g_hEd, WM_SETREDRAW, FALSE, 0);
    if (g_tom) { long c = 0; g_tom->Undo(tomSuspend, &c); }   // formatting must not fill the undo stack
    for (int i = 0; i < nl; i++) if (dirty[(size_t)i]) FormatLine(ls[i], lineText(i), st[i], sel.cpMin, sel.cpMax);
    if (g_tom) { long c = 0; g_tom->Undo(tomResume, &c); }
    SendMessageW(g_hEd, EM_EXSETSEL, 0, (LPARAM)&sel);
    int nowVis = (int)SendMessageW(g_hEd, EM_GETFIRSTVISIBLELINE, 0, 0);
    if (nowVis != firstVis) SendMessageW(g_hEd, EM_LINESCROLL, 0, firstVis - nowVis);
    SendMessageW(g_hEd, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(g_hEd, nullptr, TRUE);
    g_fmtState = st;
    g_prevLine = cur;
    g_noteBusy = false;
}
static void NoteSchedule() { if (g_nw && !g_noteBusy && !g_noteLoading) SetTimer(g_nw, T_FMT, 25, nullptr); }

// ------------------------------------------------------------------ inserting markdown syntax (chips and shortcuts)
static void InsertChip(int ci) {
    if (ci < 0 || ci >= kChipN || !g_hEd) return;
    const ChipDef& c = kChips[ci];
    LONG s = 0, e = 0;
    GetSel(s, e);
    switch (c.kind) {
        case CH_WRAP: {
            wstring sel = SelText(s, e);
            ReplaceSel(wstring(c.a) + sel + c.b);
            if (sel.empty()) SetSel(s + (LONG)wcslen(c.a), s + (LONG)wcslen(c.a));
            break;
        }
        case CH_INSERT: ReplaceSel(c.a); break;
        case CH_DATE: {
            wchar_t d[32] = L"";
            GetDateFormatW(LOCALE_USER_DEFAULT, 0, nullptr, L"yyyy-MM-dd", d, 32);
            ReplaceSel(d);
            break;
        }
        case CH_PREFIX: case CH_HEAD: {
            wstring all = RichGet(g_hEd);
            size_t ls = 0;
            if (s > 0) { size_t p = all.rfind(L'\r', (size_t)s - 1); ls = (p == wstring::npos) ? 0 : p + 1; }
            size_t rm = 0, al = wcslen(c.a);
            if (c.kind == CH_HEAD) {
                while (ls + rm < all.size() && all[ls + rm] == L'#') rm++;
                if (rm && ls + rm < all.size() && all[ls + rm] == L' ') rm++;
                if (rm == al && all.compare(ls, rm, c.a) == 0) { SetSel((LONG)ls, (LONG)(ls + rm)); ReplaceSel(L""); break; }   // same heading again: toggle off
            } else if (all.compare(ls, al, c.a) == 0) { SetSel((LONG)ls, (LONG)(ls + al)); ReplaceSel(L""); break; }               // same prefix again: toggle off
            SetSel((LONG)ls, (LONG)(ls + rm));
            ReplaceSel(c.a);
            size_t np = ((size_t)s >= ls + rm) ? (size_t)s + al - rm : ls + al;
            SetSel((LONG)np, (LONG)np);
            break;
        }
    }
    g_chipCount[c.id]++;
    SendMessageW(g_hEd, EM_SCROLLCARET, 0, 0);
}
static void InsertChipId(const wchar_t* id) { for (int i = 0; i < kChipN; i++) if (!wcscmp(kChips[i].id, id)) { InsertChip(i); return; } }

// Enter inside a list/quote continues it, Enter on an empty item ends the list
static bool ListContinue() {
    LONG s = 0, e = 0;
    GetSel(s, e);
    if (s != e) return false;
    wstring all = RichGet(g_hEd);
    if ((size_t)s > all.size()) return false;
    size_t ls = 0;
    if (s > 0) { size_t p = all.rfind(L'\r', (size_t)s - 1); ls = (p == wstring::npos) ? 0 : p + 1; }
    wstring line = all.substr(ls, (size_t)s - ls);
    size_t p = 0;
    while (p < line.size() && line[p] == L' ') p++;
    wstring indent = line.substr(0, p), rest = line.substr(p), next;
    size_t used = 0;
    if (rest.compare(0, 2, L"> ") == 0) { next = L"> "; used = 2; }
    else if (rest.size() >= 6 && (rest.compare(0, 6, L"- [ ] ") == 0 || rest.compare(0, 6, L"- [x] ") == 0 || rest.compare(0, 6, L"- [X] ") == 0)) { next = L"- [ ] "; used = 6; }
    else if (rest.size() >= 2 && (rest[0] == L'-' || rest[0] == L'*' || rest[0] == L'+') && rest[1] == L' ') { next = rest.substr(0, 2); used = 2; }
    else {
        size_t d = 0; while (d < rest.size() && iswdigit(rest[d])) d++;
        if (d > 0 && d < 10 && d + 1 < rest.size() && rest[d] == L'.' && rest[d + 1] == L' ') { next = std::to_wstring(_wtoi(rest.substr(0, d).c_str()) + 1) + L". "; used = d + 2; }
        else return false;
    }
    if (rest.size() == used) { SetSel((LONG)ls, s); ReplaceSel(L""); return true; }   // empty item: stop the list
    ReplaceSel(L"\r" + indent + next);
    SendMessageW(g_hEd, EM_SCROLLCARET, 0, 0);
    return true;
}

// Typing an opening mark completes it: * _ ~ = ` [ ( . Typing the closing mark just steps over it.
static bool AutoPair(wchar_t c) {
    if (g_noteBusy || g_noteLoading) return false;
    LONG s = 0, e = 0;
    GetSel(s, e);
    bool has = e > s;
    wchar_t prev = CharAt(s - 1), next = CharAt(e);
    auto isMark = [](wchar_t x) { return x == L'*' || x == L'_' || x == L'~' || x == L'=' || x == L'`'; };
    auto edge = [](wchar_t x) { return x == 0 || x == L'\r' || iswspace(x) || (x < 128 && ispunct(x)); };
    if (isMark(c)) {
        if (has) {   // wrap the selection
            wstring sel = SelText(s, e);
            ReplaceSel(wstring(1, c) + sel + wstring(1, c));
            SetSel(s + 1, s + 1 + (LONG)sel.size());
            return true;
        }
        if (prev == c && next == c) {   // "*|*" -> "**|**" (bold), "**|**" -> "***|***" ...
            ReplaceSel(wstring(2, c));
            SetSel(s + 1, s + 1);
            return true;
        }
        if (next == c) { SetSel(s + 1, s + 1); return true; }   // step over the closing mark
        if (c == L'*') {   // a bullet like "* item" at the start of a line is not a pair
            wstring all = RichGet(g_hEd);
            size_t ls = 0;
            if (s > 0) { size_t p = all.rfind(L'\r', (size_t)s - 1); ls = (p == wstring::npos) ? 0 : p + 1; }
            bool onlySpace = true;
            for (size_t i = ls; i < (size_t)s && i < all.size(); i++) if (all[i] != L' ') { onlySpace = false; break; }
            if (onlySpace) return false;
        }
        if (c == L'~' || c == L'=' || c == L'`') { if (!(edge(prev) && edge(next))) return false; }
        else if (!(edge(prev) && edge(next))) return false;
        ReplaceSel(wstring(2, c));
        SetSel(s + 1, s + 1);
        return true;
    }
    if (c == L'[') { if (has) return false; ReplaceSel(L"[]"); SetSel(s + 1, s + 1); return true; }
    if (c == L']' && next == L']' && !has) { SetSel(s + 1, s + 1); return true; }
    if (c == L'(' && prev == L']' && !has) { ReplaceSel(L"()"); SetSel(s + 1, s + 1); return true; }
    if (c == L')' && next == L')' && !has) { SetSel(s + 1, s + 1); return true; }
    return false;
}
static bool PairBackspace() {   // Backspace inside an empty pair removes both marks
    LONG s = 0, e = 0;
    GetSel(s, e);
    if (e > s || s < 1) return false;
    wchar_t p = CharAt(s - 1), n = CharAt(s);
    bool pair = (p == n && (p == L'*' || p == L'_' || p == L'~' || p == L'=' || p == L'`')) || (p == L'[' && n == L']') || (p == L'(' && n == L')');
    if (!pair) return false;
    SetSel(s - 1, s + 1);
    ReplaceSel(L"");
    return true;
}

static bool OpenLinkSafe(const wstring& u) {
    if (u.rfind(L"http://", 0) == 0 || u.rfind(L"https://", 0) == 0 || u.rfind(L"mailto:", 0) == 0) {
        ShellExecuteW(nullptr, L"open", u.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        return true;
    }
    return false;
}
static bool ClickTask(LPARAM lp) {   // click a [ ] / [x] box to toggle it
    POINTL pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
    LONG cp = (LONG)SendMessageW(g_hEd, EM_CHARFROMPOS, 0, (LPARAM)&pt);
    wstring all = RichGet(g_hEd);
    if (cp < 0 || (size_t)cp > all.size()) return false;
    size_t ls = 0;
    if (cp > 0) { size_t p = all.rfind(L'\r', (size_t)cp - 1); ls = (p == wstring::npos) ? 0 : p + 1; }
    size_t le = all.find(L'\r', ls);
    if (le == wstring::npos) le = all.size();
    wstring line = all.substr(ls, le - ls);
    size_t p = 0;
    while (p < line.size() && line[p] == L' ') p++;
    if (!(p + 4 < line.size() && (line[p] == L'-' || line[p] == L'*' || line[p] == L'+') && line[p + 1] == L' ' && line[p + 2] == L'[' && line[p + 4] == L']')) return false;
    LONG b0 = (LONG)(ls + p + 2);
    if (cp < b0 || cp > b0 + 3) return false;
    wchar_t ch = line[p + 3];
    if (ch != L' ' && ch != L'x' && ch != L'X') return false;
    SetSel(b0 + 1, b0 + 2);
    ReplaceSel(ch == L' ' ? L"x" : L" ");
    return true;
}
static bool CtrlClickLink(LPARAM lp) {   // Ctrl+click opens a link (http, https, mailto only)
    POINTL pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
    LONG cp = (LONG)SendMessageW(g_hEd, EM_CHARFROMPOS, 0, (LPARAM)&pt);
    wstring all = RichGet(g_hEd);
    if (cp < 0 || (size_t)cp > all.size()) return false;
    size_t ls = 0;
    if (cp > 0) { size_t p = all.rfind(L'\r', (size_t)cp - 1); ls = (p == wstring::npos) ? 0 : p + 1; }
    size_t le = all.find(L'\r', ls);
    if (le == wstring::npos) le = all.size();
    wstring line = all.substr(ls, le - ls);
    int off = (int)(cp - (LONG)ls);
    vector<FSeg> segs;
    ScanInline(line, 0, (int)line.size(), segs);
    for (auto& s : segs) {
        if (s.kind != FK_LINK || off < s.gs || off > s.ge) continue;
        wstring url = (s.arg < 0) ? line.substr((size_t)s.a, (size_t)(s.b - s.a)) : line.substr((size_t)s.arg + 2, (size_t)(s.ge - 1 - (s.arg + 2)));
        return OpenLinkSafe(Trim(url));
    }
    return false;
}

static void SaveNote() {
    if (!g_noteOpen || !g_hEd) return;
    KillTimer(g_nw, T_AUTOSAVE);
    if (g_noteDirty) {
        wstring t = RichGet(g_hEd);
        t = ReplaceAll(t, L"\r", g_noteLF ? L"\n" : L"\r\n");
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
static void OnNoteChanged() {
    if (g_noteBusy || g_noteLoading) return;
    g_noteDirty = true;
    SetTimer(g_nw, T_AUTOSAVE, 2500, nullptr);
    NoteSchedule();
}

static LRESULT CALLBACK EdProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
        case WM_KEYDOWN: {
            bool ctrl = GetKeyState(VK_CONTROL) < 0, shift = GetKeyState(VK_SHIFT) < 0;
            if (w == VK_ESCAPE) { PostMessageW(g_nw, WM_CLOSE, 0, 0); return 0; }   // Esc saves and closes the note
            if (ctrl) {
                if (w == 'W') { PostMessageW(g_nw, WM_CLOSE, 0, 0); return 0; }
                if (w == 'S') { SaveNote(); return 0; }
                if (w == 'B') { InsertChipId(L"bold"); return 0; }
                if (w == 'I') { InsertChipId(L"italic"); return 0; }
                if (w == 'K') { InsertChipId(L"link"); return 0; }
                if (w == 'V') { SendMessageW(h, EM_PASTESPECIAL, CF_UNICODETEXT, 0); return 0; }   // plain text only
            }
            if (shift && w == VK_INSERT) { SendMessageW(h, EM_PASTESPECIAL, CF_UNICODETEXT, 0); return 0; }
            if (w == VK_RETURN && !ctrl && !shift && ListContinue()) { g_swallowEnter = true; return 0; }
            if (w == VK_BACK && !ctrl && PairBackspace()) return 0;
            break;
        }
        case WM_CHAR: {
            if (w == 13 && g_swallowEnter) { g_swallowEnter = false; return 0; }
            if (w == 27) return 0;
            if (GetKeyState(VK_CONTROL) < 0 && (w == 2 || w == 9 || w == 11 || w == 19 || w == 22 || w == 23)) return 0;
            if (w >= 32 && w != 127 && AutoPair((wchar_t)w)) return 0;
            break;
        }
        case WM_PASTE: SendMessageW(h, EM_PASTESPECIAL, CF_UNICODETEXT, 0); return 0;
        case WM_LBUTTONDOWN:
            if (GetKeyState(VK_CONTROL) < 0 && CtrlClickLink(l)) return 0;
            if (ClickTask(l)) return 0;
            break;
    }
    return CallWindowProcW(g_oldEd, h, m, w, l);
}

// ------------------------------------------------------------------ window
static void NoteLayout() {
    if (!g_noteOpen || !g_nw) return;
    int W, H; ClientWHw(g_nw, W, H);
    if (g_hEd) MoveWindow(g_hEd, NMargin(), px(18), std::max<int>(50, W - 2 * NMargin()), std::max<int>(50, H - NStat() - px(18) - px(8)), TRUE);
    LayoutChips();
    InvalidateRect(g_nw, nullptr, FALSE);
}

static void NoteContentClose() {
    if (g_nw) { KillTimer(g_nw, T_AUTOSAVE); KillTimer(g_nw, T_FMT); }
    if (g_tom) { g_tom->Release(); g_tom = nullptr; }
    if (g_hEd) { DestroyWindow(g_hEd); g_hEd = nullptr; g_oldEd = nullptr; }
    g_chipRects.clear(); g_fmtState.clear(); g_chipDown = false;
}

static bool NoteLoad(int idx) {
    g_noteIdx = idx;
    g_notePath = ExpandNotePath(g_notes[idx].path);
    if (!g_hRich) g_hRich = LoadLibraryW(L"Msftedit.dll");
    if (!g_hRich) { Balloon(L"Quick Palette", L"The Windows Rich Edit component was not found.", NIIF_WARNING); return false; }

    wstring txt;
    bool had = ReadFileUtf8(g_notePath, txt, 4u * 1024 * 1024);
    g_noteLF = had && txt.find(L'\r') == wstring::npos && txt.find(L'\n') != wstring::npos;
    if (had) { txt = ReplaceAll(txt, L"\r\n", L"\r"); txt = ReplaceAll(txt, L"\n", L"\r"); }

    g_noteLoading = true;
    g_hEd = CreateWindowExW(0, MSFTEDIT_CLASS, L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | ES_NOHIDESEL,
                            0, 0, 100, 100, g_nw, (HMENU)(INT_PTR)102, g_inst, nullptr);
    if (!g_hEd) { g_noteLoading = false; return false; }
    SendMessageW(g_hEd, EM_SETBKGNDCOLOR, 0, (LPARAM)g_th->editorBg);
    SendMessageW(g_hEd, EM_EXLIMITTEXT, 0, 4000000);
    CHARFORMAT2W cf; MakeBase(cf);
    SendMessageW(g_hEd, EM_SETCHARFORMAT, SCF_DEFAULT, (LPARAM)&cf);
    SetWindowTheme(g_hEd, g_th->dark ? L"DarkMode_Explorer" : L"Explorer", nullptr);
    g_oldEd = (WNDPROC)SetWindowLongPtrW(g_hEd, GWLP_WNDPROC, (LONG_PTR)EdProc);
    IRichEditOle* ole = nullptr;
    if (SendMessageW(g_hEd, EM_GETOLEINTERFACE, 0, (LPARAM)&ole) && ole) { ole->QueryInterface(__uuidof(ITextDocument), (void**)&g_tom); ole->Release(); }
    RichSet(g_hEd, txt);
    SendMessageW(g_hEd, EM_SETEVENTMASK, 0, ENM_CHANGE | ENM_SELCHANGE);
    g_noteLoading = false;

    g_noteDirty = false; g_fmtState.clear(); g_prevLine = 0;
    LoadChipCounts(); BuildChipOrder();
    g_chipScroll = 0; g_chipHot = -1;
    SetWindowTextW(g_nw, (g_notes[idx].name + L" - Quick Palette").c_str());
    NoteLayout();
    SetSel((LONG)txt.size(), (LONG)txt.size());
    NoteFormat(true);
    SendMessageW(g_hEd, EM_SCROLLCARET, 0, 0);
    return true;
}

static void EnterNote(int idx) {
    if (idx < 0 || idx >= (int)g_notes.size()) return;
    if (g_noteOpen && g_nw) {   // already open: switch note if needed, then bring it forward
        if (g_noteIdx != idx) { SaveNote(); NoteContentClose(); NoteLoad(idx); }
        if (IsIconic(g_nw)) ShowWindow(g_nw, SW_RESTORE);
        SetForegroundWindow(g_nw);
        if (g_hEd) SetFocus(g_hEd);
        return;
    }
    if (!MakeToolWindow(&g_nw, L"Notes", g_noteW, g_noteH)) return;
    g_noteOpen = true;
    if (!NoteLoad(idx)) { DestroyWindow(g_nw); return; }
    ShowWindow(g_nw, SW_SHOW);
    SetForegroundWindow(g_nw);
    if (g_hEd) SetFocus(g_hEd);
}
static void ExitNote() {   // the note window was destroyed
    NoteContentClose();
    g_noteOpen = false; g_noteIdx = -1; g_nw = nullptr;
}

static void DrawNote(HDC dc, int W, int H) {
    const Theme& T = *g_th;
    FillRectC(dc, { 0, 0, W, H }, T.editorBg);
    RECT all = { 0, 0, W, H };
    FrameRect(dc, &all, Brush(T.border));
    SetBkMode(dc, TRANSPARENT);
    int sy = H - NStat();
    FillRectC(dc, { 1, sy, W - 1, H - 1 }, T.footerBg);
    FillRectC(dc, { 1, sy, W - 1, sy + 1 }, T.divider);
    int l, r; ChipViewport(W, l, r);
    int ch = px(26), cy = sy + (NStat() - ch) / 2;
    int sc = SaveDC(dc);
    IntersectClipRect(dc, l, sy + 1, r, H - 1);
    for (size_t k = 0; k < g_chipRects.size(); k++) {
        RECT rc = g_chipRects[k];
        rc = { l + rc.left - g_chipScroll, cy, l + rc.right - g_chipScroll, cy + ch };
        if (rc.right < l || rc.left > r) continue;
        FillRound(dc, rc, (int)k == g_chipHot ? T.chipHot : T.chip, px(7));
        TextAt(dc, g_fChip, T.text, kChips[g_chipOrder[k]].label, rc, DT_CENTER | DT_VCENTER);
    }
    RestoreDC(dc, sc);
    if (g_chipTotalW > r - l) {   // sideways scroll arrows
        wchar_t la[2] = { 0xE76B, 0 }, ra[2] = { 0xE76C, 0 };
        bool canL = g_chipScroll > 0, canR = g_chipScroll < g_chipTotalW - (r - l);
        TextAt(dc, g_fIcon, canL ? T.text : T.thumb, la, { px(4), sy, l, H }, DT_CENTER | DT_VCENTER);
        TextAt(dc, g_fIcon, canR ? T.text : T.thumb, ra, { r, sy, W - px(4), H }, DT_CENTER | DT_VCENTER);
    }
}

static int ChipAt(int x, int y) {
    int W, H; ClientWHw(g_nw, W, H);
    int sy = H - NStat(), ch = px(26), cy = sy + (NStat() - ch) / 2;
    if (y < cy || y >= cy + ch) return -1;
    int l, r; ChipViewport(W, l, r);
    if (x < l || x >= r) return -1;
    for (size_t k = 0; k < g_chipRects.size(); k++)
        if (x >= l + g_chipRects[k].left - g_chipScroll && x < l + g_chipRects[k].right - g_chipScroll) return (int)k;
    return -1;
}
static int ArrowAt(int x, int y) {   // -1 left, 1 right, 0 none
    int W, H; ClientWHw(g_nw, W, H);
    int l, r; ChipViewport(W, l, r);
    if (g_chipTotalW <= r - l || y < H - NStat()) return 0;
    if (x < l) return -1;
    if (x >= r) return 1;
    return 0;
}
static void ChipScrollBy(int dx) {
    int W, H; ClientWHw(g_nw, W, H);
    g_chipScroll += dx;
    ClampChipScroll(W);
    InvalidateRect(g_nw, nullptr, FALSE);
}

static LRESULT CALLBACK NoteProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    LRESULT out;
    if (ToolFrameMsg(h, msg, wp, lp, out)) return out;
    switch (msg) {
        case WM_ERASEBKGND: return 1;
        case WM_NCHITTEST: {
            POINT p = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
            int e = EdgeHit(h, p);
            if (e) return e;
            POINT c = p; ScreenToClient(h, &c);
            int W, H; ClientWHw(h, W, H);
            if (c.y < px(18)) return HTCAPTION;   // invisible strip above the text: drag to move the window
            if (c.y >= H - NStat() && ChipAt(c.x, c.y) < 0 && ArrowAt(c.x, c.y) == 0) return HTCAPTION;   // empty part of the bottom bar
            return HTCLIENT;
        }
        case WM_SIZE:
            if (wp != SIZE_MINIMIZED) { EnsureDpiFor(h); NoteLayout(); }
            return 0;
        case WM_DPICHANGED: {
            const RECT* r = (const RECT*)lp;
            SetWindowPos(h, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE);
            SetDpi((int)LOWORD(wp), true);
            NoteLayout();
            return 0;
        }
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(h, &ps);
            RECT rc; GetClientRect(h, &rc);
            HDC m = CreateCompatibleDC(dc);
            HBITMAP bmp = CreateCompatibleBitmap(dc, rc.right, rc.bottom);
            HGDIOBJ old = SelectObject(m, bmp);
            DrawNote(m, rc.right, rc.bottom);
            BitBlt(dc, 0, 0, rc.right, rc.bottom, m, 0, 0, SRCCOPY);
            SelectObject(m, old); DeleteObject(bmp); DeleteDC(m);
            EndPaint(h, &ps);
            return 0;
        }
        case WM_ACTIVATE:
            if (LOWORD(wp) != WA_INACTIVE && g_hEd) SetFocus(g_hEd);
            return 0;
        case WM_COMMAND:
            if (HIWORD(wp) == EN_CHANGE && LOWORD(wp) == 102) OnNoteChanged();
            return 0;
        case WM_NOTIFY: {
            auto* n = (NMHDR*)lp;
            if (n && n->idFrom == 102 && n->code == EN_SELCHANGE) NoteSchedule();
            return 0;
        }
        case WM_TIMER:
            if (wp == T_AUTOSAVE) SaveNote();
            else if (wp == T_FMT) { KillTimer(h, T_FMT); NoteFormat(false); }
            return 0;
        case WM_MOUSEMOVE: {
            int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
            if (g_chipDown) {
                int dx = x - g_chipDownX;
                if (g_chipPanned || std::abs(dx) > px(4)) { g_chipPanned = true; int W, H; ClientWHw(h, W, H); g_chipScroll = g_chipDownScroll - dx; ClampChipScroll(W); InvalidateRect(h, nullptr, FALSE); }
                return 0;
            }
            int hot = ChipAt(x, y);
            if (hot != g_chipHot) { g_chipHot = hot; InvalidateRect(h, nullptr, FALSE); }
            TRACKMOUSEEVENT t = { sizeof t, TME_LEAVE, h, 0 };
            TrackMouseEvent(&t);
            return 0;
        }
        case WM_MOUSELEAVE:
            if (g_chipHot != -1) { g_chipHot = -1; InvalidateRect(h, nullptr, FALSE); }
            return 0;
        case WM_LBUTTONDOWN: {
            int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
            int W, H; ClientWHw(h, W, H);
            if (y >= H - NStat()) {   // press in the bottom bar: click a chip / arrow, or drag to scroll sideways
                g_chipDown = true; g_chipPanned = false; g_chipDownX = x; g_chipDownScroll = g_chipScroll;
                g_chipDownIdx = ChipAt(x, y); g_chipDownArrow = ArrowAt(x, y);
                SetCapture(h);
            }
            return 0;
        }
        case WM_LBUTTONUP: {
            if (g_chipDown) {
                g_chipDown = false; ReleaseCapture();
                if (!g_chipPanned) {
                    if (g_chipDownArrow) ChipScrollBy(g_chipDownArrow * px(160));
                    else if (g_chipDownIdx >= 0 && g_chipDownIdx < (int)g_chipOrder.size()) { InsertChip(g_chipOrder[g_chipDownIdx]); if (g_hEd) SetFocus(g_hEd); }
                }
            }
            return 0;
        }
        case WM_MOUSEWHEEL: {   // wheel over the bottom bar scrolls it sideways
            POINT p = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
            ScreenToClient(h, &p);
            int W, H; ClientWHw(h, W, H);
            if (p.y >= H - NStat()) ChipScrollBy(-GET_WHEEL_DELTA_WPARAM(wp) * px(60) / WHEEL_DELTA);
            return 0;
        }
        case WM_MOUSEHWHEEL: ChipScrollBy(GET_WHEEL_DELTA_WPARAM(wp) * px(60) / WHEEL_DELTA); return 0;
        case WM_SETCURSOR:
            if (LOWORD(lp) == HTCLIENT) {
                POINT p; GetCursorPos(&p); ScreenToClient(h, &p);
                bool hand = ChipAt(p.x, p.y) >= 0 || ArrowAt(p.x, p.y) != 0;
                SetCursor(LoadCursorW(nullptr, hand ? IDC_HAND : IDC_ARROW));
                return TRUE;
            }
            break;
        case WM_CLOSE:
            SaveNote();
            DestroyWindow(h);
            return 0;
        case WM_DESTROY:
            ExitNote();
            return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}
