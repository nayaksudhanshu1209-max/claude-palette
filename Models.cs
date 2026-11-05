using System;
using System.Collections.ObjectModel;
using System.Collections.Generic;
using System.IO;
using System.Text.Json;

namespace QuickPalette;

public class Bang
{
    public string Trigger { get; set; } = "";
    public string Name { get; set; } = "";
    public string Url { get; set; } = "";   // use {q} where the query goes
}

public class CustomCommand
{
    public string Name { get; set; } = "";
    public string Keywords { get; set; } = "";
    public string Target { get; set; } = "";     // exe, file, folder or URL
    public string Arguments { get; set; } = "";
}

public class HiddenItem
{
    public string Id { get; set; } = "";
    public string Title { get; set; } = "";
}

public class Config
{
    public string Hotkey { get; set; } = "Ctrl+Space";
    public bool RunAtStartup { get; set; }
    public string DefaultBang { get; set; } = "g";
    public ObservableCollection<Bang> Bangs { get; set; } = new();
    public ObservableCollection<CustomCommand> Commands { get; set; } = new();
    public ObservableCollection<HiddenItem> Hidden { get; set; } = new();
    public Dictionary<string, int> Usage { get; set; } = new();

    public static string Dir => Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "QuickPalette");
    static string FilePath => Path.Combine(Dir, "config.json");

    static readonly JsonSerializerOptions Opts = new() { WriteIndented = true };

    public static Config Load()
    {
        try
        {
            if (File.Exists(FilePath))
                return JsonSerializer.Deserialize<Config>(File.ReadAllText(FilePath), Opts) ?? new Config();
        }
        catch { }

        var c = new Config();
        void B(string t, string n, string u) => c.Bangs.Add(new Bang { Trigger = t, Name = n, Url = u });
        B("g", "Google", "https://www.google.com/search?q={q}");
        B("ddg", "DuckDuckGo", "https://duckduckgo.com/?q={q}");
        B("yt", "YouTube", "https://www.youtube.com/results?search_query={q}");
        B("gh", "GitHub", "https://github.com/search?q={q}");
        B("so", "Stack Overflow", "https://stackoverflow.com/search?q={q}");
        B("w", "Wikipedia", "https://en.wikipedia.org/w/index.php?search={q}");
        B("r", "Reddit", "https://www.reddit.com/search/?q={q}");
        B("maps", "Google Maps", "https://www.google.com/maps/search/{q}");
        B("a", "Amazon", "https://www.amazon.com/s?k={q}");
        B("tr", "Google Translate", "https://translate.google.com/?sl=auto&tl=en&text={q}");
        B("npm", "npm", "https://www.npmjs.com/search?q={q}");
        B("imdb", "IMDb", "https://www.imdb.com/find/?q={q}");
        c.Save();
        return c;
    }

    public void Save()
    {
        try
        {
            Directory.CreateDirectory(Dir);
            File.WriteAllText(FilePath, JsonSerializer.Serialize(this, Opts));
        }
        catch { }
    }
}

public class ResultItem
{
    public string Id { get; set; } = "";
    public string Title { get; set; } = "";
    public string Subtitle { get; set; } = "";
    public string Category { get; set; } = "";
    public string Group { get; set; } = "";
    public string Keywords { get; set; } = "";
    public bool Track { get; set; }          // indexed items: can be hidden + ranked by usage
    public int Score { get; set; }
    public Action Run { get; set; } = () => { };

    string? _glyph, _tl, _kl;
    public string Glyph
    {
        get => _glyph ?? Category switch
        {
            "Apps" => "\uE71D",
            "Settings" => "\uE713",
            "Commands" => "\uE756",
            "Calculator" => "\uE8EF",
            "Open" => "\uE8A7",
            _ => "\uE774"
        };
        set => _glyph = value;
    }

    public string Accent => Category switch
    {
        "Apps" => "#3B82F6",
        "Settings" => "#14B8A6",
        "Commands" => "#F59E0B",
        "Calculator" => "#EC4899",
        "Open" => "#64748B",
        _ => "#A855F7"
    };

    public string TitleLower => _tl ??= Title.ToLowerInvariant();
    public string KeywordsLower => _kl ??= Keywords.ToLowerInvariant();
}
