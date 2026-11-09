# Quick Palette

A tiny offline launcher for Windows 10/11: apps, settings, files in your own folders, commands, bangs,
calculator, unit conversion, and a Markdown note editor with live preview.

## What it does and does not do
- **The app itself has no network code.** Web "bangs" hand a URL to your default browser. The optional built-in browser
  (`w `) uses Microsoft's Edge WebView2 Runtime, and that runtime does the networking, not this program.
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
| `10 km to mi`, `2*(3+4)`, `sqrt(16)`, `255 to hex`, `date`, `guid`, `> notepad file.txt` | quick answers / run box |
| `#ff8800` or `rgb(255,136,0)` | color converter (HEX / RGB / HSL) |
| `upper`/`lower`/`title`/`len`/`b64`/`b64d`/`urlenc`/`urldec` + text | text tools |
| `timer 10m`, `remind 1h30m call mom`, `timer off` | timers and reminders (notification when done) |
| `sw chrome` | switch to an open window |
| `n+ buy milk` | quick capture: appends a line to your note without opening it |
| Alt+1 ... Alt+9 | run the numbered result |
| Ctrl+D | pin / unpin the selected item (pinned items show first when the box is empty) |
| Ctrl+Shift+C | copy the path of a file result |

## Built-in browser (`w `)
- Type `w` and a space: a start page appears instantly (tiles for your favorite sites, Alt+1..8 opens one) and the search box
  becomes an address bar. Type a site or search words (Google by default), press Enter. Esc closes it.
- The engine warms up while you pause on `w`, and stays warm for 45 s after closing so reopening is instant, then exits completely.
- Needs the Microsoft Edge WebView2 Runtime (already on most Windows 10/11 PCs).
- It exists only while open: closing it ends every browser process and (by default) wipes cookies, cache and history.
- Safety: https-only (http upgraded), certificate errors can't be clicked through, only http/https pages, no downloads,
  camera/mic/location/notification requests denied, no pop-ups, pages can't launch other programs, strict tracking prevention,
  built-in ad/tracker blocking, SmartScreen on, DevTools and password/autofill saving off.
- Memory: several `msedgewebview2.exe` processes are normal (browser, GPU, network, one per site). `web_lowram=1` trims them
  at the cost of isolation and speed.
- Extensions (such as uBlock Origin Lite): unzip the extension and add `web_extension=C:\path\to\folder` to config.ini.
  WebView2 only supports part of the extension system, so this is best-effort.
- Keys: Enter go, Alt+Left/Right back/forward, F5 reload, Ctrl+L address bar, Esc close. Command "Clear browser data".

## Notes (Markdown editor)
- Type your note trigger and a space (default `n `). The search box becomes a large editor with live preview.
- View buttons: Edit / Split / Preview (Ctrl+P cycles). Esc saves and closes. Ctrl+S saves. Autosaves every few seconds.
- Status bar: click a syntax chip to insert it at the cursor (wraps your selection). Chips are ordered by how often
  YOU use them, separately for each note file.
- Ctrl+B bold, Ctrl+I italic, Ctrl+K link. Enter continues lists and quotes. Task boxes in the preview are clickable.

## Settings
You can also type things like "theme dark", "compact", "animations" or "cursor" in the palette to switch options without editing the file.
Tray icon > Edit settings opens `config.ini` (theme, compact, width, at_cursor, show_recent, folders, notes, bangs, ...).
