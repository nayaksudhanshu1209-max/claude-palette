// md.h - tiny Markdown parser + GDI renderer. No dependencies, no network, no images.
// Supports: headings (# and ===/---), bold, italic, strike, ==highlight==, `code`, fenced code,
// links, autolinks, images (shown as a labelled placeholder), quotes, bullet/numbered/task lists
// (task boxes are clickable), tables with alignment, horizontal rules, escapes, hard line breaks.
#pragma once

struct MdStyle { bool b = false, i = false, s = false, code = false, hl = false, dim = false; int link = -1; };
struct MdSpan { wstring text; MdStyle st; };
struct MdBlock {
    enum T { PARA, HEAD, CODE, LIST, HR, TABLE } type = PARA;
    int level = 0, quote = 0, task = 0, taskPos = -1;
    wstring marker, code;
    vector<MdSpan> spans;
    vector<vector<vector<MdSpan>>> cells;   // table: row -> column -> spans
    vector<int> align;                      // 0 left, 1 center, 2 right
};
struct MdTheme { COLORREF text, dim, link, codeBg, bar, rule, hl, tableHead, tableLine, accent; };
enum { OP_TEXT = 1, OP_FILL, OP_CHECK };
struct MdOp {
    int type = 0; RECT r = { 0, 0, 0, 0 }; int by = 0; wstring text; MdStyle st; int fkey = 0;
    COLORREF col = 0; int taskPos = -1; int checked = 0; int round = 0;
};

struct MdView {
    vector<MdBlock> blocks;
    vector<wstring> links;
    vector<MdOp> ops;
    int height = 0;
    wstring bodyFace = L"Segoe UI", monoFace = L"Consolas";

    struct F { HFONT f; int asc; int h; };
    std::unordered_map<int, F> fonts;

    ~MdView() { Clear(); }
    void FreeFonts() { for (auto& kv : fonts) DeleteObject(kv.second.f); fonts.clear(); }
    void Clear() { blocks.clear(); links.clear(); ops.clear(); FreeFonts(); height = 0; }

    const F& Font(HDC dc, int size, bool b, bool i, bool mono) {
        int key = (size << 3) | (b ? 1 : 0) | (i ? 2 : 0) | (mono ? 4 : 0);
        auto it = fonts.find(key);
        if (it != fonts.end()) return it->second;
        HFONT f = CreateFontW(-size, 0, 0, 0, b ? FW_BOLD : FW_NORMAL, i, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                              CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, mono ? monoFace.c_str() : bodyFace.c_str());
        HGDIOBJ o = SelectObject(dc, f);
        TEXTMETRICW tm; GetTextMetricsW(dc, &tm);
        SelectObject(dc, o);
        F nf; nf.f = f; nf.asc = tm.tmAscent; nf.h = tm.tmHeight;
        return fonts[key] = nf;
    }
    static int Key(int size, bool b, bool i, bool mono) { return (size << 3) | (b ? 1 : 0) | (i ? 2 : 0) | (mono ? 4 : 0); }

    // ------------------------------------------------------------ parsing
    struct Ln { wstring t; int start; };

    static bool Blank(const wstring& t) { return t.find_first_not_of(L" \t") == wstring::npos; }
    static int StripQuote(wstring& t, size_t& removed) {
        int d = 0; size_t p = 0;
        for (;;) {
            size_t q = p;
            while (q < t.size() && t[q] == L' ') q++;
            if (q < t.size() && t[q] == L'>') { d++; p = q + 1; if (p < t.size() && t[p] == L' ') p++; }
            else break;
        }
        removed = p; t = t.substr(p);
        return d;
    }
    static bool IsHr(const wstring& t) {
        wchar_t c = 0; int n = 0;
        for (wchar_t x : t) {
            if (x == L' ') continue;
            if (x != L'-' && x != L'*' && x != L'_') return false;
            if (c && x != c) return false;
            c = x; n++;
        }
        return n >= 3;
    }
    static bool Fence(const wstring& t, wchar_t& ch) {
        size_t p = 0; while (p < t.size() && t[p] == L' ' && p < 3) p++;
        if (t.compare(p, 3, L"```") == 0) { ch = L'`'; return true; }
        if (t.compare(p, 3, L"~~~") == 0) { ch = L'~'; return true; }
        return false;
    }
    static int Heading(const wstring& t, size_t& textStart) {
        size_t p = 0; while (p < t.size() && t[p] == L' ' && p < 3) p++;
        size_t q = p; while (q < t.size() && t[q] == L'#') q++;
        int n = (int)(q - p);
        if (n < 1 || n > 6) return 0;
        if (q < t.size() && t[q] != L' ') return 0;
        while (q < t.size() && t[q] == L' ') q++;
        textStart = q; return n;
    }
    // list item: returns true and fills level/marker/content start
    static bool ListItem(const wstring& t, int& level, wstring& marker, size_t& cs) {
        size_t p = 0; int cols = 0;
        while (p < t.size() && (t[p] == L' ' || t[p] == L'\t')) { cols += t[p] == L'\t' ? 4 : 1; p++; }
        if (p >= t.size()) return false;
        level = std::min(cols / 2, 5);
        if ((t[p] == L'-' || t[p] == L'*' || t[p] == L'+') && p + 1 < t.size() && t[p + 1] == L' ') {
            marker = L"\u2022"; cs = p + 2; return true;
        }
        size_t q = p; while (q < t.size() && iswdigit(t[q])) q++;
        if (q > p && q - p < 10 && q + 1 < t.size() && (t[q] == L'.' || t[q] == L')') && t[q + 1] == L' ') {
            marker = t.substr(p, q - p) + L"."; cs = q + 2; return true;
        }
        return false;
    }
    static bool TableSep(const wstring& t) {
        if (t.find(L'-') == wstring::npos) return false;
        for (wchar_t c : t) if (c != L'|' && c != L'-' && c != L':' && c != L' ') return false;
        return true;
    }
    static vector<wstring> Cells(const wstring& t) {
        vector<wstring> out; wstring cur;
        size_t a = t.find_first_not_of(L' '); size_t b = t.find_last_not_of(L' ');
        if (a == wstring::npos) return out;
        wstring u = t.substr(a, b - a + 1);
        if (!u.empty() && u[0] == L'|') u.erase(0, 1);
        if (!u.empty() && u.back() == L'|' && !(u.size() > 1 && u[u.size() - 2] == L'\\')) u.pop_back();
        for (size_t i = 0; i < u.size(); i++) {
            if (u[i] == L'\\' && i + 1 < u.size() && u[i + 1] == L'|') { cur += L'|'; i++; }
            else if (u[i] == L'|') { out.push_back(Trim(cur)); cur.clear(); }
            else cur += u[i];
        }
        out.push_back(Trim(cur));
        return out;
    }
    static bool IsUrl(const wstring& u) {
        return u.rfind(L"http://", 0) == 0 || u.rfind(L"https://", 0) == 0 || u.rfind(L"mailto:", 0) == 0;
    }

    void Inline(const wstring& s, vector<MdSpan>& out, MdStyle st) {
        wstring cur;
        auto flush = [&]() { if (!cur.empty()) { MdSpan sp; sp.text = cur; sp.st = st; out.push_back(sp); cur.clear(); } };
        size_t n = s.size();
        for (size_t i = 0; i < n;) {
            wchar_t c = s[i];
            if (c == L'\\' && i + 1 < n && wcschr(L"\\`*_{}[]()#+-.!|~<>=", s[i + 1])) { cur += s[i + 1]; i += 2; continue; }
            if (c == L'`') {
                size_t e = s.find(L'`', i + 1);
                if (e != wstring::npos) { flush(); MdSpan sp; sp.text = s.substr(i + 1, e - i - 1); sp.st = st; sp.st.code = true; out.push_back(sp); i = e + 1; continue; }
            }
            if (c == L'!' && i + 1 < n && s[i + 1] == L'[') {
                size_t rb = s.find(L"](", i + 2);
                if (rb != wstring::npos) {
                    size_t rp = s.find(L')', rb + 2);
                    if (rp != wstring::npos) {
                        flush(); MdSpan sp; sp.text = L"\u25A3 " + s.substr(i + 2, rb - i - 2); sp.st = st; sp.st.i = true; out.push_back(sp);
                        i = rp + 1; continue;
                    }
                }
            }
            if (c == L'[') {
                size_t rb = s.find(L"](", i + 1);
                if (rb != wstring::npos) {
                    size_t rp = s.find(L')', rb + 2);
                    if (rp != wstring::npos) {
                        flush();
                        links.push_back(Trim(s.substr(rb + 2, rp - rb - 2)));
                        MdStyle ls = st; ls.link = (int)links.size() - 1;
                        Inline(s.substr(i + 1, rb - i - 1), out, ls);
                        i = rp + 1; continue;
                    }
                }
            }
            if (c == L'<') {
                size_t e = s.find(L'>', i + 1);
                if (e != wstring::npos) {
                    wstring u = s.substr(i + 1, e - i - 1);
                    if (IsUrl(u)) {
                        flush(); links.push_back(u);
                        MdSpan sp; sp.text = u; sp.st = st; sp.st.link = (int)links.size() - 1; out.push_back(sp);
                        i = e + 1; continue;
                    }
                }
            }
            if (c == L'h' && (s.compare(i, 7, L"http://") == 0 || s.compare(i, 8, L"https://") == 0) && (i == 0 || !iswalnum(s[i - 1]))) {
                size_t e = i;
                while (e < n && !iswspace(s[e]) && s[e] != L'<') e++;
                while (e > i && wcschr(L".,;:!?)\"'", s[e - 1])) e--;
                flush();
                wstring u = s.substr(i, e - i); links.push_back(u);
                MdSpan sp; sp.text = u; sp.st = st; sp.st.link = (int)links.size() - 1; out.push_back(sp);
                i = e; continue;
            }
            if (i + 1 < n && ((c == L'*' && s[i + 1] == L'*') || (c == L'_' && s[i + 1] == L'_'))) {
                wstring mk(2, c);
                if (st.b || s.find(mk, i + 2) != wstring::npos) { flush(); st.b = !st.b; i += 2; continue; }
            }
            if (c == L'~' && i + 1 < n && s[i + 1] == L'~') {
                if (st.s || s.find(L"~~", i + 2) != wstring::npos) { flush(); st.s = !st.s; i += 2; continue; }
            }
            if (c == L'=' && i + 1 < n && s[i + 1] == L'=') {
                if (st.hl || s.find(L"==", i + 2) != wstring::npos) { flush(); st.hl = !st.hl; i += 2; continue; }
            }
            if (c == L'*' || c == L'_') {
                bool ok;
                if (c == L'_') ok = st.i ? (i + 1 >= n || !iswalnum(s[i + 1])) : (i == 0 || !iswalnum(s[i - 1]));
                else ok = true;
                if (ok) {
                    if (st.i) { flush(); st.i = false; i++; continue; }
                    if (i + 1 < n && !iswspace(s[i + 1]) && s.find(c, i + 1) != wstring::npos) { flush(); st.i = true; i++; continue; }
                }
            }
            cur += c; i++;
        }
        flush();
    }

    void Parse(const wstring& src) {
        blocks.clear(); links.clear();
        vector<Ln> L;
        { size_t st = 0;
          while (st <= src.size()) {
              size_t e = src.find(L'\n', st);
              size_t en = (e == wstring::npos) ? src.size() : e;
              wstring t = src.substr(st, en - st);
              if (!t.empty() && t.back() == L'\r') t.pop_back();
              L.push_back({ t, (int)st });
              if (e == wstring::npos) break;
              st = e + 1;
          } }
        size_t n = L.size();
        for (size_t i = 0; i < n;) {
            wstring t = L[i].t; size_t rem = 0;
            int qd = StripQuote(t, rem);
            int base = L[i].start + (int)rem;
            if (Blank(t)) { i++; continue; }
            wchar_t fc;
            if (Fence(t, fc)) {
                MdBlock b; b.type = MdBlock::CODE; b.quote = qd;
                size_t j = i + 1; wstring code;
                for (; j < n; j++) {
                    wstring u = L[j].t; size_t r2 = 0; StripQuote(u, r2);
                    wchar_t f2;
                    if (Fence(u, f2) && f2 == fc) { j++; break; }
                    if (!code.empty() || j > i + 1) code += L'\n';
                    code += u;
                }
                b.code = code; blocks.push_back(b); i = j; continue;
            }
            size_t ts = 0;
            int hl = Heading(t, ts);
            if (hl) {
                MdBlock b; b.type = MdBlock::HEAD; b.level = hl; b.quote = qd;
                wstring h = t.substr(ts);
                while (!h.empty() && (h.back() == L'#' || h.back() == L' ')) h.pop_back();
                Inline(h, b.spans, MdStyle());
                blocks.push_back(b); i++; continue;
            }
            if (IsHr(t)) { MdBlock b; b.type = MdBlock::HR; b.quote = qd; blocks.push_back(b); i++; continue; }
            int lvl; wstring mk; size_t cs;
            if (ListItem(t, lvl, mk, cs)) {
                MdBlock b; b.type = MdBlock::LIST; b.level = lvl; b.marker = mk; b.quote = qd;
                wstring body = t.substr(cs);
                if (body.size() >= 3 && body[0] == L'[' && body[2] == L']' && (body[1] == L' ' || body[1] == L'x' || body[1] == L'X') && (body.size() == 3 || body[3] == L' ')) {
                    b.task = (body[1] == L' ') ? 1 : 2; b.taskPos = base + (int)cs;
                    body = body.size() > 4 ? body.substr(4) : L"";
                }
                MdStyle st; if (b.task == 2) { st.s = true; st.dim = true; }
                Inline(body, b.spans, st);
                blocks.push_back(b); i++; continue;
            }
            if (t.find(L'|') != wstring::npos && i + 1 < n) {
                wstring u = L[i + 1].t; size_t r2 = 0; StripQuote(u, r2);
                if (TableSep(u) && u.find(L'|') != wstring::npos) {
                    MdBlock b; b.type = MdBlock::TABLE; b.quote = qd;
                    vector<wstring> head = Cells(t);
                    for (auto& a : Cells(u)) { bool l = !a.empty() && a[0] == L':', r = !a.empty() && a.back() == L':'; b.align.push_back(l && r ? 1 : r ? 2 : 0); }
                    size_t cols = head.size();
                    auto addRow = [&](const vector<wstring>& cells, bool header) {
                        vector<vector<MdSpan>> row;
                        for (size_t c = 0; c < cols; c++) {
                            vector<MdSpan> sp; MdStyle st; st.b = header;
                            Inline(c < cells.size() ? cells[c] : L"", sp, st);
                            row.push_back(sp);
                        }
                        b.cells.push_back(row);
                    };
                    addRow(head, true);
                    size_t j = i + 2;
                    for (; j < n; j++) {
                        wstring v = L[j].t; size_t r3 = 0; StripQuote(v, r3);
                        if (Blank(v) || v.find(L'|') == wstring::npos) break;
                        addRow(Cells(v), false);
                    }
                    while (b.align.size() < cols) b.align.push_back(0);
                    blocks.push_back(b); i = j; continue;
                }
            }
            // paragraph (with setext headings)
            MdBlock b; b.type = MdBlock::PARA; b.quote = qd;
            wstring text = t; size_t j = i + 1; int setext = 0;
            for (; j < n; j++) {
                wstring u = L[j].t; size_t r2 = 0; int q2 = StripQuote(u, r2);
                if (q2 != qd || Blank(u)) break;
                size_t a = u.find_first_not_of(L' '), z = u.find_last_not_of(L' ');
                if (a != wstring::npos) {
                    wstring core = u.substr(a, z - a + 1);
                    if (core.find_first_not_of(L'=') == wstring::npos) { setext = 1; j++; break; }
                    if (core.find_first_not_of(L'-') == wstring::npos) { setext = 2; j++; break; }
                }
                wchar_t f2; size_t ts2; int l2; wstring m2; size_t c2;
                if (Fence(u, f2) || Heading(u, ts2) || IsHr(u) || ListItem(u, l2, m2, c2)) break;
                bool hard = text.size() >= 2 && text.compare(text.size() - 2, 2, L"  ") == 0;
                bool bs = !text.empty() && text.back() == L'\\';
                if (bs) text.pop_back();
                text += (hard || bs) ? L"\n" : L" ";
                text += Trim(u);
            }
            if (setext) { b.type = MdBlock::HEAD; b.level = setext; }
            Inline(Trim(text), b.spans, MdStyle());
            blocks.push_back(b); i = j;
        }
    }

    // ------------------------------------------------------------ layout
    struct LI { wstring text; MdStyle st; int key; int w; int asc; int h; bool space; };

    void Flow(HDC dc, const vector<MdSpan>& spans, int x0, int x1, int& y, int size, bool boldAll, bool wrap, int align) {
        vector<LI> line; int x = x0;
        const int LS = std::max(2, size / 4);
        auto flush = [&]() {
            while (!line.empty() && line.back().space) { x -= line.back().w; line.pop_back(); }
            int asc = 0, h = 0;
            for (auto& li : line) { asc = std::max(asc, li.asc); h = std::max(h, li.h); }
            if (line.empty()) { const F& f = Font(dc, size, false, false, false); asc = f.asc; h = f.h; }
            int lh = h + LS;
            int cx = x0;
            if (align) { int used = x - x0, avail = x1 - x0; cx = x0 + (align == 1 ? (avail - used) / 2 : (avail - used)); if (cx < x0) cx = x0; }
            for (auto& li : line) {
                if (li.st.code || li.st.hl) {
                    MdOp f; f.type = OP_FILL; f.r = { cx - 2, y + 1, cx + li.w + 2, y + lh - 1 }; f.round = 5;
                    f.col = li.st.hl ? 0xFFFFFFFF : 0xFFFFFFFE;   // placeholders resolved at draw time
                    ops.push_back(f);
                }
                MdOp o; o.type = OP_TEXT; o.r = { cx, y, std::min(cx + li.w, x1 + (wrap ? 0 : 0)), y + lh };
                o.by = y + asc + LS / 2; o.text = li.text; o.st = li.st; o.fkey = li.key;
                if (!wrap) o.r.right = std::min(cx + li.w, x1);
                ops.push_back(o);
                cx += li.w;
            }
            y += lh; line.clear(); x = x0;
        };
        for (auto& sp : spans) {
            const MdStyle& st = sp.st;
            bool bold = st.b || boldAll;
            int fs = st.code ? std::max(8, size * 92 / 100) : size;
            const F& f = Font(dc, fs, bold, st.i, st.code);
            int key = Key(fs, bold, st.i, st.code);
            HGDIOBJ old = SelectObject(dc, f.f);
            const wstring& t = sp.text;
            size_t i = 0;
            while (i < t.size()) {
                if (t[i] == L'\n') { flush(); i++; continue; }
                size_t j = i; bool isSp = iswspace(t[i]) != 0;
                wstring tok;
                if (isSp) { while (j < t.size() && t[j] != L'\n' && iswspace(t[j])) j++; tok = L" "; }
                else { while (j < t.size() && !iswspace(t[j])) j++; tok = t.substr(i, j - i); }
                i = j;
                if (isSp && line.empty()) continue;
                SIZE sz; GetTextExtentPoint32W(dc, tok.c_str(), (int)tok.size(), &sz);
                if (wrap && !isSp && x + sz.cx > x1 && !line.empty()) flush();
                while (wrap && !isSp && sz.cx > x1 - x0 && tok.size() > 1) {   // very long word: break it
                    int fit = 0; SIZE s2;
                    GetTextExtentExPointW(dc, tok.c_str(), (int)tok.size(), x1 - x, &fit, nullptr, &s2);
                    if (fit < 1) fit = 1;
                    if (fit >= (int)tok.size()) break;
                    wstring part = tok.substr(0, fit);
                    GetTextExtentPoint32W(dc, part.c_str(), fit, &s2);
                    line.push_back({ part, st, key, s2.cx, f.asc, f.h, false }); x += s2.cx;
                    flush();
                    tok = tok.substr(fit);
                    GetTextExtentPoint32W(dc, tok.c_str(), (int)tok.size(), &sz);
                }
                line.push_back({ tok, st, key, sz.cx, f.asc, f.h, isSp });
                x += sz.cx;
            }
            SelectObject(dc, old);
        }
        if (!line.empty()) flush();
    }

    void Layout(HDC dc, int width, int basePx) {
        ops.clear();
        int pad = basePx, gap = basePx * 6 / 10;
        int x0 = pad, x1 = std::max(pad * 3, width - pad);
        int y = pad;
        static const int hs[7] = { 0, 200, 170, 140, 120, 108, 100 };
        for (size_t bi = 0; bi < blocks.size(); bi++) {
            MdBlock& b = blocks[bi];
            int bx0 = x0 + b.quote * basePx;
            size_t barIdx = (size_t)-1; int barTop = y;
            if (b.quote > 0) {
                barIdx = ops.size();
                MdOp bar; bar.type = OP_FILL; bar.r = { x0, y, x0 + std::max(2, basePx / 5), y }; bar.col = 0xFFFFFFFD; ops.push_back(bar);
            }
            switch (b.type) {
                case MdBlock::HEAD: {
                    int size = basePx * hs[std::min(std::max(b.level, 1), 6)] / 100;
                    if (y > pad) y += gap;
                    Flow(dc, b.spans, bx0, x1, y, size, true, true, 0);
                    if (b.level <= 2) { y += gap / 3; MdOp r; r.type = OP_FILL; r.r = { bx0, y, x1, y + 1 }; r.col = 0xFFFFFFFC; ops.push_back(r); y += 1; }
                    y += gap / 2;
                    break;
                }
                case MdBlock::PARA:
                    Flow(dc, b.spans, bx0, x1, y, basePx, false, true, 0);
                    y += gap;
                    break;
                case MdBlock::LIST: {
                    int ind = bx0 + b.level * basePx * 3 / 2;
                    int mw = basePx * 3 / 2;
                    const F& f = Font(dc, basePx, false, false, false);
                    int lineTop = y;
                    int tx;
                    if (b.task) {
                        int bs = basePx;
                        MdOp c; c.type = OP_CHECK; c.r = { ind, y + (f.h - bs) / 2 + 2, ind + bs, y + (f.h - bs) / 2 + 2 + bs };
                        c.checked = (b.task == 2); c.taskPos = b.taskPos; ops.push_back(c);
                        tx = ind + bs + basePx / 2;
                    } else {
                        MdOp m; m.type = OP_TEXT; m.r = { ind, y, ind + mw, y + f.h }; m.by = y + f.asc + 2;
                        wstring mk = b.marker;
                        if (mk == L"\u2022" && b.level == 1) mk = L"\u25E6"; else if (mk == L"\u2022" && b.level >= 2) mk = L"\u25AA";
                        m.text = mk; m.fkey = Key(basePx, false, false, false); m.st.dim = true; ops.push_back(m);
                        tx = ind + mw;
                    }
                    (void)lineTop;
                    Flow(dc, b.spans, tx, x1, y, basePx, false, true, 0);
                    y += gap / 3;
                    break;
                }
                case MdBlock::CODE: {
                    int fs = std::max(8, basePx * 92 / 100);
                    const F& f = Font(dc, fs, false, false, true);
                    vector<wstring> lines = Split(b.code, L'\n');
                    int cp = basePx * 7 / 10, lh = f.h + 2;
                    int h = (int)lines.size() * lh + cp * 2;
                    MdOp bg; bg.type = OP_FILL; bg.r = { bx0, y, x1, y + h }; bg.col = 0xFFFFFFFE; bg.round = 8; ops.push_back(bg);
                    int ty = y + cp;
                    for (auto& ln : lines) {
                        MdOp o; o.type = OP_TEXT; o.r = { bx0 + cp, ty, x1 - cp, ty + lh }; o.by = ty + f.asc; o.text = ln;
                        o.fkey = Key(fs, false, false, true); ops.push_back(o); ty += lh;
                    }
                    y += h + gap;
                    break;
                }
                case MdBlock::HR: {
                    y += gap / 2;
                    MdOp r; r.type = OP_FILL; r.r = { bx0, y, x1, y + 2 }; r.col = 0xFFFFFFFC; ops.push_back(r);
                    y += 2 + gap;
                    break;
                }
                case MdBlock::TABLE: {
                    size_t cols = b.cells.empty() ? 0 : b.cells[0].size();
                    if (!cols) break;
                    vector<int> w(cols, 0);
                    int cpad = basePx * 6 / 10;
                    for (auto& row : b.cells)
                        for (size_t c = 0; c < cols; c++) {
                            int tw = 0;
                            for (auto& sp : row[c]) {
                                const F& f = Font(dc, sp.st.code ? basePx * 92 / 100 : basePx, sp.st.b, sp.st.i, sp.st.code);
                                HGDIOBJ o = SelectObject(dc, f.f); SIZE sz; GetTextExtentPoint32W(dc, sp.text.c_str(), (int)sp.text.size(), &sz); SelectObject(dc, o);
                                tw += sz.cx;
                            }
                            w[c] = std::max(w[c], tw + cpad * 2);
                        }
                    int total = 0; for (int v : w) total += v;
                    int avail = x1 - bx0;
                    if (total > avail && total > 0) for (auto& v : w) v = std::max(basePx * 3, v * avail / total);
                    total = 0; for (int v : w) total += v;
                    const F& bf = Font(dc, basePx, false, false, false);
                    int rh = bf.h + cpad * 2 / 1;
                    int ty = y;
                    for (size_t r = 0; r < b.cells.size(); r++) {
                        if (r == 0) { MdOp hd; hd.type = OP_FILL; hd.r = { bx0, ty, bx0 + total, ty + rh }; hd.col = 0xFFFFFFFB; ops.push_back(hd); }
                        int cx = bx0;
                        for (size_t c = 0; c < cols; c++) {
                            int ly = ty + cpad / 2 + 1;
                            Flow(dc, b.cells[r][c], cx + cpad, cx + w[c] - cpad, ly, basePx, false, false, b.align[c]);
                            cx += w[c];
                        }
                        ty += rh;
                        MdOp ln; ln.type = OP_FILL; ln.r = { bx0, ty - 1, bx0 + total, ty }; ln.col = 0xFFFFFFFA; ops.push_back(ln);
                    }
                    { int cx = bx0;
                      for (size_t c = 0; c <= cols; c++) {
                          MdOp ln; ln.type = OP_FILL; ln.r = { cx, y, cx + 1, ty }; ln.col = 0xFFFFFFFA; ops.push_back(ln);
                          if (c < cols) cx += w[c];
                      } }
                    MdOp top; top.type = OP_FILL; top.r = { bx0, y, bx0 + total, y + 1 }; top.col = 0xFFFFFFFA; ops.push_back(top);
                    y = ty + gap;
                    break;
                }
            }
            if (barIdx != (size_t)-1) { ops[barIdx].r.top = barTop; ops[barIdx].r.bottom = y - gap / 2; }
        }
        height = y + pad;
    }

    // ------------------------------------------------------------ drawing
    COLORREF Resolve(COLORREF c, const MdTheme& t) const {
        switch (c) {
            case 0xFFFFFFFF: return t.hl;
            case 0xFFFFFFFE: return t.codeBg;
            case 0xFFFFFFFD: return t.bar;
            case 0xFFFFFFFC: return t.rule;
            case 0xFFFFFFFB: return t.tableHead;
            case 0xFFFFFFFA: return t.tableLine;
        }
        return c;
    }

    void Draw(HDC dc, RECT area, int scroll, const MdTheme& t) {
        int sc = SaveDC(dc);
        IntersectClipRect(dc, area.left, area.top, area.right, area.bottom);
        SetBkMode(dc, TRANSPARENT);
        int oy = area.top - scroll, ox = area.left;
        for (auto& op : ops) {
            if (op.r.bottom + oy < area.top - 4 || op.r.top + oy > area.bottom + 4) continue;
            RECT r = { op.r.left + ox, op.r.top + oy, op.r.right + ox, op.r.bottom + oy };
            if (op.type == OP_FILL) {
                HBRUSH br = CreateSolidBrush(Resolve(op.col, t));
                if (op.round) {
                    HGDIOBJ ob = SelectObject(dc, br), opn = SelectObject(dc, GetStockObject(NULL_PEN));
                    RoundRect(dc, r.left, r.top, r.right + 1, r.bottom + 1, op.round, op.round);
                    SelectObject(dc, ob); SelectObject(dc, opn);
                } else FillRect(dc, &r, br);
                DeleteObject(br);
            } else if (op.type == OP_CHECK) {
                HPEN pen = CreatePen(PS_SOLID, 2, op.checked ? t.accent : t.dim);
                HBRUSH br = op.checked ? CreateSolidBrush(t.accent) : (HBRUSH)GetStockObject(NULL_BRUSH);
                HGDIOBJ op1 = SelectObject(dc, pen), ob = SelectObject(dc, br);
                RoundRect(dc, r.left, r.top, r.right, r.bottom, 5, 5);
                if (op.checked) {
                    HPEN w = CreatePen(PS_SOLID, 2, RGB(255, 255, 255));
                    SelectObject(dc, w);
                    int s = r.right - r.left;
                    MoveToEx(dc, r.left + s * 22 / 100, r.top + s * 52 / 100, nullptr);
                    LineTo(dc, r.left + s * 43 / 100, r.top + s * 72 / 100);
                    LineTo(dc, r.left + s * 79 / 100, r.top + s * 30 / 100);
                    SelectObject(dc, pen); DeleteObject(w);
                }
                SelectObject(dc, op1); SelectObject(dc, ob);
                DeleteObject(pen); if (op.checked) DeleteObject(br);
            } else if (op.type == OP_TEXT) {
                auto it = fonts.find(op.fkey);
                if (it == fonts.end()) continue;
                SelectObject(dc, it->second.f);
                COLORREF col = op.st.link >= 0 ? t.link : (op.st.dim ? t.dim : t.text);
                SetTextColor(dc, col);
                SetTextAlign(dc, TA_LEFT | TA_BASELINE);
                RECT clip = r;
                ExtTextOutW(dc, r.left, op.by + oy, ETO_CLIPPED, &clip, op.text.c_str(), (UINT)op.text.size(), nullptr);
                SetTextAlign(dc, TA_LEFT | TA_TOP);
                SIZE sz; GetTextExtentPoint32W(dc, op.text.c_str(), (int)op.text.size(), &sz);
                int w = std::min((int)sz.cx, (int)(r.right - r.left));
                if (op.st.link >= 0 || op.st.s) {
                    HPEN pen = CreatePen(PS_SOLID, 1, col); HGDIOBJ o = SelectObject(dc, pen);
                    int ly = op.st.s ? (op.by + oy - it->second.asc / 3) : (op.by + oy + 2);
                    MoveToEx(dc, r.left, ly, nullptr); LineTo(dc, r.left + w, ly);
                    SelectObject(dc, o); DeleteObject(pen);
                }
            }
        }
        RestoreDC(dc, sc);
    }

    // hit tests use layout coordinates (already minus the viewport origin and plus the scroll)
    int HitLink(int x, int y) const {
        for (auto& op : ops)
            if (op.type == OP_TEXT && op.st.link >= 0 && x >= op.r.left && x < op.r.right && y >= op.r.top && y < op.r.bottom) return op.st.link;
        return -1;
    }
    int HitTask(int x, int y) const {
        for (auto& op : ops)
            if (op.type == OP_CHECK && op.taskPos >= 0 && x >= op.r.left - 4 && x < op.r.right + 4 && y >= op.r.top - 4 && y < op.r.bottom + 4) return op.taskPos;
        return -1;
    }
};
