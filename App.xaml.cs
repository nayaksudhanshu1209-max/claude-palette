using System.Threading;
using System.Windows;
using Drawing = System.Drawing;
using Forms = System.Windows.Forms;

namespace QuickPalette;

public partial class App : Application
{
    public static Config Cfg { get; set; } = new();
    public static MainWindow Palette { get; private set; } = null!;

    Mutex? _mutex;
    Forms.NotifyIcon? _tray;

    protected override void OnStartup(StartupEventArgs e)
    {
        base.OnStartup(e);

        _mutex = new Mutex(true, "QuickPalette.SingleInstance.v1", out bool first);
        if (!first) { Shutdown(); return; }

        Cfg = Config.Load();
        Catalog.Rebuild();

        Palette = new MainWindow();
        Palette.Init();
        BuildTray();

        if (!Palette.RegisterHotkey())
            _tray!.ShowBalloonTip(5000, "Quick Palette",
                $"The shortcut {Cfg.Hotkey} is used by another app. Choose a different one in Settings.",
                Forms.ToolTipIcon.Warning);

        Catalog.RefreshApps(false);
    }

    void BuildTray()
    {
        var menu = new Forms.ContextMenuStrip();
        menu.Items.Add("Open palette", null, (_, _) => Palette.ShowPalette());
        menu.Items.Add("Settings", null, (_, _) => SettingsWindow.ShowSingle());
        menu.Items.Add("Reload app list", null, (_, _) => Catalog.RefreshApps(true));
        menu.Items.Add(new Forms.ToolStripSeparator());
        menu.Items.Add("Quit", null, (_, _) => Shutdown());

        _tray = new Forms.NotifyIcon
        {
            Icon = Drawing.SystemIcons.Application,
            Text = "Quick Palette",
            Visible = true,
            ContextMenuStrip = menu
        };
        _tray.MouseClick += (_, a) =>
        {
            if (a.Button == Forms.MouseButtons.Left) Palette.ShowPalette();
        };
    }

    protected override void OnExit(ExitEventArgs e)
    {
        if (_tray != null) { _tray.Visible = false; _tray.Dispose(); }
        base.OnExit(e);
    }
}
