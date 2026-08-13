# Crosshair Overlay for Linux — Design

## Summary

A lightweight, always-on-top, click-through crosshair overlay for Linux. Configured
through a GUI options window (opened from a system tray icon), with import/export
of presets as JSON files, and a rebindable global hotkey (2+ keys) to toggle
visibility even while a game has focus.

## Goals

- Very low idle memory usage.
- Works across Linux distros/desktop environments (X11 primarily; most Proton/DX12
  games run under XWayland even on Wayland sessions).
- Fully free/open-source toolchain — no proprietary or licensed dependencies.
- Click-through: the overlay never intercepts mouse input.
- Always-on-top: stays above the target window/game.
- Configurable via GUI, not manual file editing.
- Import/export crosshair presets as shareable JSON files.

## Non-goals

- No support for true fullscreen-exclusive rendering paths that bypass the X11
  compositor entirely — this is a fundamental Linux/X11 limitation shared by every
  overlay tool (including Steam's own overlay), not something this app can work
  around. Borderless/windowed-fullscreen (the default or common option for most
  Proton/DX12 titles) works fine.
- No reading of game memory, process injection, or any interaction with the target
  application beyond drawing pixels on top of the screen. Purely a visual overlay.
- No Wayland-native (layer-shell) support in v1. If needed later, this is a
  separate follow-up (compositor-specific, e.g. `wlr-layer-shell` on
  wlroots-based compositors), out of scope here.

## Tech stack

- **C**, built with a plain **Makefile** (via `pkg-config`).
- **GTK3** — windows, tray icon, options UI widgets.
- **Cairo** — drawing the crosshair shapes.
- **X11 / Xext (Shape extension)** — click-through (empty input region) and
  always-on-top/override-redirect window behavior; `XGrabKey` for the global hotkey.
- **json-glib** — reading/writing the JSON config format.

All dependencies are free, open-source, and available in the default repositories
of virtually every Linux distribution (Debian/Ubuntu, Fedora, Arch, etc.).

## Architecture

Six modules:

1. **`main.c`** — entry point. Loads config, initializes GTK, creates the overlay
   window, tray icon, and hotkey listener, then runs the GTK main loop.

2. **`config.c` / `config.h`** — defines `CrosshairConfig` (see schema below) and:
   - `config_load()` / `config_save()` — auto-persist to
     `~/.config/crosshair-overlay/config.json` (debounced writes on change).
   - `config_import(path)` / `config_export(path)` — load/save an arbitrary JSON
     file for sharing presets. Import validates the JSON; on failure, shows an
     error dialog and leaves the current config untouched.

3. **`overlay_window.c` / `overlay_window.h`** — a borderless GTK window,
   positioned relative to the selected monitor's center (offset by X/Y from
   config), always-on-top and skip-taskbar. Click-through is achieved by setting
   an empty X11 input shape region on the window (`XShapeCombineRegion` with the
   `ShapeInput` kind). Redraws via a Cairo `draw` handler whenever the config
   changes.

4. **`tray.c` / `tray.h`** — system tray icon with a menu: **Enable/Disable**,
   **Options…**, **Quit**.

5. **`options_window.c` / `options_window.h`** — the GUI settings window (see
   layout below). All controls apply live to the overlay and trigger an autosave;
   there is no separate "Apply" button.

6. **`hotkey.c` / `hotkey.h`** — grabs the configured key combo globally via
   `XGrabKey` on the root window (works regardless of which window has focus,
   including a fullscreen game) and calls the toggle callback on match. Supports
   live rebinding: entering "capture mode" listens for the next key-down combo
   (minimum 2 keys) and saves it. If `XGrabKey` fails (combo already owned by
   another application), the options window shows a warning and the previous
   binding remains active.

## Config schema

Stored at `~/.config/crosshair-overlay/config.json`, and the same shape is used
for import/export:

```json
{
  "enabled": true,
  "monitor": 0,
  "offset_x": 0,
  "offset_y": 0,
  "size_percent": 100,
  "shape": "cross",
  "cross":  { "length": 10, "thickness": 2, "gap": 4, "color": "#00FF00", "opacity": 1.0 },
  "dot":    { "radius": 2, "color": "#00FF00", "opacity": 1.0 },
  "circle": { "radius": 8, "thickness": 2, "color": "#00FF00", "opacity": 1.0 },
  "hotkey": ["Ctrl", "Alt", "X"]
}
```

- `offset_x` / `offset_y`: pixel offset from the center of the selected monitor
  (0,0 = dead center).
- `size_percent`: uniform scale factor applied to whichever shape's dimensions are
  active (100 = the raw values above, e.g. 200 = double size).
- `shape`: which of `cross` / `dot` / `circle` is currently displayed. (Only one
  shape is active at a time, selected via the dropdown; each keeps its own
  settings so switching back and forth doesn't lose your tuning.)
- `hotkey`: ordered list of key names for the toggle combo; minimum 2 entries.

## Options window layout

Opened via tray → **Options…**:

- **Position** — X/Y spin buttons (0,0 = screen center), live-updating the overlay.
- **Size** — slider/spinbox for size % (100% = normal).
- **Monitor** — dropdown listing detected monitors.
- **Shape** — dropdown (Cross / Dot / Circle), revealing that shape's sub-controls
  (length/thickness/gap for cross; radius for dot; radius/thickness for circle).
- **Color & Opacity** — GTK RGBA color picker + opacity slider, applying to the
  currently selected shape.
- **Hotkey** — shows the current combo (e.g. "Ctrl+Alt+X") with a **Rebind**
  button; clicking it enters capture mode for the next 2+ key combo.
- **Import / Export** — buttons opening file-choosers for loading/saving a
  `.json` preset.
- **Enable/Disable** — checkbox mirroring the tray toggle.

All changes apply immediately (overlay redraw, live hotkey re-grab) and autosave
to disk.

## Error handling

- **Malformed import file**: error dialog shown, current config untouched.
- **Hotkey grab failure** (combo already in use elsewhere): warning shown in the
  options window, previous binding stays active.
- **No monitors detected / monitor unplugged**: falls back to the primary monitor.
- **Config file missing/corrupt on startup**: falls back to built-in defaults
  (green cross, 100%, centered, `Ctrl+Alt+X`) rather than failing to launch.

## Testing

This is primarily a GUI/X11 application, so testing is largely manual against a
running desktop session:

- Config round-trip: save → reload → values match (can be a small automated unit
  test using json-glib, independent of the GUI).
- Manual verification checklist: click-through (clicks pass to the window below),
  always-on-top (stays above a maximized window and a borderless-fullscreen game),
  hotkey toggle works while a game has focus, multi-monitor centering, import of
  an exported preset reproduces the same crosshair.

## Known limitations

- True fullscreen-exclusive games (bypassing the X11 compositor) may hide the
  overlay — inherent to how Linux compositing works, not fixable in this app.
  Documented in-app (e.g. a note in the options window) so it's not a surprise.
- Wayland-native sessions without XWayland are unsupported in v1.
