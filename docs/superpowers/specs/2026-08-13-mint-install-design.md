# Per-User Install Target (Linux Mint) — Design

## Summary

Add a `make install` / `make uninstall` target so the app can be installed
for the current user (no sudo) and shows up in the Cinnamon application
menu, plus a short Mint-specific "Install" section in the README.

## Goals

- `make install` builds the binary if needed, then installs it for the
  current user only — no sudo required.
- After install, the app appears in the Cinnamon (and any other
  freedesktop-compliant) application menu with an icon, and can also be
  launched from a terminal by typing `crosshair-overlay`.
- `make uninstall` cleanly removes everything `install` added.
- No new asset/dependency requirements — reuse an existing stock icon
  theme name rather than shipping a custom icon file (YAGNI).

## Non-goals

- No system-wide install (`/usr/local/bin`, requires sudo) — out of scope
  per the per-user decision.
- No autostart-on-login entry — not requested; can be added later if wanted.
- No `.deb` package — a plain `make install` is enough for a friend trying
  the app out; packaging is a separate, larger effort if ever wanted.

## Design

**New file: `crosshair-overlay.desktop`**

A standard freedesktop `.desktop` entry:

```ini
[Desktop Entry]
Type=Application
Name=Crosshair Overlay
Comment=Lightweight click-through crosshair overlay
Exec=%h/.local/bin/crosshair-overlay
Icon=input-mouse
Terminal=false
Categories=Utility;
```

`%h` is not a real freedesktop macro, so the file installed to
`~/.local/share/applications/` must have the literal absolute path
substituted at install time (the Makefile does this with `sed`, writing
the substituted copy to the install destination — the source file in the
repo keeps a placeholder `@HOME@` instead of `%h` for clarity).

**Makefile additions:**

```makefile
PREFIX = $(HOME)/.local
BINDIR = $(PREFIX)/bin
DESKTOPDIR = $(PREFIX)/share/applications

install: all
	@mkdir -p $(BINDIR) $(DESKTOPDIR)
	install -m 755 $(BIN) $(BINDIR)/crosshair-overlay
	sed 's|@HOME@|$(HOME)|' crosshair-overlay.desktop > $(DESKTOPDIR)/crosshair-overlay.desktop
	-update-desktop-database $(DESKTOPDIR) 2>/dev/null

uninstall:
	rm -f $(BINDIR)/crosshair-overlay
	rm -f $(DESKTOPDIR)/crosshair-overlay.desktop
	-update-desktop-database $(DESKTOPDIR) 2>/dev/null
```

The leading `-` on `update-desktop-database` means `make` ignores its exit
code — it's a nice-to-have (refreshes the menu immediately) but its
absence (e.g. a minimal Mint install without it) must not fail the install.

**README addition:** a new "Install (Linux Mint / Ubuntu-based)" section
right after "Build", showing `make install`, noting it appears in the
Cinnamon menu afterward, and `make uninstall` to remove it.

## Testing

Manual, on the user's/friend's actual Mint machine — added as new lines in
the existing README manual verification checklist:

- [ ] `make install` succeeds without sudo and without errors.
- [ ] "Crosshair Overlay" appears in the Cinnamon application menu with an icon.
- [ ] Launching it from the menu, and from a terminal via `crosshair-overlay` (with `~/.local/bin` on `$PATH`), both work.
- [ ] `make uninstall` removes the binary and the menu entry.
