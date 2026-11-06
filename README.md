# Quick Palette

A tiny offline launcher for Windows: apps, settings, files in your own folders, commands, bangs, calculator, unit conversion.

## What it does and does not do
- **No network code.** The program links no networking libraries and never opens a connection.
  Web "bangs" just hand a URL to your default browser, exactly like typing it yourself.
- **No telemetry, no accounts, no updates, no downloads.**
- **No keyboard hooks, no clipboard watching, no screen capture, no injection into other processes.**
  The only keyboard feature is one registered hotkey (default Ctrl+Space).
- **Files it touches:** only `%AppData%\QuickPalette\` (config.ini, usage.txt, hidden.txt, apps.txt, files.txt).
- **Registry:** writes one value (`HKCU\...\Run\QuickPalette`) ONLY if you answer Yes to the first-run question
  or turn "Start with Windows" on. Turning it off deletes the value.
- **File search** indexes file and folder *names* only (never contents), only in folders you choose
  (default: Desktop, Documents, Downloads), in the background at low priority.
- Source is one readable file: `main.cpp`.

## Use
- `Ctrl+Space` opens it. Type to search apps, Windows settings, commands.
- `f report` file search · `g cats` / `yt lofi` bangs · `10 km to mi` · `2*(3+4)` · `date` · `guid` · `> notepad file.txt`
- `Ctrl+Enter` shows a file in its folder · `Ctrl+H` or right-click hides an item
- Settings: tray icon > Edit settings (config.ini)
