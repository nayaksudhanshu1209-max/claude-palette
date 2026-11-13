# Quick Palette

A small, offline-first launcher for Windows 10/11: apps, Windows settings, files in your own folders, commands, bangs,
a calculator, timers, a live-formatting Markdown note window and an optional built-in browser.

## What it does and does not do
- **The app itself has no network code.** Web "bangs" hand a URL to your default browser. The optional built-in browser
  (`w `) uses Microsoft's Edge WebView2 Runtime, and that runtime does the networking, not this program.
- No telemetry, accounts, updates or downloads. No keyboard hooks, no clipboard watching, no screen capture.
- Files it touches: `%AppData%\QuickPalette\` and the note files YOU configure.
- Registry: one value (`HKCU\...\Run\QuickPalette`) ONLY if you answer Yes to the first-run question or turn startup on.
- File search reads file/folder NAMES only, only in the folders you list, in the background at low priority.
- Source: `main.cpp` (core), `ui.h` (palette), `note.h` (notes), `web.h` (browser).

## Palette
| | |
|---|---|
| Ctrl+Space | open / close (change `hotkey=`) |
| Up / Down / Tab / Shift+Tab / PgUp / PgDn | move through results (mouse works too) |
| Enter | run |
| Ctrl+Enter | show a file in its folder |
| Ctrl+Shift+C | copy a file's path |
| Ctrl+D | pin / unpin (pinned items show first when the box is empty) |
| Ctrl+H or right-click | hide an item (type "unhide" to bring all back) |
| Esc, click outside, run something | closes the palette |

Type: `f report` (file search) · `g cats` / `yt lofi` (web bangs) · `n ` (notes) · `w ` (browser) ·
`2*(3+4)`, `sqrt(16)` · `date` `time` `unix` `guid` · `> notepad file.txt` · `timer 10m`, `remind 1h30m call mom`, `timer off` ·
`n+ buy milk` (adds a line to your note without opening it).

## Notes (`n `)
- Opens its own window with no title bar: one live Markdown editor. Esc or Ctrl+W saves and closes. Clicking elsewhere does not close it.
- Formatting appears as you type. The syntax marks (`**`, `~~`, `` ` ``, `[](url)`, `#`) are hidden while the caret is elsewhere
  and shown dimmed while the caret is inside that piece of text.
- Auto-complete: typing `*` `_` `~` `=` `` ` `` `[` `(` completes the pair; typing `**` makes bold; typing the closing mark steps over it;
  Backspace on an empty pair removes both. Enter continues lists and quotes. Click a task box to tick it. Ctrl+click opens a link.
- Bottom bar: syntax chips (click to insert / wrap your selection), ordered by your use per note file. It scrolls sideways:
  mouse wheel, drag, the arrow buttons, or a horizontal wheel/touchpad.
- Move the window by dragging the thin strip above the text (or an empty part of the bottom bar); resize from the edges.
- Ctrl+S saves; it also autosaves a few seconds after you stop typing and when the window closes.

## Browser (`w `)
- There is no address bar: the palette is the address bar. Type `w `, then a site or search words (Google by default), press Enter.
  Empty after `w ` shows your bookmarks (`web_bookmark=Name|URL` lines in config.ini).
- The browser engine starts in the background the moment you type `w `, so Enter shows the page straight away.
- The browser window is only the web page: no tabs, no bars, no buttons. Esc or Ctrl+W close it, Alt+Left / Alt+Right back / forward,
  F5 reload, F11 maximize / restore, Ctrl+L opens the palette with `w ` ready for a new address.
- With the browser already open, searching from the palette just loads the new page in the same window.
- Security: private (in-memory) profile, https-only (http upgraded), certificate errors can't be clicked through, only http/https pages,
  no downloads, camera/mic/location/notification requests denied, no pop-ups, pages can't launch other programs, a public site
  cannot reach your PC or home network, strict tracking prevention, built-in ad/tracker blocking, SmartScreen on, DevTools and
  password/autofill off.
- Memory: it stays warm 45 s after closing (instant reopen), then exits completely. Several `msedgewebview2.exe` processes are normal.
  `web_lowram=1` trims them at the cost of isolation and speed.

## Settings
Tray icon > Edit settings opens `config.ini` (reloaded the next time you open the palette). Useful lines: `folder=` (file search),
`quick_folders=` (which quick-folder commands exist), `theme`, `compact`, `at_cursor`, `show_recent`, `animations`, `note=`, `web_*`.
You can also type "theme dark", "compact", "cursor", "animations" or "browser search" in the palette to switch options.
