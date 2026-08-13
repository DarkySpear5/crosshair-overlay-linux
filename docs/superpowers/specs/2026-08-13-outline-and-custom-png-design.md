# Outline Toggle & Custom PNG Crosshair — Design

## Summary

Two additions to the crosshair overlay:

1. A per-shape, toggleable **outline** (color + thickness) for Cross/Dot/Circle.
2. A **custom PNG crosshair** (max 16×16), imported as a 4th entry in the
   existing Shape dropdown ("Custom Image"), reusing the app's existing
   "exactly one shape is active" rule so it's impossible to end up with two
   crosshairs drawn on top of each other.

The Options window is reorganized into labeled sections (Position, Shape,
Appearance, Custom Image, Hotkey, Presets) so the new controls don't make
the window feel cluttered, and so Color/Opacity/Outline simply aren't shown
at all when Custom Image is the active shape, since they don't apply to a
raster image.

## Goals

- Outline is off by default, toggleable per shape, with its own color and
  thickness — consistent with how color/opacity already work per-shape.
- A user can import any PNG that is 16×16 or smaller as their crosshair.
- Exactly one crosshair is ever drawn — Custom Image is a 4th mutually
  exclusive Shape option, not a separate overlay layer.
- Switching to Custom Image hides the controls that don't apply
  (Color/Opacity/Outline for Cross/Dot/Circle) rather than graying them out,
  so the settings window stays simple to read at a glance.
- Custom PNG presets remain fully portable: the image travels inside the
  same JSON config/export file as everything else (base64-encoded), so
  sharing one preset file shares the exact custom crosshair too.

## Non-goals

- No outline around a custom PNG image — out of scope; outlining an
  arbitrary raster shape is a materially harder problem than outlining a
  vector line/circle, and wasn't asked for.
- No auto-downscaling of oversized PNGs — imports over 16×16 are rejected
  with an error message stating the actual dimensions, not resized.
- No animated/multi-frame image support (e.g. APNG/GIF) — a single static
  PNG only.

## Config schema changes

Each shape settings struct gains outline fields:

```json
"cross": {
  "length": 10, "thickness": 2, "gap": 4,
  "color": "#00FF00", "opacity": 1.0,
  "outline_enabled": false,
  "outline_color": "#000000",
  "outline_thickness": 1.0
}
```

(same three `outline_*` fields added to `dot` and `circle`)

`shape` gains a new possible value, `"custom_png"`. The top-level config
gains one new optional field:

```json
"custom_png_base64": "iVBORw0KGgoAAAANSUhEUgAAABAAAAAQ..."
```

Absent/empty when no PNG has been imported yet (matches how every other
optional field already falls back to a default when missing).

## Options window layout

Reorganized into `GtkFrame` sections:

- **Position** — X, Y, Size %, Monitor (unchanged; applies to every shape,
  including Custom Image).
- **Shape** — dropdown: Cross / Dot / Circle / Custom Image.
- **Appearance** (visible only when Shape ∈ {Cross, Dot, Circle}) — Color,
  Opacity, and a new Outline row: a checkbox, a color picker, and a
  thickness spinbox, all three enabled only when the checkbox is checked.
- **Custom Image** (visible only when Shape = Custom Image) — an
  "Import PNG…" button, a small preview thumbnail (the loaded image scaled
  up for visibility), and a label showing its dimensions once loaded
  (e.g. "12×12 loaded") or "No image loaded" beforehand.
- **Hotkey**, **Presets** (Import/Export), **Enabled** — unchanged.

Changing the Shape dropdown toggles which of Appearance/Custom Image is
shown; the other is hidden (not disabled — simply not present), keeping
the window focused on only the settings that currently apply.

## PNG import flow

1. "Import PNG…" opens a file chooser filtered to `*.png`.
2. Load the selected file with `GdkPixbuf`. Load failure (corrupt file,
   not actually a PNG) → error dialog, current config untouched.
3. Check `width <= 16 && height <= 16`. If not → error dialog stating the
   actual dimensions ("Image is 32×32 — must be 16×16 or smaller"), current
   config untouched.
4. Otherwise: read the file's raw bytes, base64-encode them into
   `custom_png_base64`, set `shape = SHAPE_CUSTOM_PNG`, apply to the
   overlay, save, and refresh the preview thumbnail + dimensions label.

## Rendering

**Outline (Cross/Dot/Circle):** a double-stroke technique — draw the shape
first at `thickness + 2*outline_thickness` using the outline color, then
draw it again on top at the normal `thickness` using the normal color,
producing a colored border. No new rendering dependencies.

**Custom PNG:** the overlay decodes `custom_png_base64` into a `GdkPixbuf`
once whenever the config changes (cached, not re-decoded every frame — the
overlay's `draw` handler can fire many times a second while nothing about
the image has changed), and draws it centered, scaled by Size %, using
nearest-neighbor filtering (`CAIRO_FILTER_NEAREST`) so a small pixel-art
image stays crisp instead of blurring when enlarged. If Custom Image is
selected but nothing has been imported yet, nothing is drawn (no crash, no
placeholder).

## Error handling

- PNG fails to load → error dialog, config untouched.
- PNG larger than 16×16 → error dialog with actual dimensions, config
  untouched.
- A preset JSON with a missing or corrupt `custom_png_base64` (e.g.
  hand-edited or truncated) → treated as "no image loaded" (falls back to
  drawing nothing while Custom Image remains selected) rather than
  crashing; a warning is logged.

## Testing

- Extend the existing `tests/test_config.c` round-trip test: outline fields
  for all three shapes round-trip correctly, `SHAPE_CUSTOM_PNG` round-trips
  as the `shape` value, and a small fixture PNG's base64 payload round-trips
  byte-for-byte through save/load.
- New manual verification checklist lines in `README.md`: toggling outline
  on/off with a color/thickness change is visible on screen; importing a
  valid ≤16×16 PNG switches the crosshair to it and hides
  Color/Opacity/Outline; importing an oversized PNG shows the error and
  leaves the current crosshair unchanged; exporting and re-importing a
  preset that includes a custom image reproduces the exact same image.
