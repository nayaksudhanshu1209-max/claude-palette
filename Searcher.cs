using System;
using System.Collections.Generic;
using System.Data;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text.RegularExpressions;
using System.Windows;

namespace QuickPalette;

public static class Searcher
{
    static readonly Regex UrlRx = new(
        @"^(https?://\S+|localhost(:\d+)?(/\S*)?|[a-z0-9-]+(\.[a-z0-9-]+)*\.(com|org|net|io|dev|app|in|co|edu|gov|ai|me|tv|uk|de|fr|info|xyz)(:\d+)?(/\S*)?)$",
        RegexOptions.IgnoreCase);
    static readonly Regex CalcRx = new(@"^[0-9+\-*/().%\s,]+$");

    public static List<ResultItem> Query(string raw)
    {
        var cfg = App.Cfg;
        var hidden = new HashSet<string>(cfg.Hidden.Select(h => h.Id));
        var q = raw.Trim();

        // empty box: show what you use most
        if (q.Length == 0)
        {
            return Catalog.All
                .Where(i => !hidden.Contains(i.Id) && cfg.Usage.ContainsKey(i.Id))
                .OrderByDescending(i => cfg.Usage[i.Id])
                .Take(6)
                .Select(i => { i.Group = "Frequently used"; return i; })
                .ToList();
        }

        // bang: only the FIRST word counts, e.g. "yt lofi beats"
        var lead = raw.TrimStart();
        int sp = lead.IndexOf(' ');
        if (sp > 0)
        {
            var trig = lead[..sp];
            var bang = cfg.Bangs.FirstOrDefault(b => b.Trigger.Equals(trig, StringComparison.OrdinalIgnoreCase));
            if (bang != null) return new List<ResultItem> { BangItem(bang, lead[(sp + 1)..].Trim(), "Web search") };
        }

        var res = new List<ResultItem>();

        var calc = Calc(q);
        if (calc != null) res.Add(calc);

        if (UrlRx.IsMatch(q))
        {
            var url = q.StartsWith("http", StringComparison.OrdinalIgnoreCase) ? q
                    : (q.StartsWith("localhost", StringComparison.OrdinalIgnoreCase) ? "http://" : "https://") + q;
            res.Add(new ResultItem { Title = "Open " + q, Subtitle = "Open in your browser", Category = "Open", Group = "Open", Run = () => Proc.Open(url) });
        }
        try
        {
            if (q.Length > 2 && Path.IsPathRooted(q) && (File.Exists(q) || Directory.Exists(q)))
                res.Add(new ResultItem { Title = q, Subtitle = Directory.Exists(q) ? "Open folder" : "Open file", Category = "Open", Group = "Open", Run = () => Proc.Open(q) });
        }
        catch { }

        // indexed items, best category first
        var ql = q.ToLowerInvariant();
        var cand = Catalog.All
            .Where(i => !hidden.Contains(i.Id))
            .Select(i =>
            {
                int s = ScoreText(ql, i);
                if (s > 0 && cfg.Usage.TryGetValue(i.Id, out var u)) s += Math.Min(u, 15);
                i.Score = s; i.Group = i.Category;
                return i;
            })
            .Where(i => i.Score > 0)
            .ToList();

        foreach (var g in cand.GroupBy(i => i.Category).OrderByDescending(g => g.Max(i => i.Score)))
            res.AddRange(g.OrderByDescending(i => i.Score).ThenBy(i => i.Title.Length).Take(g.Key == "Apps" ? 6 : 4));

        // always offer a web search at the bottom
        var def = cfg.Bangs.FirstOrDefault(b => b.Trigger.Equals(cfg.DefaultBang, StringComparison.OrdinalIgnoreCase))
                  ?? cfg.Bangs.FirstOrDefault();
        if (def != null) res.Add(BangItem(def, q, "Web"));

        return res;
    }

    static ResultItem BangItem(Bang b, string text, string cat)
    {
        string Root()
        {
            try { return new Uri(b.Url.Replace("{q}", "")).GetLeftPart(UriPartial.Authority); }
            catch { return b.Url; }
        }
        return new ResultItem
        {
            Category = cat, Group = cat,
            Title = text.Length > 0 ? $"Search {b.Name} for “{text}”" : $"Search {b.Name}",
            Subtitle = text.Length > 0 ? "Web search" : "Keep typing to search directly on the site · Enter opens it",
            Run = () => Proc.Open(text.Length > 0 ? b.Url.Replace("{q}", Uri.EscapeDataString(text)) : Root())
        };
    }

    static ResultItem? Calc(string q)
    {
        if (!CalcRx.IsMatch(q) || !q.Any(char.IsDigit) || !q.Skip(1).Any(c => "+-*/%".Contains(c))) return null;
        try
        {
            using var dt = new DataTable();
            var v = Convert.ToDouble(dt.Compute(q.Replace(',', '.'), null), CultureInfo.InvariantCulture);
            if (double.IsNaN(v) || double.IsInfinity(v)) return null;
            var s = v.ToString("0.##########", CultureInfo.InvariantCulture);
            return new ResultItem
            {
                Title = "= " + s, Subtitle = "Calculator · Enter copies the result", Category = "Calculator", Group = "Calculator",
                Run = () => Clipboard.SetText(s)
            };
        }
        catch { return null; }
    }

    // ---------- scoring ----------
    static int ScoreText(string q, ResultItem it)
    {
        var t = it.TitleLower; var kw = it.KeywordsLower;
        int s = ScoreOne(q, t, kw);
        if (s > 0 || !q.Contains(' ')) return s;
        var toks = q.Split(' ', StringSplitOptions.RemoveEmptyEntries);
        int sum = 0;
        foreach (var tk in toks) { int x = ScoreOne(tk, t, kw); if (x == 0) return 0; sum += x; }
        return sum / toks.Length;
    }

    static int ScoreOne(string q, string t, string kw)
    {
        if (t == q) return 100;
        if (t.StartsWith(q, StringComparison.Ordinal)) return 85;
        foreach (var w in t.Split(' ', '-', '_', '.', '(', ')', '&'))
            if (w.Length > 0 && w.StartsWith(q, StringComparison.Ordinal)) return 70;
        if (t.Contains(q, StringComparison.Ordinal)) return 55;
        if (q.Length >= 2)
        {
            var ini = new string(t.Split(' ', '-', '_').Where(w => w.Length > 0).Select(w => w[0]).ToArray());
            if (ini.StartsWith(q, StringComparison.Ordinal)) return 50;   // "vsc" -> Visual Studio Code
        }
        if (kw.Contains(q, StringComparison.Ordinal)) return 35;
        if (q.Length >= 3 && IsSubsequence(q, t)) return 20;
        return 0;
    }

    static bool IsSubsequence(string q, string t)
    {
        int i = 0;
        foreach (var c in t) if (i < q.Length && c == q[i]) i++;
        return i == q.Length;
    }
}
