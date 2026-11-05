using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;

namespace QuickPalette;

public partial class SettingsWindow : Window
{
    static SettingsWindow? _open;

    public static void ShowSingle()
    {
        if (_open == null)
        {
            _open = new SettingsWindow();
            _open.Closed += (_, _) => _open = null;
            _open.Show();
        }
        _open.Activate();
    }

    readonly ObservableCollection<Bang> _bangs;
    readonly ObservableCollection<CustomCommand> _cmds;
    readonly ObservableCollection<HiddenItem> _hidden;

    public SettingsWindow()
    {
        InitializeComponent();
        var c = App.Cfg;
        HotkeyBox.Text = c.Hotkey;
        StartupBox.IsChecked = c.RunAtStartup;
        DefaultBangBox.Text = c.DefaultBang;

        _bangs = new(c.Bangs.Select(b => new Bang { Trigger = b.Trigger, Name = b.Name, Url = b.Url }));
        _cmds = new(c.Commands.Select(x => new CustomCommand { Name = x.Name, Keywords = x.Keywords, Target = x.Target, Arguments = x.Arguments }));
        _hidden = new(c.Hidden.Select(h => new HiddenItem { Id = h.Id, Title = h.Title }));
        BangGrid.ItemsSource = _bangs;
        CmdGrid.ItemsSource = _cmds;
        HiddenList.ItemsSource = _hidden;
    }

    void Hotkey_KeyDown(object sender, KeyEventArgs e)
    {
        e.Handled = true;
        var key = e.Key == Key.System ? e.SystemKey : e.Key;
        if (key is Key.LeftCtrl or Key.RightCtrl or Key.LeftAlt or Key.RightAlt or Key.LeftShift
            or Key.RightShift or Key.LWin or Key.RWin or Key.None or Key.ImeProcessed) return;

        var m = Keyboard.Modifiers;
        var parts = new List<string>();
        if ((m & ModifierKeys.Control) != 0) parts.Add("Ctrl");
        if ((m & ModifierKeys.Alt) != 0) parts.Add("Alt");
        if ((m & ModifierKeys.Shift) != 0) parts.Add("Shift");
        if ((m & ModifierKeys.Windows) != 0) parts.Add("Win");
        if (parts.Count == 0) return;
        parts.Add(key.ToString());
        HotkeyBox.Text = string.Join("+", parts);
    }

    void Unhide_Click(object sender, RoutedEventArgs e)
    {
        if (HiddenList.SelectedItem is HiddenItem h) _hidden.Remove(h);
    }

    void Save_Click(object sender, RoutedEventArgs e)
    {
        BangGrid.CommitEdit(DataGridEditingUnit.Row, true);
        CmdGrid.CommitEdit(DataGridEditingUnit.Row, true);

        var c = App.Cfg;
        c.Hotkey = HotkeyBox.Text;
        c.RunAtStartup = StartupBox.IsChecked == true;
        c.DefaultBang = DefaultBangBox.Text.Trim();
        c.Bangs = new(_bangs.Where(b => !string.IsNullOrWhiteSpace(b.Trigger) && !string.IsNullOrWhiteSpace(b.Url)));
        c.Commands = new(_cmds.Where(x => !string.IsNullOrWhiteSpace(x.Name) && !string.IsNullOrWhiteSpace(x.Target)));
        c.Hidden = new(_hidden);
        c.Save();

        Startup.Set(c.RunAtStartup);
        Catalog.Rebuild();
        if (!App.Palette.RegisterHotkey())
            MessageBox.Show($"The shortcut {c.Hotkey} is already used by another app. Pick a different one.", "Quick Palette");
        else
            Close();
    }

    void Cancel_Click(object sender, RoutedEventArgs e) => Close();
    void Reload_Click(object sender, RoutedEventArgs e) => Catalog.RefreshApps(true);
    void Folder_Click(object sender, RoutedEventArgs e) => Proc.Open("explorer.exe", Config.Dir);
}
