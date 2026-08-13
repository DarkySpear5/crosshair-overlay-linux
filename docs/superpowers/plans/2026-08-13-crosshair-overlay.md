# Crosshair Overlay for Linux Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a lightweight, click-through, always-on-top crosshair overlay for Linux, configured through a GTK options window (opened from a tray icon), with JSON import/export of presets and a rebindable global hotkey (2+ keys) to toggle visibility.

**Architecture:** Six C modules (`config`, `overlay_window`, `tray`, `options_window`, `hotkey`, `main`) built with a plain Makefile. `config` owns the `CrosshairConfig` struct and JSON load/save/import/export. `overlay_window` renders the crosshair with Cairo in a click-through, override-redirect GTK window. `tray` shows the status icon and menu. `options_window` is the settings UI, applying changes live to `overlay_window` and autosaving via `config`. `hotkey` grabs a global X11 key combo via `XGrabKey` to toggle visibility regardless of window focus.

**Tech Stack:** C, GTK3, Cairo, X11/Xext (Shape extension, XGrabKey), json-glib, Makefile/pkg-config.

## Global Constraints

- All dependencies must be free/open-source and available in standard Linux distro repositories: `gtk+-3.0`, `x11`, `xext`, `json-glib-1.0` (verify via `pkg-config --exists`).
- Config auto-persists to `~/.config/crosshair-overlay/config.json`; import/export uses the same JSON schema at an arbitrary user-chosen path.
- Config schema field names, defaults, and types are exactly as specified in `docs/superpowers/specs/2026-08-13-crosshair-overlay-design.md` (defaults: green `#00FF00`, opacity `1.0`, `size_percent: 100`, `offset_x`/`offset_y`: `0`, `shape: "cross"`, `hotkey: ["Ctrl", "Alt", "X"]`).
- Hotkey combos must support a minimum of 2 keys (at least one modifier + one trigger key).
- The overlay must never intercept mouse input (click-through) and must stay above other windows (always-on-top), implemented via GTK's override-redirect popup window type plus the X11 Shape extension's empty input region.
- License: MIT (see `LICENSE`, created in Task 1).
- **Build/test environment constraint:** this plan is being implemented on a Windows machine with no C compiler, GTK3, or X11 dev headers installed, and no WSL/Docker available. **No task in this plan can be compiled or run by the implementing agent.** Every "verify" step is phrased as a command for the *user* to run later on their actual Linux machine, not something the agent executes. The agent's job on each task is: write correct code, self-review it by re-reading it carefully for syntax/logic errors (mismatched braces, wrong types, unincluded headers, mismatched function signatures against the Interfaces block), then commit. Do not attempt to invoke `gcc`, `make`, `pkg-config`, or the built binary — they do not exist in this environment.

---

## File Structure

```
Crosshair for Linux/
├── LICENSE
├── README.md
├── .gitignore
├── Makefile
├── src/
│   ├── main.c              # entry point, wires all modules together
│   ├── config.h/.c         # CrosshairConfig struct, JSON load/save/import/export
│   ├── overlay_window.h/.c # click-through always-on-top Cairo-drawn overlay
│   ├── tray.h/.c           # status icon + menu
│   ├── options_window.h/.c # settings GUI window
│   └── hotkey.h/.c         # global X11 hotkey grab + rebind capture
└── tests/
    └── test_config.c       # standalone round-trip test for config.c (no framework)
```

---

### Task 1: Project scaffolding and toolchain sanity check

**Files:**
- Create: `LICENSE`
- Create: `README.md`
- Create: `.gitignore`
- Create: `Makefile`
- Create: `src/main.c`

**Interfaces:**
- Produces: a `bin/crosshair-overlay` binary target (via `make`) that later tasks extend by adding source files to the `SRC` variable in `Makefile`, and functions in `src/main.c` that later tasks will modify (notably `int main(int argc, char **argv)`).

- [ ] **Step 1: Write `LICENSE`**

Standard MIT license text, copyright line using the current year and a placeholder holder name the user can adjust:

```
MIT License

Copyright (c) 2026 Crosshair Overlay Contributors

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

- [ ] **Step 2: Write `.gitignore`**

```
bin/
*.o
*.d
```

- [ ] **Step 3: Write `README.md`**

```markdown
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
```

- [ ] **Step 4: Write `Makefile`**

```makefile
CC = gcc
PKGS = gtk+-3.0 x11 xext json-glib-1.0
CFLAGS = -Wall -Wextra -g $(shell pkg-config --cflags $(PKGS))
LDFLAGS = $(shell pkg-config --libs $(PKGS))

SRC = src/main.c
OBJ = $(SRC:.c=.o)
BIN = bin/crosshair-overlay

TEST_SRC = tests/test_config.c src/config.c
TEST_BIN = bin/test_config

.PHONY: all clean test

all: $(BIN)

$(BIN): $(OBJ)
	@mkdir -p bin
	$(CC) $(OBJ) -o $(BIN) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

test:
	@mkdir -p bin
	$(CC) $(CFLAGS) $(TEST_SRC) -o $(TEST_BIN) $(LDFLAGS)
	./$(TEST_BIN)

clean:
	rm -f $(OBJ) $(TEST_BIN)
	rm -rf bin
```

- [ ] **Step 5: Write `src/main.c` (toolchain sanity check)**

A minimal GTK app that just proves the build toolchain and GTK3 linkage work, before any real logic is added:

```c
#include <gtk/gtk.h>

int main(int argc, char **argv) {
    gtk_init(&argc, &argv);

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Crosshair Overlay - toolchain OK");
    gtk_window_set_default_size(GTK_WINDOW(window), 300, 100);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget *label = gtk_label_new("Build succeeded. This window will be replaced by the tray + overlay in later tasks.");
    gtk_container_add(GTK_CONTAINER(window), label);

    gtk_widget_show_all(window);
    gtk_main();
    return 0;
}
```

- [ ] **Step 6: Manual verification (user runs on Linux)**

```bash
make
./bin/crosshair-overlay
```
Expected: a window titled "Crosshair Overlay - toolchain OK" appears with the label text; closing it exits cleanly.

- [ ] **Step 7: Commit**

```bash
git add LICENSE README.md .gitignore Makefile src/main.c
git commit -m "Scaffold project: build system, license, docs, toolchain sanity check"
```

---

### Task 2: Config module + round-trip test

**Files:**
- Create: `src/config.h`
- Create: `src/config.c`
- Create: `tests/test_config.c`
- Modify: `Makefile` (add `src/config.c` to `SRC`)

**Interfaces:**
- Consumes: nothing from other modules (this is the foundational data module).
- Produces (used by every later task):
  - `typedef enum { SHAPE_CROSS, SHAPE_DOT, SHAPE_CIRCLE } CrosshairShape;`
  - `typedef struct CrosshairConfig { gboolean enabled; int monitor; int offset_x; int offset_y; int size_percent; CrosshairShape shape; CrossSettings cross; DotSettings dot; CircleSettings circle; char *hotkey_keys[HOTKEY_MAX_KEYS]; int hotkey_count; } CrosshairConfig;` (each shape settings struct has `double r, g, b, opacity` plus its own geometry fields — see below)
  - `void config_set_defaults(CrosshairConfig *cfg);`
  - `void config_free_contents(CrosshairConfig *cfg);` (frees the `hotkey_keys` strings)
  - `char *config_default_path(void);` (caller frees with `g_free`)
  - `gboolean config_load(CrosshairConfig *cfg, const char *path, GError **error);` (on missing/corrupt file, fills `cfg` with defaults and returns `TRUE`; returns `FALSE` only for a genuinely unexpected I/O condition captured in `error`)
  - `gboolean config_save(const CrosshairConfig *cfg, const char *path, GError **error);` (creates parent directories as needed)
  - `void config_set_hotkey(CrosshairConfig *cfg, char * const *keys, int count);` (deep-copies `keys`, frees any previous strings, clamps `count` to `HOTKEY_MAX_KEYS`)
  - `const char *shape_to_string(CrosshairShape shape);` / `gboolean shape_from_string(const char *str, CrosshairShape *out);`
  - `#define HOTKEY_MAX_KEYS 4`

- [ ] **Step 1: Write `tests/test_config.c` (the test, written first)**

```c
#include <glib.h>
#include <string.h>
#include <stdio.h>
#include "../src/config.h"

static int failures = 0;

#define CHECK(cond, msg) do { \
    if (!(cond)) { fprintf(stderr, "FAIL: %s\n", msg); failures++; } \
    else { fprintf(stderr, "PASS: %s\n", msg); } \
} while (0)

static void test_defaults(void) {
    CrosshairConfig cfg;
    config_set_defaults(&cfg);
    CHECK(cfg.enabled == TRUE, "defaults: enabled");
    CHECK(cfg.monitor == 0, "defaults: monitor");
    CHECK(cfg.offset_x == 0 && cfg.offset_y == 0, "defaults: offset centered");
    CHECK(cfg.size_percent == 100, "defaults: size_percent");
    CHECK(cfg.shape == SHAPE_CROSS, "defaults: shape is cross");
    CHECK(cfg.cross.r == 0.0 && cfg.cross.g == 1.0 && cfg.cross.b == 0.0, "defaults: cross color is green");
    CHECK(cfg.cross.opacity == 1.0, "defaults: cross opacity");
    CHECK(cfg.hotkey_count == 2, "defaults: hotkey has 2 keys (Ctrl+Alt... wait see below)");
    config_free_contents(&cfg);
}

static void test_round_trip(void) {
    CrosshairConfig cfg;
    config_set_defaults(&cfg);
    cfg.offset_x = 42;
    cfg.offset_y = -17;
    cfg.size_percent = 150;
    cfg.shape = SHAPE_CIRCLE;
    cfg.circle.radius = 12.5;
    cfg.circle.thickness = 3.0;
    cfg.circle.r = 1.0; cfg.circle.g = 0.0; cfg.circle.b = 0.0;
    cfg.circle.opacity = 0.75;
    char *keys[3] = { "Ctrl", "Shift", "F1" };
    config_set_hotkey(&cfg, keys, 3);

    const char *path = "/tmp/crosshair_overlay_test_config.json";
    GError *error = NULL;
    gboolean saved = config_save(&cfg, path, &error);
    CHECK(saved && error == NULL, "save succeeds");
    if (error) { fprintf(stderr, "  save error: %s\n", error->message); g_clear_error(&error); }

    CrosshairConfig loaded;
    gboolean ok = config_load(&loaded, path, &error);
    CHECK(ok && error == NULL, "load succeeds");
    if (error) { fprintf(stderr, "  load error: %s\n", error->message); g_clear_error(&error); }

    CHECK(loaded.offset_x == 42, "round-trip: offset_x");
    CHECK(loaded.offset_y == -17, "round-trip: offset_y");
    CHECK(loaded.size_percent == 150, "round-trip: size_percent");
    CHECK(loaded.shape == SHAPE_CIRCLE, "round-trip: shape");
    CHECK(loaded.circle.radius > 12.4 && loaded.circle.radius < 12.6, "round-trip: circle radius");
    CHECK(loaded.circle.r == 1.0 && loaded.circle.g == 0.0 && loaded.circle.b == 0.0, "round-trip: circle color");
    CHECK(loaded.circle.opacity > 0.74 && loaded.circle.opacity < 0.76, "round-trip: circle opacity");
    CHECK(loaded.hotkey_count == 3, "round-trip: hotkey count");
    CHECK(loaded.hotkey_count == 3 && strcmp(loaded.hotkey_keys[0], "Ctrl") == 0, "round-trip: hotkey[0]");
    CHECK(loaded.hotkey_count == 3 && strcmp(loaded.hotkey_keys[2], "F1") == 0, "round-trip: hotkey[2]");

    config_free_contents(&cfg);
    config_free_contents(&loaded);
    remove(path);
}

static void test_missing_file_falls_back_to_defaults(void) {
    CrosshairConfig cfg;
    GError *error = NULL;
    gboolean ok = config_load(&cfg, "/tmp/crosshair_overlay_does_not_exist.json", &error);
    CHECK(ok == TRUE, "missing file: load still returns TRUE");
    CHECK(cfg.shape == SHAPE_CROSS, "missing file: falls back to default shape");
    g_clear_error(&error);
    config_free_contents(&cfg);
}

static void test_corrupt_file_falls_back_to_defaults(void) {
    const char *path = "/tmp/crosshair_overlay_corrupt.json";
    FILE *f = fopen(path, "w");
    fprintf(f, "{ this is not valid json ");
    fclose(f);

    CrosshairConfig cfg;
    GError *error = NULL;
    gboolean ok = config_load(&cfg, path, &error);
    CHECK(ok == TRUE, "corrupt file: load still returns TRUE");
    CHECK(cfg.size_percent == 100, "corrupt file: falls back to default size_percent");
    g_clear_error(&error);
    config_free_contents(&cfg);
    remove(path);
}

int main(void) {
    test_defaults();
    test_round_trip();
    test_missing_file_falls_back_to_defaults();
    test_corrupt_file_falls_back_to_defaults();

    if (failures > 0) {
        fprintf(stderr, "\n%d check(s) FAILED\n", failures);
        return 1;
    }
    fprintf(stderr, "\nAll checks passed\n");
    return 0;
}
```

- [ ] **Step 2: Fix the hotkey-count default assertion**

The `test_defaults` check above assumes the default hotkey is `["Ctrl", "Alt", "X"]` (3 keys), not 2 — correct the check before moving on, so the test file's assertion reads:

```c
    CHECK(cfg.hotkey_count == 3, "defaults: hotkey has 3 keys (Ctrl+Alt+X)");
```

(This replaces the `cfg.hotkey_count == 2` line from Step 1 — that line was a mistake caught during self-review; use `3` to match the spec's default of `Ctrl+Alt+X`.)

- [ ] **Step 3: Write `src/config.h`**

```c
#ifndef CONFIG_H
#define CONFIG_H

#include <glib.h>

#define HOTKEY_MAX_KEYS 4

typedef enum {
    SHAPE_CROSS = 0,
    SHAPE_DOT,
    SHAPE_CIRCLE
} CrosshairShape;

typedef struct {
    double length;
    double thickness;
    double gap;
    double r, g, b;
    double opacity;
} CrossSettings;

typedef struct {
    double radius;
    double r, g, b;
    double opacity;
} DotSettings;

typedef struct {
    double radius;
    double thickness;
    double r, g, b;
    double opacity;
} CircleSettings;

typedef struct {
    gboolean enabled;
    int monitor;
    int offset_x;
    int offset_y;
    int size_percent;
    CrosshairShape shape;
    CrossSettings cross;
    DotSettings dot;
    CircleSettings circle;
    char *hotkey_keys[HOTKEY_MAX_KEYS];
    int hotkey_count;
} CrosshairConfig;

void config_set_defaults(CrosshairConfig *cfg);
void config_free_contents(CrosshairConfig *cfg);
char *config_default_path(void);
gboolean config_load(CrosshairConfig *cfg, const char *path, GError **error);
gboolean config_save(const CrosshairConfig *cfg, const char *path, GError **error);
void config_set_hotkey(CrosshairConfig *cfg, char * const *keys, int count);
const char *shape_to_string(CrosshairShape shape);
gboolean shape_from_string(const char *str, CrosshairShape *out);

#endif
```

- [ ] **Step 4: Write `src/config.c`**

```c
#include "config.h"
#include <json-glib/json-glib.h>
#include <glib/gstdio.h>
#include <string.h>
#include <stdio.h>

void config_set_defaults(CrosshairConfig *cfg) {
    memset(cfg, 0, sizeof(*cfg));
    cfg->enabled = TRUE;
    cfg->monitor = 0;
    cfg->offset_x = 0;
    cfg->offset_y = 0;
    cfg->size_percent = 100;
    cfg->shape = SHAPE_CROSS;

    cfg->cross.length = 10;
    cfg->cross.thickness = 2;
    cfg->cross.gap = 4;
    cfg->cross.r = 0.0; cfg->cross.g = 1.0; cfg->cross.b = 0.0;
    cfg->cross.opacity = 1.0;

    cfg->dot.radius = 2;
    cfg->dot.r = 0.0; cfg->dot.g = 1.0; cfg->dot.b = 0.0;
    cfg->dot.opacity = 1.0;

    cfg->circle.radius = 8;
    cfg->circle.thickness = 2;
    cfg->circle.r = 0.0; cfg->circle.g = 1.0; cfg->circle.b = 0.0;
    cfg->circle.opacity = 1.0;

    char *keys[3] = { "Ctrl", "Alt", "X" };
    config_set_hotkey(cfg, keys, 3);
}

void config_free_contents(CrosshairConfig *cfg) {
    for (int i = 0; i < cfg->hotkey_count; i++) {
        g_free(cfg->hotkey_keys[i]);
        cfg->hotkey_keys[i] = NULL;
    }
    cfg->hotkey_count = 0;
}

char *config_default_path(void) {
    return g_build_filename(g_get_user_config_dir(), "crosshair-overlay", "config.json", NULL);
}

void config_set_hotkey(CrosshairConfig *cfg, char * const *keys, int count) {
    for (int i = 0; i < cfg->hotkey_count; i++) {
        g_free(cfg->hotkey_keys[i]);
        cfg->hotkey_keys[i] = NULL;
    }
    if (count > HOTKEY_MAX_KEYS) count = HOTKEY_MAX_KEYS;
    for (int i = 0; i < count; i++) {
        cfg->hotkey_keys[i] = g_strdup(keys[i]);
    }
    cfg->hotkey_count = count;
}

const char *shape_to_string(CrosshairShape shape) {
    switch (shape) {
        case SHAPE_CROSS:  return "cross";
        case SHAPE_DOT:    return "dot";
        case SHAPE_CIRCLE: return "circle";
        default:           return "cross";
    }
}

gboolean shape_from_string(const char *str, CrosshairShape *out) {
    if (g_strcmp0(str, "cross") == 0)  { *out = SHAPE_CROSS;  return TRUE; }
    if (g_strcmp0(str, "dot") == 0)    { *out = SHAPE_DOT;    return TRUE; }
    if (g_strcmp0(str, "circle") == 0) { *out = SHAPE_CIRCLE; return TRUE; }
    return FALSE;
}

static char *color_to_hex(double r, double g, double b) {
    return g_strdup_printf("#%02X%02X%02X",
        (int)(CLAMP(r, 0.0, 1.0) * 255.0 + 0.5),
        (int)(CLAMP(g, 0.0, 1.0) * 255.0 + 0.5),
        (int)(CLAMP(b, 0.0, 1.0) * 255.0 + 0.5));
}

static void hex_to_color(const char *hex, double *r, double *g, double *b) {
    if (!hex || hex[0] != '#' || strlen(hex) < 7) { *r = 0.0; *g = 1.0; *b = 0.0; return; }
    unsigned int ri = 0, gi = 0, bi = 0;
    sscanf(hex + 1, "%02x%02x%02x", &ri, &gi, &bi);
    *r = ri / 255.0; *g = gi / 255.0; *b = bi / 255.0;
}

static void build_shape_object(JsonBuilder *b, const char *name, gboolean has_length_thickness_gap,
                                gboolean has_thickness_only, double length, double thickness, double gap,
                                double radius, double r, double g, double bl, double opacity) {
    json_builder_set_member_name(b, name);
    json_builder_begin_object(b);
    if (has_length_thickness_gap) {
        json_builder_set_member_name(b, "length"); json_builder_add_double_value(b, length);
        json_builder_set_member_name(b, "thickness"); json_builder_add_double_value(b, thickness);
        json_builder_set_member_name(b, "gap"); json_builder_add_double_value(b, gap);
    } else if (has_thickness_only) {
        json_builder_set_member_name(b, "radius"); json_builder_add_double_value(b, radius);
        json_builder_set_member_name(b, "thickness"); json_builder_add_double_value(b, thickness);
    } else {
        json_builder_set_member_name(b, "radius"); json_builder_add_double_value(b, radius);
    }
    char *hex = color_to_hex(r, g, bl);
    json_builder_set_member_name(b, "color"); json_builder_add_string_value(b, hex);
    g_free(hex);
    json_builder_set_member_name(b, "opacity"); json_builder_add_double_value(b, opacity);
    json_builder_end_object(b);
}

gboolean config_save(const CrosshairConfig *cfg, const char *path, GError **error) {
    char *dir = g_path_get_dirname(path);
    if (g_mkdir_with_parents(dir, 0755) != 0) {
        g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "Could not create directory: %s", dir);
        g_free(dir);
        return FALSE;
    }
    g_free(dir);

    JsonBuilder *b = json_builder_new();
    json_builder_begin_object(b);

    json_builder_set_member_name(b, "enabled"); json_builder_add_boolean_value(b, cfg->enabled);
    json_builder_set_member_name(b, "monitor"); json_builder_add_int_value(b, cfg->monitor);
    json_builder_set_member_name(b, "offset_x"); json_builder_add_int_value(b, cfg->offset_x);
    json_builder_set_member_name(b, "offset_y"); json_builder_add_int_value(b, cfg->offset_y);
    json_builder_set_member_name(b, "size_percent"); json_builder_add_int_value(b, cfg->size_percent);
    json_builder_set_member_name(b, "shape"); json_builder_add_string_value(b, shape_to_string(cfg->shape));

    build_shape_object(b, "cross", TRUE, FALSE, cfg->cross.length, cfg->cross.thickness, cfg->cross.gap,
                        0, cfg->cross.r, cfg->cross.g, cfg->cross.b, cfg->cross.opacity);
    build_shape_object(b, "dot", FALSE, FALSE, 0, 0, 0,
                        cfg->dot.radius, cfg->dot.r, cfg->dot.g, cfg->dot.b, cfg->dot.opacity);
    build_shape_object(b, "circle", FALSE, TRUE, 0, cfg->circle.thickness, 0,
                        cfg->circle.radius, cfg->circle.r, cfg->circle.g, cfg->circle.b, cfg->circle.opacity);

    json_builder_set_member_name(b, "hotkey");
    json_builder_begin_array(b);
    for (int i = 0; i < cfg->hotkey_count; i++) {
        json_builder_add_string_value(b, cfg->hotkey_keys[i]);
    }
    json_builder_end_array(b);

    json_builder_end_object(b);

    JsonGenerator *gen = json_generator_new();
    JsonNode *root = json_builder_get_root(b);
    json_generator_set_root(gen, root);
    json_generator_set_pretty(gen, TRUE);
    gboolean ok = json_generator_to_file(gen, path, error);

    json_node_free(root);
    g_object_unref(gen);
    g_object_unref(b);
    return ok;
}

static double get_double_or(JsonObject *obj, const char *key, double fallback) {
    return json_object_has_member(obj, key) ? json_object_get_double_member(obj, key) : fallback;
}

static int get_int_or(JsonObject *obj, const char *key, int fallback) {
    return json_object_has_member(obj, key) ? (int)json_object_get_int_member(obj, key) : fallback;
}

gboolean config_load(CrosshairConfig *cfg, const char *path, GError **error) {
    config_set_defaults(cfg);

    if (!g_file_test(path, G_FILE_TEST_EXISTS)) {
        return TRUE;
    }

    JsonParser *parser = json_parser_new();
    GError *local_error = NULL;
    if (!json_parser_load_from_file(parser, path, &local_error)) {
        g_clear_error(&local_error);
        g_object_unref(parser);
        return TRUE;
    }

    JsonNode *root = json_parser_get_root(parser);
    if (!root || json_node_get_node_type(root) != JSON_NODE_OBJECT) {
        g_object_unref(parser);
        return TRUE;
    }
    JsonObject *obj = json_node_get_object(root);

    if (json_object_has_member(obj, "enabled"))
        cfg->enabled = json_object_get_boolean_member(obj, "enabled");
    cfg->monitor = get_int_or(obj, "monitor", cfg->monitor);
    cfg->offset_x = get_int_or(obj, "offset_x", cfg->offset_x);
    cfg->offset_y = get_int_or(obj, "offset_y", cfg->offset_y);
    cfg->size_percent = get_int_or(obj, "size_percent", cfg->size_percent);

    if (json_object_has_member(obj, "shape")) {
        CrosshairShape s;
        if (shape_from_string(json_object_get_string_member(obj, "shape"), &s)) {
            cfg->shape = s;
        }
    }

    if (json_object_has_member(obj, "cross")) {
        JsonObject *s = json_object_get_object_member(obj, "cross");
        cfg->cross.length = get_double_or(s, "length", cfg->cross.length);
        cfg->cross.thickness = get_double_or(s, "thickness", cfg->cross.thickness);
        cfg->cross.gap = get_double_or(s, "gap", cfg->cross.gap);
        cfg->cross.opacity = get_double_or(s, "opacity", cfg->cross.opacity);
        if (json_object_has_member(s, "color"))
            hex_to_color(json_object_get_string_member(s, "color"), &cfg->cross.r, &cfg->cross.g, &cfg->cross.b);
    }
    if (json_object_has_member(obj, "dot")) {
        JsonObject *s = json_object_get_object_member(obj, "dot");
        cfg->dot.radius = get_double_or(s, "radius", cfg->dot.radius);
        cfg->dot.opacity = get_double_or(s, "opacity", cfg->dot.opacity);
        if (json_object_has_member(s, "color"))
            hex_to_color(json_object_get_string_member(s, "color"), &cfg->dot.r, &cfg->dot.g, &cfg->dot.b);
    }
    if (json_object_has_member(obj, "circle")) {
        JsonObject *s = json_object_get_object_member(obj, "circle");
        cfg->circle.radius = get_double_or(s, "radius", cfg->circle.radius);
        cfg->circle.thickness = get_double_or(s, "thickness", cfg->circle.thickness);
        cfg->circle.opacity = get_double_or(s, "opacity", cfg->circle.opacity);
        if (json_object_has_member(s, "color"))
            hex_to_color(json_object_get_string_member(s, "color"), &cfg->circle.r, &cfg->circle.g, &cfg->circle.b);
    }

    if (json_object_has_member(obj, "hotkey")) {
        JsonArray *arr = json_object_get_array_member(obj, "hotkey");
        guint len = json_array_get_length(arr);
        if (len >= 2) {
            char *keys[HOTKEY_MAX_KEYS];
            guint n = len > HOTKEY_MAX_KEYS ? HOTKEY_MAX_KEYS : len;
            for (guint i = 0; i < n; i++) {
                keys[i] = (char *)json_array_get_string_element(arr, i);
            }
            config_set_hotkey(cfg, keys, (int)n);
        }
    }

    g_object_unref(parser);
    return TRUE;
}
```

- [ ] **Step 5: Modify `Makefile`**

Add `src/config.c` to the `SRC` variable:

```makefile
SRC = src/main.c src/config.c
```

- [ ] **Step 6: Self-review**

Re-read `src/config.h` and `src/config.c` side by side: confirm every function declared in the header has a matching definition with the exact same signature in the `.c` file, confirm all `json-glib` and `glib` calls used (`json_builder_*`, `json_parser_*`, `json_object_*`, `json_array_*`, `g_build_filename`, `g_mkdir_with_parents`, `g_file_test`, `g_strdup_printf`, `CLAMP`) are real GLib/json-glib API names, and confirm `#include` lines cover everything used (`<json-glib/json-glib.h>`, `<glib/gstdio.h>`, `<string.h>`, `<stdio.h>`).

- [ ] **Step 7: Manual verification (user runs on Linux)**

```bash
make test
```
Expected output: every `CHECK` line printed with `PASS:`, ending in `All checks passed`, exit code 0. If any `FAIL:` lines appear, report them back before continuing to the next task.

- [ ] **Step 8: Commit**

```bash
git add src/config.h src/config.c tests/test_config.c Makefile
git commit -m "Add config module with JSON load/save and round-trip test"
```

---

### Task 3: Overlay window (click-through, always-on-top, Cairo-drawn)

**Files:**
- Create: `src/overlay_window.h`
- Create: `src/overlay_window.c`
- Modify: `src/main.c` (load config, create and show the overlay instead of the sanity-check window)
- Modify: `Makefile` (add `src/overlay_window.c` to `SRC`)

**Interfaces:**
- Consumes: `CrosshairConfig` and all functions from Task 2's `config.h`.
- Produces (used by Tasks 4, 5, 8):
  - `typedef struct _OverlayWindow OverlayWindow;`
  - `OverlayWindow *overlay_window_new(void);`
  - `void overlay_window_apply_config(OverlayWindow *ow, const CrosshairConfig *cfg);` (repositions, resizes, and redraws based on `cfg`; also shows/hides based on `cfg->enabled`)
  - `void overlay_window_set_visible(OverlayWindow *ow, gboolean visible);`
  - `GtkWidget *overlay_window_get_widget(OverlayWindow *ow);`

- [ ] **Step 1: Write `src/overlay_window.h`**

```c
#ifndef OVERLAY_WINDOW_H
#define OVERLAY_WINDOW_H

#include <gtk/gtk.h>
#include "config.h"

typedef struct _OverlayWindow OverlayWindow;

OverlayWindow *overlay_window_new(void);
void overlay_window_apply_config(OverlayWindow *ow, const CrosshairConfig *cfg);
void overlay_window_set_visible(OverlayWindow *ow, gboolean visible);
GtkWidget *overlay_window_get_widget(OverlayWindow *ow);

#endif
```

- [ ] **Step 2: Write `src/overlay_window.c`**

```c
#include "overlay_window.h"
#include <gdk/gdkx.h>
#include <X11/extensions/shape.h>
#include <math.h>

struct _OverlayWindow {
    GtkWidget *window;
    CrosshairConfig cfg;
    gboolean shaped_once;
};

static int shape_bounding_size(const CrosshairConfig *cfg) {
    double scale = cfg->size_percent / 100.0;
    double half;
    switch (cfg->shape) {
        case SHAPE_CROSS:
            half = (cfg->cross.length + cfg->cross.gap) * scale;
            break;
        case SHAPE_DOT:
            half = cfg->dot.radius * scale;
            break;
        case SHAPE_CIRCLE:
            half = (cfg->circle.radius + cfg->circle.thickness) * scale;
            break;
        default:
            half = 10;
    }
    int size = (int)ceil(half * 2.0) + 4;
    return size < 8 ? 8 : size;
}

static gboolean on_draw(GtkWidget *widget, cairo_t *cr, gpointer user_data) {
    OverlayWindow *ow = (OverlayWindow *)user_data;
    guint width = gtk_widget_get_allocated_width(widget);
    guint height = gtk_widget_get_allocated_height(widget);
    double cx = width / 2.0;
    double cy = height / 2.0;
    double scale = ow->cfg.size_percent / 100.0;

    cairo_save(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    cairo_restore(cr);

    switch (ow->cfg.shape) {
        case SHAPE_CROSS: {
            double len = ow->cfg.cross.length * scale;
            double gap = ow->cfg.cross.gap * scale;
            double thick = ow->cfg.cross.thickness * scale;
            cairo_set_source_rgba(cr, ow->cfg.cross.r, ow->cfg.cross.g, ow->cfg.cross.b, ow->cfg.cross.opacity);
            cairo_set_line_width(cr, thick);
            cairo_set_line_cap(cr, CAIRO_LINE_CAP_BUTT);

            cairo_move_to(cr, cx - gap - len, cy);
            cairo_line_to(cr, cx - gap, cy);
            cairo_move_to(cr, cx + gap, cy);
            cairo_line_to(cr, cx + gap + len, cy);
            cairo_move_to(cr, cx, cy - gap - len);
            cairo_line_to(cr, cx, cy - gap);
            cairo_move_to(cr, cx, cy + gap);
            cairo_line_to(cr, cx, cy + gap + len);
            cairo_stroke(cr);
            break;
        }
        case SHAPE_DOT: {
            double r = ow->cfg.dot.radius * scale;
            cairo_set_source_rgba(cr, ow->cfg.dot.r, ow->cfg.dot.g, ow->cfg.dot.b, ow->cfg.dot.opacity);
            cairo_arc(cr, cx, cy, r, 0, 2 * G_PI);
            cairo_fill(cr);
            break;
        }
        case SHAPE_CIRCLE: {
            double r = ow->cfg.circle.radius * scale;
            double thick = ow->cfg.circle.thickness * scale;
            cairo_set_source_rgba(cr, ow->cfg.circle.r, ow->cfg.circle.g, ow->cfg.circle.b, ow->cfg.circle.opacity);
            cairo_set_line_width(cr, thick);
            cairo_arc(cr, cx, cy, r, 0, 2 * G_PI);
            cairo_stroke(cr);
            break;
        }
    }
    return FALSE;
}

/* Empty X11 input region so all mouse events pass through to the window below. */
static void make_click_through(GtkWidget *widget) {
    GdkWindow *gdk_win = gtk_widget_get_window(widget);
    if (!gdk_win) return;
    Display *xdisplay = GDK_WINDOW_XDISPLAY(gdk_win);
    Window xid = GDK_WINDOW_XID(gdk_win);
    Region empty = XCreateRegion();
    XShapeCombineRegion(xdisplay, xid, ShapeInput, 0, 0, empty, ShapeSet);
    XDestroyRegion(empty);
}

static void reposition(OverlayWindow *ow) {
    int size = shape_bounding_size(&ow->cfg);
    gtk_window_resize(GTK_WINDOW(ow->window), size, size);

    GdkDisplay *display = gdk_display_get_default();
    GdkMonitor *monitor = NULL;
    int n = gdk_display_get_n_monitors(display);
    if (ow->cfg.monitor >= 0 && ow->cfg.monitor < n) {
        monitor = gdk_display_get_monitor(display, ow->cfg.monitor);
    }
    if (!monitor) {
        monitor = gdk_display_get_primary_monitor(display);
    }
    if (!monitor) {
        monitor = gdk_display_get_monitor(display, 0);
    }

    GdkRectangle geo;
    gdk_monitor_get_geometry(monitor, &geo);
    int center_x = geo.x + geo.width / 2 + ow->cfg.offset_x;
    int center_y = geo.y + geo.height / 2 + ow->cfg.offset_y;

    gtk_window_move(GTK_WINDOW(ow->window), center_x - size / 2, center_y - size / 2);
}

static void on_realize(GtkWidget *widget, gpointer user_data) {
    OverlayWindow *ow = (OverlayWindow *)user_data;
    make_click_through(widget);
    ow->shaped_once = TRUE;
}

OverlayWindow *overlay_window_new(void) {
    OverlayWindow *ow = g_new0(OverlayWindow, 1);

    ow->window = gtk_window_new(GTK_WINDOW_POPUP);
    gtk_window_set_decorated(GTK_WINDOW(ow->window), FALSE);
    gtk_window_set_skip_taskbar_hint(GTK_WINDOW(ow->window), TRUE);
    gtk_window_set_skip_pager_hint(GTK_WINDOW(ow->window), TRUE);
    gtk_window_set_keep_above(GTK_WINDOW(ow->window), TRUE);
    gtk_widget_set_app_paintable(ow->window, TRUE);

    GdkScreen *screen = gdk_screen_get_default();
    GdkVisual *visual = gdk_screen_get_rgba_visual(screen);
    if (visual && gdk_screen_is_composited(screen)) {
        gtk_widget_set_visual(ow->window, visual);
    }

    g_signal_connect(ow->window, "draw", G_CALLBACK(on_draw), ow);
    g_signal_connect(ow->window, "realize", G_CALLBACK(on_realize), ow);

    config_set_defaults(&ow->cfg);
    return ow;
}

void overlay_window_apply_config(OverlayWindow *ow, const CrosshairConfig *cfg) {
    ow->cfg = *cfg;
    if (!gtk_widget_get_realized(ow->window)) {
        gtk_widget_realize(ow->window);
    }
    reposition(ow);
    if (cfg->enabled) {
        gtk_widget_show(ow->window);
    } else {
        gtk_widget_hide(ow->window);
    }
    if (ow->shaped_once) {
        make_click_through(ow->window);
    }
    gtk_widget_queue_draw(ow->window);
}

void overlay_window_set_visible(OverlayWindow *ow, gboolean visible) {
    if (visible) {
        gtk_widget_show(ow->window);
    } else {
        gtk_widget_hide(ow->window);
    }
}

GtkWidget *overlay_window_get_widget(OverlayWindow *ow) {
    return ow->window;
}
```

**Note:** `ow->cfg` is a shallow copy of `CrosshairConfig`, including the `hotkey_keys` pointers. The overlay window never calls `config_free_contents` on its copy, so it never double-frees those strings — ownership stays with whichever `CrosshairConfig` the caller (`main.c`, later `options_window.c`) manages. This is documented here because it's a non-obvious sharp edge for whoever touches this struct next.

- [ ] **Step 3: Modify `src/main.c`**

Replace the sanity-check window with config loading and the real overlay:

```c
#include <gtk/gtk.h>
#include "config.h"
#include "overlay_window.h"

int main(int argc, char **argv) {
    gtk_init(&argc, &argv);

    CrosshairConfig cfg;
    char *path = config_default_path();
    GError *error = NULL;
    config_load(&cfg, path, &error);
    if (error) {
        g_warning("Config load warning: %s", error->message);
        g_clear_error(&error);
    }

    OverlayWindow *overlay = overlay_window_new();
    overlay_window_apply_config(overlay, &cfg);

    g_free(path);
    gtk_main();

    config_free_contents(&cfg);
    return 0;
}
```

- [ ] **Step 4: Modify `Makefile`**

```makefile
SRC = src/main.c src/config.c src/overlay_window.c
```

- [ ] **Step 5: Self-review**

Confirm `overlay_window.c` includes `<gdk/gdkx.h>` and `<X11/extensions/shape.h>` for the `GDK_WINDOW_XDISPLAY`/`GDK_WINDOW_XID`/`XShapeCombineRegion`/`Region`/`XCreateRegion`/`XDestroyRegion` symbols, and that every function declared in `overlay_window.h` has a matching definition. Confirm `main.c`'s `#include` list matches what it actually calls.

- [ ] **Step 6: Manual verification (user runs on Linux)**

```bash
make
./bin/crosshair-overlay
```
Expected: a small green cross appears centered on the primary monitor, with no window border, staying on top of other windows; clicking where the cross is drawn passes the click through to whatever is underneath; closing other means of stopping it is `Ctrl+C` in the terminal for now (Quit via tray comes in Task 4).

- [ ] **Step 7: Commit**

```bash
git add src/overlay_window.h src/overlay_window.c src/main.c Makefile
git commit -m "Add click-through always-on-top overlay window with Cairo-drawn crosshair"
```

---

### Task 4: Tray icon and menu

**Files:**
- Create: `src/tray.h`
- Create: `src/tray.c`
- Modify: `src/main.c` (create tray, wire Enable/Disable + Quit; Options… uses a placeholder callback for now)
- Modify: `Makefile` (add `src/tray.c` to `SRC`)

**Interfaces:**
- Consumes: nothing beyond GTK.
- Produces (used by Tasks 5, 8):
  - `typedef struct _TrayIcon TrayIcon;`
  - `typedef void (*TrayCallback)(gpointer user_data);`
  - `TrayIcon *tray_icon_new(gboolean initial_enabled, TrayCallback on_toggle, TrayCallback on_options, TrayCallback on_quit, gpointer user_data);`
  - `void tray_icon_set_enabled(TrayIcon *tray, gboolean enabled);` (updates the menu item label/checked-state to reflect current state, e.g. after the hotkey toggles it)

- [ ] **Step 1: Write `src/tray.h`**

```c
#ifndef TRAY_H
#define TRAY_H

#include <gtk/gtk.h>

typedef struct _TrayIcon TrayIcon;
typedef void (*TrayCallback)(gpointer user_data);

TrayIcon *tray_icon_new(gboolean initial_enabled, TrayCallback on_toggle, TrayCallback on_options,
                         TrayCallback on_quit, gpointer user_data);
void tray_icon_set_enabled(TrayIcon *tray, gboolean enabled);

#endif
```

- [ ] **Step 2: Write `src/tray.c`**

```c
#include "tray.h"

struct _TrayIcon {
    GtkStatusIcon *status_icon;
    GtkWidget *menu;
    GtkWidget *toggle_item;
    TrayCallback on_toggle;
    TrayCallback on_options;
    TrayCallback on_quit;
    gpointer user_data;
    gboolean enabled;
};

static void update_toggle_label(TrayIcon *tray) {
    gtk_menu_item_set_label(GTK_MENU_ITEM(tray->toggle_item), tray->enabled ? "Disable" : "Enable");
}

static void on_toggle_activate(GtkMenuItem *item, gpointer user_data) {
    (void)item;
    TrayIcon *tray = (TrayIcon *)user_data;
    if (tray->on_toggle) tray->on_toggle(tray->user_data);
}

static void on_options_activate(GtkMenuItem *item, gpointer user_data) {
    (void)item;
    TrayIcon *tray = (TrayIcon *)user_data;
    if (tray->on_options) tray->on_options(tray->user_data);
}

static void on_quit_activate(GtkMenuItem *item, gpointer user_data) {
    (void)item;
    TrayIcon *tray = (TrayIcon *)user_data;
    if (tray->on_quit) tray->on_quit(tray->user_data);
}

static void on_status_icon_activate(GtkStatusIcon *status_icon, gpointer user_data) {
    (void)status_icon;
    on_toggle_activate(NULL, user_data);
}

static void on_status_icon_popup(GtkStatusIcon *status_icon, guint button, guint activate_time, gpointer user_data) {
    TrayIcon *tray = (TrayIcon *)user_data;
    gtk_menu_popup(GTK_MENU(tray->menu), NULL, NULL, gtk_status_icon_position_menu, status_icon, button, activate_time);
}

TrayIcon *tray_icon_new(gboolean initial_enabled, TrayCallback on_toggle, TrayCallback on_options,
                         TrayCallback on_quit, gpointer user_data) {
    TrayIcon *tray = g_new0(TrayIcon, 1);
    tray->on_toggle = on_toggle;
    tray->on_options = on_options;
    tray->on_quit = on_quit;
    tray->user_data = user_data;
    tray->enabled = initial_enabled;

    tray->status_icon = gtk_status_icon_new_from_icon_name("view-restore");
    gtk_status_icon_set_tooltip_text(tray->status_icon, "Crosshair Overlay");
    gtk_status_icon_set_visible(tray->status_icon, TRUE);

    tray->menu = gtk_menu_new();

    tray->toggle_item = gtk_menu_item_new_with_label(initial_enabled ? "Disable" : "Enable");
    g_signal_connect(tray->toggle_item, "activate", G_CALLBACK(on_toggle_activate), tray);
    gtk_menu_shell_append(GTK_MENU_SHELL(tray->menu), tray->toggle_item);

    GtkWidget *options_item = gtk_menu_item_new_with_label("Options…");
    g_signal_connect(options_item, "activate", G_CALLBACK(on_options_activate), tray);
    gtk_menu_shell_append(GTK_MENU_SHELL(tray->menu), options_item);

    GtkWidget *separator = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(tray->menu), separator);

    GtkWidget *quit_item = gtk_menu_item_new_with_label("Quit");
    g_signal_connect(quit_item, "activate", G_CALLBACK(on_quit_activate), tray);
    gtk_menu_shell_append(GTK_MENU_SHELL(tray->menu), quit_item);

    gtk_widget_show_all(tray->menu);

    g_signal_connect(tray->status_icon, "activate", G_CALLBACK(on_status_icon_activate), tray);
    g_signal_connect(tray->status_icon, "popup-menu", G_CALLBACK(on_status_icon_popup), tray);

    return tray;
}

void tray_icon_set_enabled(TrayIcon *tray, gboolean enabled) {
    tray->enabled = enabled;
    update_toggle_label(tray);
}
```

**Note on `GtkStatusIcon`:** it's deprecated since GTK 3.14 but still functional and is the simplest pure-GTK3 way to get a tray icon without adding an `libappindicator3` dependency (keeping the dependency list to exactly what's in the spec). On GNOME Shell, tray icons require the "AppIndicator and KStatusNotifierItem Support" extension to be visible at all — this is a GNOME-specific quirk, not a bug in this app; call it out in the README (done in Task 8).

- [ ] **Step 3: Modify `src/main.c`**

```c
#include <gtk/gtk.h>
#include "config.h"
#include "overlay_window.h"
#include "tray.h"

typedef struct {
    CrosshairConfig cfg;
    OverlayWindow *overlay;
    TrayIcon *tray;
    char *config_path;
} AppState;

static void save_current_config(AppState *app) {
    GError *error = NULL;
    if (!config_save(&app->cfg, app->config_path, &error)) {
        g_warning("Config save failed: %s", error->message);
        g_clear_error(&error);
    }
}

static void on_toggle(gpointer user_data) {
    AppState *app = (AppState *)user_data;
    app->cfg.enabled = !app->cfg.enabled;
    overlay_window_apply_config(app->overlay, &app->cfg);
    tray_icon_set_enabled(app->tray, app->cfg.enabled);
    save_current_config(app);
}

static void on_options(gpointer user_data) {
    (void)user_data;
    g_message("Options… clicked (options window not implemented yet)");
}

static void on_quit(gpointer user_data) {
    (void)user_data;
    gtk_main_quit();
}

int main(int argc, char **argv) {
    gtk_init(&argc, &argv);

    AppState app = { 0 };
    app.config_path = config_default_path();

    GError *error = NULL;
    config_load(&app.cfg, app.config_path, &error);
    if (error) {
        g_warning("Config load warning: %s", error->message);
        g_clear_error(&error);
    }

    app.overlay = overlay_window_new();
    overlay_window_apply_config(app.overlay, &app.cfg);

    app.tray = tray_icon_new(app.cfg.enabled, on_toggle, on_options, on_quit, &app);

    gtk_main();

    config_free_contents(&app.cfg);
    g_free(app.config_path);
    return 0;
}
```

- [ ] **Step 4: Modify `Makefile`**

```makefile
SRC = src/main.c src/config.c src/overlay_window.c src/tray.c
```

- [ ] **Step 5: Self-review**

Confirm `AppState` field names used in `on_toggle`/`on_options`/`on_quit` (`cfg`, `overlay`, `tray`, `config_path`) match the struct definition, and that `tray_icon_new`'s parameter order in the call matches its declaration in `tray.h`.

- [ ] **Step 6: Manual verification (user runs on Linux)**

```bash
make
./bin/crosshair-overlay
```
Expected: a tray icon appears; left-clicking it toggles the crosshair on/off; right-clicking (or the platform's menu gesture for `GtkStatusIcon`) shows Enable/Disable/Options…/Quit; Quit exits the app; Options… logs a message to the terminal (not yet a window).

- [ ] **Step 7: Commit**

```bash
git add src/tray.h src/tray.c src/main.c Makefile
git commit -m "Add tray icon with enable/disable toggle and quit"
```

---

### Task 5: Options window — position, size, monitor, shape, color, opacity

**Files:**
- Create: `src/options_window.h`
- Create: `src/options_window.c`
- Modify: `src/main.c` (replace the `on_options` placeholder with one that presents the real options window)
- Modify: `Makefile` (add `src/options_window.c` to `SRC`)

**Interfaces:**
- Consumes: `CrosshairConfig`/`config_save` from Task 2, `OverlayWindow`/`overlay_window_apply_config` from Task 3.
- Produces (used by Tasks 6, 7, 8):
  - `typedef struct _OptionsWindow OptionsWindow;`
  - `OptionsWindow *options_window_new(CrosshairConfig *cfg, OverlayWindow *overlay, const char *config_path);` (the window does **not** own `cfg`; it mutates the caller's struct in place and calls `overlay_window_apply_config` + `config_save` after every change)
  - `void options_window_present(OptionsWindow *ow);`
  - `GtkWidget *options_window_get_widget(OptionsWindow *ow);` (needed by Task 6 to attach key-press handlers for hotkey capture)

- [ ] **Step 1: Write `src/options_window.h`**

```c
#ifndef OPTIONS_WINDOW_H
#define OPTIONS_WINDOW_H

#include <gtk/gtk.h>
#include "config.h"
#include "overlay_window.h"

typedef struct _OptionsWindow OptionsWindow;

OptionsWindow *options_window_new(CrosshairConfig *cfg, OverlayWindow *overlay, const char *config_path);
void options_window_present(OptionsWindow *ow);
GtkWidget *options_window_get_widget(OptionsWindow *ow);

#endif
```

- [ ] **Step 2: Write `src/options_window.c`**

```c
#include "options_window.h"

struct _OptionsWindow {
    GtkWidget *window;
    CrosshairConfig *cfg;
    OverlayWindow *overlay;
    char *config_path;

    GtkWidget *x_spin;
    GtkWidget *y_spin;
    GtkWidget *size_spin;
    GtkWidget *monitor_combo;
    GtkWidget *shape_combo;
    GtkWidget *color_button;
    GtkWidget *opacity_scale;

    gboolean updating_ui; /* guards against feedback loops while syncing widgets to cfg */
};

static void apply_and_save(OptionsWindow *ow) {
    overlay_window_apply_config(ow->overlay, ow->cfg);
    GError *error = NULL;
    if (!config_save(ow->cfg, ow->config_path, &error)) {
        g_warning("Config save failed: %s", error->message);
        g_clear_error(&error);
    }
}

static void get_active_shape_color(OptionsWindow *ow, double *r, double *g, double *b, double *opacity) {
    switch (ow->cfg->shape) {
        case SHAPE_CROSS:  *r = ow->cfg->cross.r;  *g = ow->cfg->cross.g;  *b = ow->cfg->cross.b;  *opacity = ow->cfg->cross.opacity;  break;
        case SHAPE_DOT:    *r = ow->cfg->dot.r;    *g = ow->cfg->dot.g;    *b = ow->cfg->dot.b;    *opacity = ow->cfg->dot.opacity;    break;
        case SHAPE_CIRCLE: *r = ow->cfg->circle.r; *g = ow->cfg->circle.g; *b = ow->cfg->circle.b; *opacity = ow->cfg->circle.opacity; break;
    }
}

static void set_active_shape_color(OptionsWindow *ow, double r, double g, double b) {
    switch (ow->cfg->shape) {
        case SHAPE_CROSS:  ow->cfg->cross.r = r;  ow->cfg->cross.g = g;  ow->cfg->cross.b = b;  break;
        case SHAPE_DOT:    ow->cfg->dot.r = r;    ow->cfg->dot.g = g;    ow->cfg->dot.b = b;    break;
        case SHAPE_CIRCLE: ow->cfg->circle.r = r; ow->cfg->circle.g = g; ow->cfg->circle.b = b; break;
    }
}

static void set_active_shape_opacity(OptionsWindow *ow, double opacity) {
    switch (ow->cfg->shape) {
        case SHAPE_CROSS:  ow->cfg->cross.opacity = opacity;  break;
        case SHAPE_DOT:    ow->cfg->dot.opacity = opacity;    break;
        case SHAPE_CIRCLE: ow->cfg->circle.opacity = opacity; break;
    }
}

static void refresh_color_widgets(OptionsWindow *ow) {
    double r, g, b, opacity;
    get_active_shape_color(ow, &r, &g, &b, &opacity);
    GdkRGBA rgba = { r, g, b, 1.0 };
    ow->updating_ui = TRUE;
    gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(ow->color_button), &rgba);
    gtk_range_set_value(GTK_RANGE(ow->opacity_scale), opacity * 100.0);
    ow->updating_ui = FALSE;
}

static void on_x_changed(GtkSpinButton *spin, gpointer user_data) {
    OptionsWindow *ow = (OptionsWindow *)user_data;
    if (ow->updating_ui) return;
    ow->cfg->offset_x = gtk_spin_button_get_value_as_int(spin);
    apply_and_save(ow);
}

static void on_y_changed(GtkSpinButton *spin, gpointer user_data) {
    OptionsWindow *ow = (OptionsWindow *)user_data;
    if (ow->updating_ui) return;
    ow->cfg->offset_y = gtk_spin_button_get_value_as_int(spin);
    apply_and_save(ow);
}

static void on_size_changed(GtkSpinButton *spin, gpointer user_data) {
    OptionsWindow *ow = (OptionsWindow *)user_data;
    if (ow->updating_ui) return;
    ow->cfg->size_percent = gtk_spin_button_get_value_as_int(spin);
    apply_and_save(ow);
}

static void on_monitor_changed(GtkComboBox *combo, gpointer user_data) {
    OptionsWindow *ow = (OptionsWindow *)user_data;
    if (ow->updating_ui) return;
    ow->cfg->monitor = gtk_combo_box_get_active(combo);
    apply_and_save(ow);
}

static void on_shape_changed(GtkComboBox *combo, gpointer user_data) {
    OptionsWindow *ow = (OptionsWindow *)user_data;
    if (ow->updating_ui) return;
    gint active = gtk_combo_box_get_active(combo);
    ow->cfg->shape = (CrosshairShape)active;
    refresh_color_widgets(ow);
    apply_and_save(ow);
}

static void on_color_changed(GtkColorButton *button, gpointer user_data) {
    OptionsWindow *ow = (OptionsWindow *)user_data;
    if (ow->updating_ui) return;
    GdkRGBA rgba;
    gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(button), &rgba);
    set_active_shape_color(ow, rgba.red, rgba.green, rgba.blue);
    apply_and_save(ow);
}

static void on_opacity_changed(GtkRange *range, gpointer user_data) {
    OptionsWindow *ow = (OptionsWindow *)user_data;
    if (ow->updating_ui) return;
    set_active_shape_opacity(ow, gtk_range_get_value(range) / 100.0);
    apply_and_save(ow);
}

static void populate_monitors(OptionsWindow *ow) {
    GdkDisplay *display = gdk_display_get_default();
    int n = gdk_display_get_n_monitors(display);
    for (int i = 0; i < n; i++) {
        GdkMonitor *mon = gdk_display_get_monitor(display, i);
        const char *model = gdk_monitor_get_model(mon);
        char *label = g_strdup_printf("Monitor %d%s%s", i, model ? " - " : "", model ? model : "");
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(ow->monitor_combo), label);
        g_free(label);
    }
}

OptionsWindow *options_window_new(CrosshairConfig *cfg, OverlayWindow *overlay, const char *config_path) {
    OptionsWindow *ow = g_new0(OptionsWindow, 1);
    ow->cfg = cfg;
    ow->overlay = overlay;
    ow->config_path = g_strdup(config_path);
    ow->updating_ui = FALSE;

    ow->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(ow->window), "Crosshair Overlay - Options");
    gtk_window_set_default_size(GTK_WINDOW(ow->window), 360, -1);
    g_signal_connect(ow->window, "delete-event", G_CALLBACK(gtk_widget_hide_on_delete), NULL);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 12);
    gtk_container_add(GTK_CONTAINER(ow->window), grid);

    int row = 0;

    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("X offset"), 0, row, 1, 1);
    ow->x_spin = gtk_spin_button_new_with_range(-4096, 4096, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(ow->x_spin), cfg->offset_x);
    g_signal_connect(ow->x_spin, "value-changed", G_CALLBACK(on_x_changed), ow);
    gtk_grid_attach(GTK_GRID(grid), ow->x_spin, 1, row, 1, 1);
    row++;

    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Y offset"), 0, row, 1, 1);
    ow->y_spin = gtk_spin_button_new_with_range(-4096, 4096, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(ow->y_spin), cfg->offset_y);
    g_signal_connect(ow->y_spin, "value-changed", G_CALLBACK(on_y_changed), ow);
    gtk_grid_attach(GTK_GRID(grid), ow->y_spin, 1, row, 1, 1);
    row++;

    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Size %"), 0, row, 1, 1);
    ow->size_spin = gtk_spin_button_new_with_range(10, 500, 5);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(ow->size_spin), cfg->size_percent);
    g_signal_connect(ow->size_spin, "value-changed", G_CALLBACK(on_size_changed), ow);
    gtk_grid_attach(GTK_GRID(grid), ow->size_spin, 1, row, 1, 1);
    row++;

    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Monitor"), 0, row, 1, 1);
    ow->monitor_combo = gtk_combo_box_text_new();
    populate_monitors(ow);
    gtk_combo_box_set_active(GTK_COMBO_BOX(ow->monitor_combo), cfg->monitor);
    g_signal_connect(ow->monitor_combo, "changed", G_CALLBACK(on_monitor_changed), ow);
    gtk_grid_attach(GTK_GRID(grid), ow->monitor_combo, 1, row, 1, 1);
    row++;

    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Shape"), 0, row, 1, 1);
    ow->shape_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(ow->shape_combo), "Cross");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(ow->shape_combo), "Dot");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(ow->shape_combo), "Circle");
    gtk_combo_box_set_active(GTK_COMBO_BOX(ow->shape_combo), (int)cfg->shape);
    g_signal_connect(ow->shape_combo, "changed", G_CALLBACK(on_shape_changed), ow);
    gtk_grid_attach(GTK_GRID(grid), ow->shape_combo, 1, row, 1, 1);
    row++;

    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Color"), 0, row, 1, 1);
    ow->color_button = gtk_color_button_new();
    g_signal_connect(ow->color_button, "color-set", G_CALLBACK(on_color_changed), ow);
    gtk_grid_attach(GTK_GRID(grid), ow->color_button, 1, row, 1, 1);
    row++;

    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Opacity %"), 0, row, 1, 1);
    ow->opacity_scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 100, 1);
    g_signal_connect(ow->opacity_scale, "value-changed", G_CALLBACK(on_opacity_changed), ow);
    gtk_grid_attach(GTK_GRID(grid), ow->opacity_scale, 1, row, 1, 1);
    row++;

    refresh_color_widgets(ow);

    return ow;
}

void options_window_present(OptionsWindow *ow) {
    gtk_widget_show_all(ow->window);
    gtk_window_present(GTK_WINDOW(ow->window));
}

GtkWidget *options_window_get_widget(OptionsWindow *ow) {
    return ow->window;
}
```

- [ ] **Step 3: Modify `src/main.c`**

Add the options window and replace the placeholder callback:

```c
#include <gtk/gtk.h>
#include "config.h"
#include "overlay_window.h"
#include "tray.h"
#include "options_window.h"

typedef struct {
    CrosshairConfig cfg;
    OverlayWindow *overlay;
    TrayIcon *tray;
    OptionsWindow *options;
    char *config_path;
} AppState;

static void save_current_config(AppState *app) {
    GError *error = NULL;
    if (!config_save(&app->cfg, app->config_path, &error)) {
        g_warning("Config save failed: %s", error->message);
        g_clear_error(&error);
    }
}

static void on_toggle(gpointer user_data) {
    AppState *app = (AppState *)user_data;
    app->cfg.enabled = !app->cfg.enabled;
    overlay_window_apply_config(app->overlay, &app->cfg);
    tray_icon_set_enabled(app->tray, app->cfg.enabled);
    save_current_config(app);
}

static void on_options(gpointer user_data) {
    AppState *app = (AppState *)user_data;
    options_window_present(app->options);
}

static void on_quit(gpointer user_data) {
    (void)user_data;
    gtk_main_quit();
}

int main(int argc, char **argv) {
    gtk_init(&argc, &argv);

    AppState app = { 0 };
    app.config_path = config_default_path();

    GError *error = NULL;
    config_load(&app.cfg, app.config_path, &error);
    if (error) {
        g_warning("Config load warning: %s", error->message);
        g_clear_error(&error);
    }

    app.overlay = overlay_window_new();
    overlay_window_apply_config(app.overlay, &app.cfg);

    app.options = options_window_new(&app.cfg, app.overlay, app.config_path);

    app.tray = tray_icon_new(app.cfg.enabled, on_toggle, on_options, on_quit, &app);

    gtk_main();

    config_free_contents(&app.cfg);
    g_free(app.config_path);
    return 0;
}
```

- [ ] **Step 4: Modify `Makefile`**

```makefile
SRC = src/main.c src/config.c src/overlay_window.c src/tray.c src/options_window.c
```

- [ ] **Step 5: Self-review**

Confirm every widget field referenced in a callback (`ow->x_spin`, `ow->monitor_combo`, etc.) is actually assigned in `options_window_new` before any signal could fire, and that `GdkRGBA` field names (`red`, `green`, `blue`) match what `on_color_changed` reads.

- [ ] **Step 6: Manual verification (user runs on Linux)**

```bash
make
./bin/crosshair-overlay
```
Expected: tray → Options… opens a window with X/Y/Size/Monitor/Shape/Color/Opacity controls; changing any of them updates the on-screen crosshair immediately; closing the options window (the X button) hides it without quitting the app (re-openable from the tray).

- [ ] **Step 7: Commit**

```bash
git add src/options_window.h src/options_window.c src/main.c Makefile
git commit -m "Add options window with live position, size, monitor, shape, color, and opacity controls"
```

---

### Task 6: Global hotkey (grab + rebind capture)

**Files:**
- Create: `src/hotkey.h`
- Create: `src/hotkey.c`
- Modify: `src/options_window.h` / `src/options_window.c` (add the Hotkey row + Rebind button + capture mode)
- Modify: `src/main.c` (initialize the hotkey module and grab the configured combo, bound to the same toggle logic as the tray)
- Modify: `Makefile` (add `src/hotkey.c` to `SRC`)

**Interfaces:**
- Consumes: `CrosshairConfig.hotkey_keys`/`hotkey_count` from Task 2.
- Produces (used by Task 8):
  - `typedef void (*HotkeyToggleCallback)(gpointer user_data);`
  - `gboolean hotkey_init(void);`
  - `void hotkey_shutdown(void);`
  - `gboolean hotkey_grab(char * const *keys, int count, HotkeyToggleCallback cb, gpointer user_data, GError **error);` (ungrabs any previous combo first)
  - `void hotkey_ungrab(void);`

- [ ] **Step 1: Write `src/hotkey.h`**

```c
#ifndef HOTKEY_H
#define HOTKEY_H

#include <glib.h>

typedef void (*HotkeyToggleCallback)(gpointer user_data);

gboolean hotkey_init(void);
void hotkey_shutdown(void);
gboolean hotkey_grab(char * const *keys, int count, HotkeyToggleCallback cb, gpointer user_data, GError **error);
void hotkey_ungrab(void);

#endif
```

- [ ] **Step 2: Write `src/hotkey.c`**

```c
#include "hotkey.h"
#include <gdk/gdkx.h>
#include <gdk/gdk.h>
#include <X11/Xlib.h>
#include <string.h>

static Display *xdisplay = NULL;
static Window root = 0;
static KeyCode grabbed_keycode = 0;
static unsigned int grabbed_modmask = 0;
static gboolean have_grab = FALSE;
static HotkeyToggleCallback active_callback = NULL;
static gpointer active_user_data = NULL;

/* X11 reports events with the currently-active lock modifiers (NumLock, CapsLock)
 * mixed into the state, so a single XGrabKey for our intended mask would silently
 * stop matching the moment NumLock or CapsLock is toggled. We grab all 4
 * combinations of {no lock, NumLock, CapsLock, both} to make the hotkey reliable
 * regardless of lock-key state, and ignore those bits when comparing incoming events. */
#define LOCK_IGNORE_MASKS_COUNT 4

static unsigned int name_to_modmask(const char *name) {
    if (g_ascii_strcasecmp(name, "Ctrl") == 0 || g_ascii_strcasecmp(name, "Control") == 0) return ControlMask;
    if (g_ascii_strcasecmp(name, "Alt") == 0) return Mod1Mask;
    if (g_ascii_strcasecmp(name, "Shift") == 0) return ShiftMask;
    if (g_ascii_strcasecmp(name, "Super") == 0 || g_ascii_strcasecmp(name, "Meta") == 0) return Mod4Mask;
    return 0;
}

static gboolean is_modifier_name(const char *name) {
    return name_to_modmask(name) != 0;
}

static GdkFilterReturn event_filter(GdkXEvent *xevent, GdkEvent *event, gpointer user_data) {
    (void)event;
    (void)user_data;
    XEvent *xev = (XEvent *)xevent;
    if (xev->type != KeyPress) return GDK_FILTER_CONTINUE;
    if (!have_grab) return GDK_FILTER_CONTINUE;

    unsigned int ignore = LockMask | Mod2Mask;
    unsigned int state = xev->xkey.state & ~ignore;

    if (xev->xkey.keycode == grabbed_keycode && state == grabbed_modmask) {
        if (active_callback) active_callback(active_user_data);
    }
    return GDK_FILTER_CONTINUE;
}

gboolean hotkey_init(void) {
    GdkDisplay *gdk_display = gdk_display_get_default();
    if (!gdk_display || !GDK_IS_X11_DISPLAY(gdk_display)) {
        return FALSE;
    }
    xdisplay = GDK_DISPLAY_XDISPLAY(gdk_display);
    root = DefaultRootWindow(xdisplay);

    GdkWindow *root_gdk = gdk_get_default_root_window();
    gdk_window_add_filter(root_gdk, event_filter, NULL);
    gdk_window_set_events(root_gdk, gdk_window_get_events(root_gdk) | GDK_KEY_PRESS_MASK);

    return TRUE;
}

void hotkey_ungrab(void) {
    if (have_grab && xdisplay) {
        unsigned int ignore_masks[LOCK_IGNORE_MASKS_COUNT] = { 0, LockMask, Mod2Mask, LockMask | Mod2Mask };
        for (int i = 0; i < LOCK_IGNORE_MASKS_COUNT; i++) {
            XUngrabKey(xdisplay, grabbed_keycode, grabbed_modmask | ignore_masks[i], root);
        }
        have_grab = FALSE;
    }
}

gboolean hotkey_grab(char * const *keys, int count, HotkeyToggleCallback cb, gpointer user_data, GError **error) {
    if (!xdisplay) {
        g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "hotkey_init() was not called or no X11 display available");
        return FALSE;
    }
    if (count < 2) {
        g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "Hotkey needs at least 2 keys");
        return FALSE;
    }

    unsigned int modmask = 0;
    const char *trigger_name = NULL;
    for (int i = 0; i < count; i++) {
        if (is_modifier_name(keys[i])) {
            modmask |= name_to_modmask(keys[i]);
        } else if (trigger_name == NULL) {
            trigger_name = keys[i];
        } else {
            g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "Only one non-modifier trigger key is allowed");
            return FALSE;
        }
    }
    if (!trigger_name) {
        g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "Hotkey needs exactly one non-modifier trigger key");
        return FALSE;
    }

    KeySym keysym = XStringToKeysym(trigger_name);
    if (keysym == NoSymbol) {
        g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "Unrecognized key name: %s", trigger_name);
        return FALSE;
    }
    KeyCode keycode = XKeysymToKeycode(xdisplay, keysym);
    if (keycode == 0) {
        g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "No keycode for key: %s", trigger_name);
        return FALSE;
    }

    hotkey_ungrab();

    gdk_x11_display_error_trap_push(gdk_display_get_default());

    unsigned int ignore_masks[LOCK_IGNORE_MASKS_COUNT] = { 0, LockMask, Mod2Mask, LockMask | Mod2Mask };
    for (int i = 0; i < LOCK_IGNORE_MASKS_COUNT; i++) {
        XGrabKey(xdisplay, keycode, modmask | ignore_masks[i], root, True, GrabModeAsync, GrabModeAsync);
    }
    XSync(xdisplay, False);

    gint xerror = gdk_x11_display_error_trap_pop(gdk_display_get_default());
    if (xerror != 0) {
        g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "Failed to grab hotkey (X error %d) - it may already be in use by another application", xerror);
        return FALSE;
    }

    grabbed_keycode = keycode;
    grabbed_modmask = modmask;
    have_grab = TRUE;
    active_callback = cb;
    active_user_data = user_data;

    return TRUE;
}

void hotkey_shutdown(void) {
    hotkey_ungrab();
    active_callback = NULL;
    active_user_data = NULL;
}
```

- [ ] **Step 3: Modify `src/options_window.h`**

Add a callback type the caller uses to be notified when the hotkey changes (so `main.c` can re-grab it) and a struct field placeholder is not needed since `options_window.c` will call `hotkey_grab` directly — but the header needs no new declarations beyond what already exists, since rebinding is entirely internal to the options window. No changes needed to `options_window.h`.

- [ ] **Step 4: Modify `src/options_window.c`**

Add includes, new struct fields, capture-mode state, and the Hotkey UI row. Add near the top:

```c
#include "hotkey.h"
```

Add to the `struct _OptionsWindow` body (after `opacity_scale`):

```c
    GtkWidget *hotkey_label;
    GtkWidget *hotkey_rebind_button;
    gboolean capturing_hotkey;
    GPtrArray *captured_keys; /* owns strdup'd key name strings during capture */
```

Add these functions above `options_window_new` (after `on_opacity_changed`):

```c
static char *hotkey_display_string(CrosshairConfig *cfg) {
    GString *s = g_string_new("");
    for (int i = 0; i < cfg->hotkey_count; i++) {
        if (i > 0) g_string_append(s, "+");
        g_string_append(s, cfg->hotkey_keys[i]);
    }
    return g_string_free(s, FALSE);
}

static void refresh_hotkey_label(OptionsWindow *ow) {
    char *s = hotkey_display_string(ow->cfg);
    gtk_label_set_text(GTK_LABEL(ow->hotkey_label), s);
    g_free(s);
}

static const char *gdk_keyval_to_hotkey_name(guint keyval) {
    switch (keyval) {
        case GDK_KEY_Control_L: case GDK_KEY_Control_R: return "Ctrl";
        case GDK_KEY_Alt_L: case GDK_KEY_Alt_R: return "Alt";
        case GDK_KEY_Shift_L: case GDK_KEY_Shift_R: return "Shift";
        case GDK_KEY_Super_L: case GDK_KEY_Super_R: return "Super";
        default: return NULL;
    }
}

static void array_add_unique(GPtrArray *arr, const char *name) {
    for (guint i = 0; i < arr->len; i++) {
        if (g_strcmp0((const char *)g_ptr_array_index(arr, i), name) == 0) return;
    }
    g_ptr_array_add(arr, g_strdup(name));
}

static gboolean on_capture_key_press(GtkWidget *widget, GdkEventKey *event, gpointer user_data) {
    OptionsWindow *ow = (OptionsWindow *)user_data;
    if (!ow->capturing_hotkey) return FALSE;
    (void)widget;

    const char *mod_name = gdk_keyval_to_hotkey_name(event->keyval);
    if (mod_name) {
        array_add_unique(ow->captured_keys, mod_name);
    } else {
        char *keyname = gdk_keyval_name(gdk_keyval_to_upper(event->keyval));
        if (keyname) array_add_unique(ow->captured_keys, keyname);
    }

    GString *preview = g_string_new("");
    for (guint i = 0; i < ow->captured_keys->len; i++) {
        if (i > 0) g_string_append(preview, "+");
        g_string_append(preview, (const char *)g_ptr_array_index(ow->captured_keys, i));
    }
    gtk_label_set_text(GTK_LABEL(ow->hotkey_label), preview->str);
    g_string_free(preview, TRUE);
    return TRUE;
}

static gboolean on_capture_key_release(GtkWidget *widget, GdkEventKey *event, gpointer user_data) {
    (void)event;
    OptionsWindow *ow = (OptionsWindow *)user_data;
    if (!ow->capturing_hotkey) return FALSE;

    GdkModifierType mask;
    gdk_window_get_device_position(gtk_widget_get_window(widget),
        gdk_seat_get_pointer(gdk_display_get_default_seat(gdk_display_get_default())),
        NULL, NULL, &mask);
    (void)mask;

    GdkDeviceManager *manager = gdk_display_get_device_manager(gdk_display_get_default());
    GdkDevice *keyboard = gdk_device_manager_get_client_pointer(manager);
    (void)keyboard;

    if (ow->captured_keys->len < 2) {
        return TRUE;
    }

    char *keys[HOTKEY_MAX_KEYS];
    guint n = ow->captured_keys->len > HOTKEY_MAX_KEYS ? HOTKEY_MAX_KEYS : ow->captured_keys->len;
    for (guint i = 0; i < n; i++) {
        keys[i] = (char *)g_ptr_array_index(ow->captured_keys, i);
    }

    GError *error = NULL;
    if (hotkey_grab(keys, (int)n, (HotkeyToggleCallback)NULL, NULL, &error)) {
        /* Grab test succeeded structurally; real (re)grab with the actual callback
           happens in main.c's on_hotkey_changed, triggered below. */
    }
    if (error) {
        g_clear_error(&error);
    }

    config_set_hotkey(ow->cfg, keys, (int)n);

    ow->capturing_hotkey = FALSE;
    gtk_button_set_label(GTK_BUTTON(ow->hotkey_rebind_button), "Rebind");
    refresh_hotkey_label(ow);
    apply_and_save(ow);

    for (guint i = 0; i < ow->captured_keys->len; i++) {
        g_free(g_ptr_array_index(ow->captured_keys, i));
    }
    g_ptr_array_set_size(ow->captured_keys, 0);

    return TRUE;
}

static void on_rebind_clicked(GtkButton *button, gpointer user_data) {
    OptionsWindow *ow = (OptionsWindow *)user_data;
    (void)button;
    ow->capturing_hotkey = TRUE;
    for (guint i = 0; i < ow->captured_keys->len; i++) {
        g_free(g_ptr_array_index(ow->captured_keys, i));
    }
    g_ptr_array_set_size(ow->captured_keys, 0);
    gtk_button_set_label(GTK_BUTTON(ow->hotkey_rebind_button), "Press 2+ keys…");
    gtk_label_set_text(GTK_LABEL(ow->hotkey_label), "");
    gtk_widget_grab_focus(ow->window);
}
```

**Important simplification note (read before implementing Step 5's `main.c` change):** the actual global `XGrabKey` re-grab bound to the real toggle callback must happen in `main.c`, since only `main.c` holds the `AppState` needed for the toggle logic. To make this work without `options_window.c` needing to know about `AppState`, add one more field to `OptionsWindow` and one setter function, declared in `options_window.h`:

```c
typedef void (*HotkeyChangedCallback)(gpointer user_data);
void options_window_set_hotkey_changed_callback(OptionsWindow *ow, HotkeyChangedCallback cb, gpointer user_data);
```

Add corresponding fields to `struct _OptionsWindow` (`HotkeyChangedCallback hotkey_changed_cb; gpointer hotkey_changed_user_data;`), implement the setter trivially, and call `if (ow->hotkey_changed_cb) ow->hotkey_changed_cb(ow->hotkey_changed_user_data);` at the end of `on_capture_key_release`, right after `apply_and_save(ow)`. Remove the speculative `hotkey_grab(...)` call in `on_capture_key_release` shown above (it was a structural test only) — the real grab happens exclusively in `main.c`'s callback, to avoid grabbing twice with two different callbacks.

Now add the Hotkey row to `options_window_new`, right after the Opacity row (`row++` at the end of that block) and before `refresh_color_widgets(ow);`:

```c
    ow->captured_keys = g_ptr_array_new();
    ow->capturing_hotkey = FALSE;

    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Hotkey"), 0, row, 1, 1);
    GtkWidget *hotkey_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    ow->hotkey_label = gtk_label_new("");
    ow->hotkey_rebind_button = gtk_button_new_with_label("Rebind");
    g_signal_connect(ow->hotkey_rebind_button, "clicked", G_CALLBACK(on_rebind_clicked), ow);
    gtk_box_pack_start(GTK_BOX(hotkey_box), ow->hotkey_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hotkey_box), ow->hotkey_rebind_button, FALSE, FALSE, 0);
    gtk_grid_attach(GTK_GRID(grid), hotkey_box, 1, row, 1, 1);
    row++;

    g_signal_connect(ow->window, "key-press-event", G_CALLBACK(on_capture_key_press), ow);
    g_signal_connect(ow->window, "key-release-event", G_CALLBACK(on_capture_key_release), ow);

    refresh_hotkey_label(ow);
```

And add the setter implementation plus fields at the end of the file / in the struct as described above:

```c
void options_window_set_hotkey_changed_callback(OptionsWindow *ow, HotkeyChangedCallback cb, gpointer user_data) {
    ow->hotkey_changed_cb = cb;
    ow->hotkey_changed_user_data = user_data;
}
```

- [ ] **Step 5: Modify `src/options_window.h`**

Add the callback typedef and setter declaration:

```c
typedef void (*HotkeyChangedCallback)(gpointer user_data);
void options_window_set_hotkey_changed_callback(OptionsWindow *ow, HotkeyChangedCallback cb, gpointer user_data);
```

(placed after the existing `options_window_get_widget` declaration)

- [ ] **Step 6: Modify `src/main.c`**

Add `#include "hotkey.h"`, initialize the hotkey system after creating the options window, grab the configured combo, and wire the rebind-changed callback:

```c
static void toggle_from_hotkey(gpointer user_data) {
    on_toggle(user_data);
}

static void regrab_hotkey(AppState *app) {
    GError *error = NULL;
    if (!hotkey_grab(app->cfg.hotkey_keys, app->cfg.hotkey_count, toggle_from_hotkey, app, &error)) {
        g_warning("Hotkey grab failed: %s", error ? error->message : "unknown error");
        g_clear_error(&error);
    }
}

static void on_hotkey_changed(gpointer user_data) {
    AppState *app = (AppState *)user_data;
    regrab_hotkey(app);
}
```

Insert these three functions after `on_quit` and before `main`. In `main`, after `app.options = options_window_new(...)`, add:

```c
    options_window_set_hotkey_changed_callback(app.options, on_hotkey_changed, &app);

    if (hotkey_init()) {
        regrab_hotkey(&app);
    } else {
        g_warning("Global hotkey unavailable (not running under X11)");
    }
```

And before `return 0;` at the end of `main`, add:

```c
    hotkey_shutdown();
```

- [ ] **Step 7: Modify `Makefile`**

```makefile
SRC = src/main.c src/config.c src/overlay_window.c src/tray.c src/options_window.c src/hotkey.c
```

- [ ] **Step 8: Self-review**

Re-read `hotkey.c` and confirm `<X11/Xlib.h>` provides `Display`, `Window`, `KeyCode`, `KeySym`, `XGrabKey`, `XUngrabKey`, `XStringToKeysym`, `XKeysymToKeycode`, `ControlMask`/`Mod1Mask`/`ShiftMask`/`Mod4Mask`/`LockMask`/`Mod2Mask`, `NoSymbol`, `GrabModeAsync`, `True`/`False`, `KeyPress`, `XEvent`, `XSync`. Confirm `options_window.c`'s new functions reference only fields that now exist on `struct _OptionsWindow` (`hotkey_label`, `hotkey_rebind_button`, `capturing_hotkey`, `captured_keys`, `hotkey_changed_cb`, `hotkey_changed_user_data`) and that the speculative `hotkey_grab` test call described mid-step was actually removed per the "Important simplification note" — leaving exactly one real grab site, in `main.c`. Confirm `main.c`'s `toggle_from_hotkey`/`regrab_hotkey`/`on_hotkey_changed` are declared before use (C requires this — place them after `on_toggle` since they call it).

- [ ] **Step 9: Manual verification (user runs on Linux)**

```bash
make
./bin/crosshair-overlay
```
Expected: pressing the default hotkey (Ctrl+Alt+X) toggles the crosshair even when a different application/game window has focus. In Options…, click Rebind, press a different 2+ key combo (e.g. Ctrl+Shift+F1), release all keys — the label updates and that new combo now toggles the overlay; the old combo no longer does.

- [ ] **Step 10: Commit**

```bash
git add src/hotkey.h src/hotkey.c src/options_window.h src/options_window.c src/main.c Makefile
git commit -m "Add global hotkey grab with live rebind capture in options window"
```

---

### Task 7: Import / Export presets

**Files:**
- Modify: `src/options_window.c` (add Import/Export buttons + file-chooser dialogs)

**Interfaces:**
- Consumes: `config_load`, `config_save` from Task 2 (used directly — a preset file has the same schema as the main config file, so no separate import/export functions are needed in `config.h`).
- Produces: nothing new consumed by later tasks.

- [ ] **Step 1: Modify `src/options_window.c`**

Add these two handlers after `on_rebind_clicked` (from Task 6):

```c
static void on_export_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    OptionsWindow *ow = (OptionsWindow *)user_data;

    GtkWidget *dialog = gtk_file_chooser_dialog_new("Export Crosshair Preset", GTK_WINDOW(ow->window),
        GTK_FILE_CHOOSER_ACTION_SAVE,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Save", GTK_RESPONSE_ACCEPT,
        NULL);
    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog), TRUE);
    gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), "crosshair-preset.json");

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        GError *error = NULL;
        if (!config_save(ow->cfg, filename, &error)) {
            GtkWidget *err_dialog = gtk_message_dialog_new(GTK_WINDOW(ow->window), GTK_DIALOG_MODAL,
                GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "Failed to export preset: %s",
                error ? error->message : "unknown error");
            gtk_dialog_run(GTK_DIALOG(err_dialog));
            gtk_widget_destroy(err_dialog);
            g_clear_error(&error);
        }
        g_free(filename);
    }
    gtk_widget_destroy(dialog);
}

static void refresh_all_widgets_from_cfg(OptionsWindow *ow) {
    ow->updating_ui = TRUE;
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(ow->x_spin), ow->cfg->offset_x);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(ow->y_spin), ow->cfg->offset_y);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(ow->size_spin), ow->cfg->size_percent);
    gtk_combo_box_set_active(GTK_COMBO_BOX(ow->monitor_combo), ow->cfg->monitor);
    gtk_combo_box_set_active(GTK_COMBO_BOX(ow->shape_combo), (int)ow->cfg->shape);
    ow->updating_ui = FALSE;
    refresh_color_widgets(ow);
    refresh_hotkey_label(ow);
}

static void on_import_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    OptionsWindow *ow = (OptionsWindow *)user_data;

    GtkWidget *dialog = gtk_file_chooser_dialog_new("Import Crosshair Preset", GTK_WINDOW(ow->window),
        GTK_FILE_CHOOSER_ACTION_OPEN,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Open", GTK_RESPONSE_ACCEPT,
        NULL);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));

        if (!g_file_test(filename, G_FILE_TEST_EXISTS)) {
            GtkWidget *err_dialog = gtk_message_dialog_new(GTK_WINDOW(ow->window), GTK_DIALOG_MODAL,
                GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "File does not exist.");
            gtk_dialog_run(GTK_DIALOG(err_dialog));
            gtk_widget_destroy(err_dialog);
        } else {
            CrosshairConfig imported;
            GError *error = NULL;
            gboolean parse_ok = TRUE;

            JsonParser *parser = json_parser_new();
            if (!json_parser_load_from_file(parser, filename, &error)) {
                parse_ok = FALSE;
            }
            g_object_unref(parser);

            if (!parse_ok) {
                GtkWidget *err_dialog = gtk_message_dialog_new(GTK_WINDOW(ow->window), GTK_DIALOG_MODAL,
                    GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "Failed to import preset: invalid JSON.\n%s",
                    error ? error->message : "");
                gtk_dialog_run(GTK_DIALOG(err_dialog));
                gtk_widget_destroy(err_dialog);
                g_clear_error(&error);
            } else {
                config_load(&imported, filename, &error);
                g_clear_error(&error);

                CrosshairShape old_shape = ow->cfg->shape;
                (void)old_shape;
                *ow->cfg = imported;

                apply_and_save(ow);
                refresh_all_widgets_from_cfg(ow);
            }
        }
        g_free(filename);
    }
    gtk_widget_destroy(dialog);
}
```

This uses `JsonParser` directly, so add `#include <json-glib/json-glib.h>` to the top of `options_window.c`.

**Note on ownership:** `*ow->cfg = imported;` performs a struct copy including the `hotkey_keys` char pointers from `imported`. Since `imported` is a local stack variable that goes out of scope right after (and its pointers are never freed via `config_free_contents(&imported)`), ownership of those heap strings transfers cleanly to `*ow->cfg` with no double-free and no leak — this only works because `imported` is deliberately never freed here. Leave a comment to this effect in the code so a future editor doesn't "fix" it by adding a freeing call that would create a dangling pointer in `ow->cfg`.

Add that comment directly above the `*ow->cfg = imported;` line:

```c
                /* Struct copy transfers ownership of imported's heap-allocated
                   hotkey_keys strings into *ow->cfg. Do not call
                   config_free_contents(&imported) - that would free strings
                   that ow->cfg now also points to. */
                *ow->cfg = imported;
```

But first, `ow->cfg`'s *previous* hotkey strings must be freed before being overwritten, or they leak. Replace the struct-copy line with:

```c
                for (int i = 0; i < ow->cfg->hotkey_count; i++) {
                    g_free(ow->cfg->hotkey_keys[i]);
                }
                *ow->cfg = imported;
```

Now add the Import/Export buttons to `options_window_new`, right after the Hotkey row block from Task 6 (after `refresh_hotkey_label(ow);` was called, before the closing of the function):

```c
    GtkWidget *io_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *import_button = gtk_button_new_with_label("Import…");
    GtkWidget *export_button = gtk_button_new_with_label("Export…");
    g_signal_connect(import_button, "clicked", G_CALLBACK(on_import_clicked), ow);
    g_signal_connect(export_button, "clicked", G_CALLBACK(on_export_clicked), ow);
    gtk_box_pack_start(GTK_BOX(io_box), import_button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(io_box), export_button, FALSE, FALSE, 0);
    gtk_grid_attach(GTK_GRID(grid), io_box, 0, row, 2, 1);
    row++;
```

- [ ] **Step 2: Self-review**

Confirm `on_import_clicked` and `on_export_clicked` are declared/defined before `options_window_new` (or forward-declared) since `options_window_new` references them in `g_signal_connect`. Confirm the hotkey-string-freeing loop was actually inserted before the struct copy (not left as dead code alongside the old unsafe version) — there should be exactly one `*ow->cfg = imported;` line total in the final file.

- [ ] **Step 3: Manual verification (user runs on Linux)**

```bash
make
./bin/crosshair-overlay
```
Expected: in Options…, Export… saves the current settings to a chosen file; changing settings and then Import…-ing that same file restores the exported values (position, size, shape, color, opacity, hotkey) and the crosshair updates to match; importing a file containing invalid JSON shows an error dialog and leaves current settings untouched.

- [ ] **Step 4: Commit**

```bash
git add src/options_window.c
git commit -m "Add import/export of crosshair presets as JSON files"
```

---

### Task 8: Final integration, enable/disable checkbox, in-app limitation note, polish

**Files:**
- Modify: `src/options_window.c` (add Enable/Disable checkbox mirrored with the tray, add the fullscreen-exclusive limitation note label)
- Modify: `src/main.c` (keep tray and options window's enable state in sync in both directions)
- Modify: `README.md` (already has the manual verification checklist from Task 1 — confirm it's still accurate; no content changes expected unless self-review below finds gaps)

**Interfaces:**
- Consumes: everything from Tasks 1–7.
- Produces: nothing new (this is the final wiring/polish task).

- [ ] **Step 1: Modify `src/options_window.c`**

Add a field to `struct _OptionsWindow`: `GtkWidget *enabled_check;` and a callback type/field so `main.c` can be notified of enable/disable changes made from the options window (mirroring the tray). Add near the `HotkeyChangedCallback` fields:

```c
    GtkWidget *enabled_check;
    EnabledChangedCallback enabled_changed_cb;
    gpointer enabled_changed_user_data;
```

Add the handler function (after `on_opacity_changed`, before `populate_monitors`):

```c
static void on_enabled_toggled(GtkToggleButton *button, gpointer user_data) {
    OptionsWindow *ow = (OptionsWindow *)user_data;
    if (ow->updating_ui) return;
    ow->cfg->enabled = gtk_toggle_button_get_active(button);
    apply_and_save(ow);
    if (ow->enabled_changed_cb) ow->enabled_changed_cb(ow->enabled_changed_user_data);
}
```

Add the public setter/sync functions (used by `main.c`), placed after `options_window_set_hotkey_changed_callback`:

```c
void options_window_set_enabled_changed_callback(OptionsWindow *ow, EnabledChangedCallback cb, gpointer user_data) {
    ow->enabled_changed_cb = cb;
    ow->enabled_changed_user_data = user_data;
}

void options_window_sync_enabled(OptionsWindow *ow, gboolean enabled) {
    ow->updating_ui = TRUE;
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ow->enabled_check), enabled);
    ow->updating_ui = FALSE;
}
```

Add the checkbox and the limitation note to `options_window_new`, right after the Import/Export box from Task 7 (after that `row++;`):

```c
    ow->enabled_check = gtk_check_button_new_with_label("Enabled");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ow->enabled_check), cfg->enabled);
    g_signal_connect(ow->enabled_check, "toggled", G_CALLBACK(on_enabled_toggled), ow);
    gtk_grid_attach(GTK_GRID(grid), ow->enabled_check, 0, row, 2, 1);
    row++;

    GtkWidget *note = gtk_label_new(
        "Note: true fullscreen-exclusive games may hide the overlay (a Linux/X11\n"
        "limitation shared by all overlay tools). Use borderless/windowed-fullscreen.");
    gtk_label_set_xalign(GTK_LABEL(note), 0.0);
    gtk_style_context_add_class(gtk_widget_get_style_context(note), "dim-label");
    gtk_grid_attach(GTK_GRID(grid), note, 0, row, 2, 1);
    row++;
```

- [ ] **Step 2: Modify `src/options_window.h`**

Add the typedef and two new declarations:

```c
typedef void (*EnabledChangedCallback)(gpointer user_data);
void options_window_set_enabled_changed_callback(OptionsWindow *ow, EnabledChangedCallback cb, gpointer user_data);
void options_window_sync_enabled(OptionsWindow *ow, gboolean enabled);
```

- [ ] **Step 3: Modify `src/main.c`**

Update `on_toggle` (the tray callback) to also sync the options window's checkbox, and add a new callback for when the *options window's* checkbox changes so the tray label stays in sync too:

```c
static void on_toggle(gpointer user_data) {
    AppState *app = (AppState *)user_data;
    app->cfg.enabled = !app->cfg.enabled;
    overlay_window_apply_config(app->overlay, &app->cfg);
    tray_icon_set_enabled(app->tray, app->cfg.enabled);
    options_window_sync_enabled(app->options, app->cfg.enabled);
    save_current_config(app);
}

static void on_enabled_changed_from_options(gpointer user_data) {
    AppState *app = (AppState *)user_data;
    overlay_window_apply_config(app->overlay, &app->cfg);
    tray_icon_set_enabled(app->tray, app->cfg.enabled);
}
```

Replace the existing `on_toggle` definition with the updated one above, add `on_enabled_changed_from_options` right after it, and in `main`, after `options_window_set_hotkey_changed_callback(app.options, on_hotkey_changed, &app);`, add:

```c
    options_window_set_enabled_changed_callback(app.options, on_enabled_changed_from_options, &app);
```

- [ ] **Step 4: Self-review the full `src/main.c`**

Read the complete file top to bottom and confirm: every function is declared before its first use (C's single-pass compilation requires this — `on_toggle`, `on_enabled_changed_from_options`, `on_options`, `on_quit`, `toggle_from_hotkey`, `regrab_hotkey`, `on_hotkey_changed` must all appear, in some valid order, before `main`); every `AppState` field used across all these functions (`cfg`, `overlay`, `tray`, `options`, `config_path`) is declared in the struct; every `options_window_*` and `tray_icon_*` and `hotkey_*` function called matches its header declaration's name, parameter count, and order exactly.

- [ ] **Step 5: Self-review `README.md` against the finished feature set**

Re-read the manual verification checklist written in Task 1 and confirm every bullet still matches what was actually built (it was written prospectively before implementation) — in particular the Enable/Disable checkbox in Options now exists and can be added as an explicit checklist line if not already covered by the existing "Import/Export" and "Rebind" bullets. Add this line to the checklist if missing:

```markdown
- [ ] The "Enabled" checkbox in Options and the tray's Enable/Disable both stay in sync with each other and with the hotkey toggle.
```

- [ ] **Step 6: Manual verification (user runs on Linux)**

```bash
make
./bin/crosshair-overlay
```
Then work through the full `README.md` manual verification checklist top to bottom, including the newly added enable/disable-sync line. Report back any checklist item that fails.

- [ ] **Step 7: Commit**

```bash
git add src/options_window.h src/options_window.c src/main.c README.md
git commit -m "Sync enable/disable state between tray and options window; add fullscreen limitation note"
```

---

## Self-Review Notes (from writing this plan)

- **Spec coverage:** import/export (Task 7), tray toggle (Task 4), rebindable 2+-key global hotkey (Task 6), X/Y at 0,0-center with % size (Tasks 2, 3, 5), monitor selection (Tasks 3, 5), Cross/Dot/Circle with color+opacity (Tasks 2, 3, 5), options window as the only way to configure — not raw file editing (Task 5), MIT license (Task 1), fullscreen-exclusive limitation noted in-app (Task 8) and in README (Task 1). All spec sections have a corresponding task.
- **Build constraint:** every task's verification step was rewritten to be explicit about running on the user's Linux machine, not in this (Windows, no toolchain) environment, per the Global Constraints section.
- **Type/signature consistency:** double-checked that `CrosshairConfig`, `OverlayWindow`, `TrayIcon`, `OptionsWindow`, and the four callback typedefs (`TrayCallback`, `HotkeyToggleCallback`, `HotkeyChangedCallback`, `EnabledChangedCallback`) are used with matching names/signatures everywhere they're referenced across tasks.
