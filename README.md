# Crosshair Overlay for Linux

A lightweight, click-through, always-on-top crosshair overlay. Configure it
from a tray icon, toggle it with a global hotkey, and import/export presets
as JSON.

## Dependencies

Install the GTK3/X11/json-glib development headers for your distro:

**Debian/Ubuntu:**
```bash
sudo apt install build-essential pkg-config libgtk-3-dev libx11-dev libxext-dev libjson-glib-dev
```

**Fedora:**
```bash
sudo dnf install gcc make pkgconf-pkg-config gtk3-devel libX11-devel libXext-devel json-glib-devel
```

**Arch:**
```bash
sudo pacman -S base-devel pkgconf gtk3 libx11 libxext json-glib
```

## Build

```bash
make
```

Produces `bin/crosshair-overlay`.

## Run

```bash
./bin/crosshair-overlay
```

A tray icon appears. Click it for options: position, size, shape (cross/dot/
circle), color/opacity, monitor, hotkey, import/export, and enable/disable.

## Test

```bash
make test
```

Runs the config load/save round-trip test.

## Known limitation

True fullscreen-exclusive games (which bypass the X11 compositor) may hide
the overlay. This is a fundamental Linux/X11 limitation shared by every
overlay tool (including Steam's own overlay) — not fixable in this app.
Use borderless/windowed-fullscreen in your game's display settings instead.

## Manual verification checklist

After building, verify on your actual Linux desktop session:

- [ ] `./bin/crosshair-overlay` launches without errors and a tray icon appears.
- [ ] Clicking the tray icon shows Enable/Disable, Options…, Quit.
- [ ] Options… opens the settings window.
- [ ] Moving the X/Y spin buttons moves the crosshair; 0,0 is screen center.
- [ ] Changing size % scales the crosshair.
- [ ] Switching shape (Cross/Dot/Circle) redraws immediately with that shape's own settings.
- [ ] Changing color/opacity updates the crosshair immediately.
- [ ] Selecting a different monitor (if you have more than one) re-centers the crosshair there.
- [ ] Clicking on the overlay does nothing — clicks pass through to the window/game underneath (click-through).
- [ ] The crosshair stays visible above a maximized window and above a borderless-fullscreen game.
- [ ] Clicking "Rebind", then pressing a 2+ key combo (e.g. Ctrl+Alt+X), saves it and the label updates.
- [ ] Pressing the configured hotkey toggles the overlay on/off, even while another window (e.g. a game) has focus.
- [ ] Export saves the current settings to a chosen `.json` file; Import loads a previously exported file and reproduces the same crosshair.
- [ ] Importing a malformed/corrupt JSON file shows an error dialog and leaves the current config untouched.
- [ ] Quitting and relaunching the app restores your last settings (autosave/reload).
- [ ] Deleting `~/.config/crosshair-overlay/config.json` and relaunching falls back to defaults (green cross, 100%, centered, Ctrl+Alt+X) instead of crashing.
- [ ] The "Enabled" checkbox in Options and the tray's Enable/Disable both stay in sync with each other and with the hotkey toggle.
