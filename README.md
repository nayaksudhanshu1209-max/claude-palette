# Quick Palette

A tiny offline launcher for Windows 10/11: apps, settings, files in your own folders, commands, bangs,
calculator, unit conversion, and a Markdown note editor with live preview.

## What it does and does not do
- **No network code.** No networking libraries are linked. Web "bangs" just hand a URL to your default browser.
- **No telemetry, accounts, updates or downloads.**
- **No keyboard hooks, no clipboard watching, no screen capture, no injection into other processes.**
  The only global key is one registered hotkey (default Ctrl+Space).
- **Files it touches:** `%AppData%\QuickPalette\` (config.ini, usage.txt, hidden.txt, apps.txt, files.txt, note_usage.txt)
  and the note files YOU configure.
- **Registry:** one value (`HKCU\...\Run\QuickPalette`) ONLY if you answer Yes to the first-run question or turn startup on.
- **File search** indexes file/folder *names* only, only in folders you list, in the background at low priority.
- Source: `main.cpp` (core), `ui.h` (window + notes), `md.h` (Markdown renderer).

## Keys
| | |
|---|---|
| Ctrl+Space | open / close (change in config.ini) |
| Up / Down / Tab / PgUp / PgDn | move through results (mouse works too) |
| Enter / click | run |
| Ctrl+Enter | show a file in its folder |
| Ctrl+H or right-click | hide an item (Unhide: type "unhide") |
| Esc, click outside, run something | closes the palette (saves an open note first) |
| `f report` | file search |
| `g cats`, `yt lofi` ... | web bangs |
| `n ` (n + space) | note editor |
| `10 km to mi`, `2*(3+4)`, `date`, `guid`, `> notepad file.txt` | quick answers / run box |

## Notes (Markdown editor)
- Type your note trigger and a space (default `n `). The search box becomes a large editor with live preview.
- View buttons: Edit / Split / Preview (Ctrl+P cycles). Esc saves and closes. Ctrl+S saves. Autosaves every few seconds.
- Status bar: click a syntax chip to insert it at the cursor (wraps your selection). Chips are ordered by how often
  YOU use them, separately for each note file.
- Ctrl+B bold, Ctrl+I italic, Ctrl+K link. Enter continues lists and quotes. Task boxes in the preview are clickable.

## Settings
Tray icon > Edit settings opens `config.ini` (theme, compact, width, at_cursor, show_recent, folders, notes, bangs, ...).
