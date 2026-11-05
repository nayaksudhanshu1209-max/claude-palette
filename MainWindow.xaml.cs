using System;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Input;
using System.Windows.Interop;
using System.Windows.Media;
using System.Windows.Threading;

namespace QuickPalette;

public partial class MainWindow : Window
{
    IntPtr _hwnd;

    public MainWindow()
    {
        InitializeComponent();
        Deactivated += (_, _) => HidePalette();
    }

    // ---------- setup ----------
    public void Init()
    {
        _hwnd = new WindowInteropHelper(this).EnsureHandle();
        HwndSource.FromHwnd(_hwnd)!.AddHook(WndProc);
        ApplyBackdrop();
    }

    void ApplyBackdrop()
    {
        int dark = 1;
        Native.DwmSetWindowAttribute(_hwnd, 20, ref dark, 4);          // dark title/frame
        if (!Native.IsWin11) return;                                   // Win10: solid dark fallback

        int round = 2;  Native.DwmSetWindowAttribute(_hwnd, 33, ref round, 4);  // rounded corners
        int acrylic = 3; Native.DwmSetWindowAttribute(_hwnd, 38, ref acrylic, 4); // acrylic backdrop
        var m = new Native.MARGINS { Left = -1, Right = -1, Top = -1, Bottom = -1 };
        Native.DwmExtendFrameIntoClientArea(_hwnd, ref m);
        HwndSource.FromHwnd(_hwnd)!.CompositionTarget.BackgroundColor = Colors.Transparent;
        Background = Brushes.Transparent;
        Root.Background = new SolidColorBrush(Color.FromArgb(0x78, 0x20, 0x20, 0x26));
    }

    public bool RegisterHotkey()
    {
        Native.UnregisterHotKey(_hwnd, 1);
        return HotkeyParser.TryParse(App.Cfg.Hotkey, out var mods, out var vk)
               && Native.RegisterHotKey(_hwnd, 1, mods | 0x4000, vk);   // 0x4000 = no auto-repeat
    }

    IntPtr WndProc(IntPtr hwnd, int msg, IntPtr w, IntPtr l, ref bool handled)
    {
        if (msg == 0x0312 && w.ToInt32() == 1)   // WM_HOTKEY
        {
            if (IsVisible) HidePalette(); else ShowPalette();
            handled = true;
        }
        return IntPtr.Zero;
    }

    // ---------- show / hide ----------
    public void ShowPalette()
    {
        SearchBox.Text = "";
        var wa = SystemParameters.WorkArea;
        Left = wa.Left + (wa.Width - Width) / 2;
        Top = wa.Top + wa.Height * 0.16;
        Refresh();
        Show();
        Activate();
        Native.SetForegroundWindow(_hwnd);
        SearchBox.Focus();
        Keyboard.Focus(SearchBox);
    }

    public void HidePalette()
    {
        if (!IsVisible) return;
        Hide();
        Dispatcher.BeginInvoke(new Action(Native.Trim), DispatcherPriority.ApplicationIdle);
    }

    // ---------- results ----------
    void SearchBox_TextChanged(object sender, TextChangedEventArgs e) => Refresh();

    void Refresh()
    {
        var items = Searcher.Query(SearchBox.Text);
        var view = new ListCollectionView(items);
        view.GroupDescriptions.Add(new PropertyGroupDescription(nameof(ResultItem.Group)));
        ResultList.ItemsSource = view;

        bool any = items.Count > 0;
        ResultList.Visibility = any ? Visibility.Visible : Visibility.Collapsed;
        Divider.Visibility = any ? Visibility.Visible : Visibility.Collapsed;
        if (any) ResultList.SelectedIndex = 0;
    }

    void Move(int d)
    {
        int n = ResultList.Items.Count;
        if (n == 0) return;
        int i = ResultList.SelectedIndex + d;
        if (i < 0) i = n - 1;
        if (i >= n) i = 0;
        ResultList.SelectedIndex = i;
        ResultList.ScrollIntoView(ResultList.SelectedItem);
    }

    void RunSelected()
    {
        if (ResultList.SelectedItem is not ResultItem it) return;
        HidePalette();

        if (it.Track)
        {
            App.Cfg.Usage[it.Id] = (App.Cfg.Usage.TryGetValue(it.Id, out var u) ? u : 0) + 1;
            App.Cfg.Save();
        }
        try { it.Run(); }
        catch (Exception ex) { MessageBox.Show(ex.Message, "Quick Palette"); }
    }

    void HideItem(ResultItem? it)
    {
        if (it == null || !it.Track) return;
        App.Cfg.Hidden.Add(new HiddenItem { Id = it.Id, Title = it.Title });
        App.Cfg.Save();
        int keep = ResultList.SelectedIndex;
        Refresh();
        if (ResultList.Items.Count > 0) ResultList.SelectedIndex = Math.Min(keep, ResultList.Items.Count - 1);
    }

    // ---------- input ----------
    protected override void OnPreviewKeyDown(KeyEventArgs e)
    {
        switch (e.Key)
        {
            case Key.Escape: HidePalette(); e.Handled = true; break;
            case Key.Down: Move(1); e.Handled = true; break;
            case Key.Up: Move(-1); e.Handled = true; break;
            case Key.Tab: Move((Keyboard.Modifiers & ModifierKeys.Shift) != 0 ? -1 : 1); e.Handled = true; break;
            case Key.Enter: RunSelected(); e.Handled = true; break;
            case Key.H when (Keyboard.Modifiers & ModifierKeys.Control) != 0:
                HideItem(ResultList.SelectedItem as ResultItem); e.Handled = true; break;
        }
        base.OnPreviewKeyDown(e);
    }

    void Item_Click(object sender, MouseButtonEventArgs e)
    {
        if (sender is ListBoxItem lbi && lbi.DataContext is ResultItem it)
        {
            ResultList.SelectedItem = it;
            RunSelected();
        }
    }

    void Hide_Click(object sender, RoutedEventArgs e)
    {
        if (sender is MenuItem mi && mi.Parent is ContextMenu cm &&
            (cm.PlacementTarget as FrameworkElement)?.DataContext is ResultItem it)
            HideItem(it);
    }
}
