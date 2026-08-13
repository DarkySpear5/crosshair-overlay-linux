# Outline Toggle & Custom PNG Crosshair Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a per-shape, toggleable outline (color + thickness) to Cross/Dot/Circle, and a custom PNG crosshair (max 16x16) as a 4th mutually-exclusive Shape option, with the Options window reorganized into labeled sections so the new controls stay easy to follow.

**Architecture:** Extend `CrosshairConfig`'s per-shape structs with outline fields and add a `SHAPE_CUSTOM_PNG` enum value plus a base64-encoded PNG field, so the existing single-active-shape model (and its JSON save/load/import/export path) covers the new features without a parallel code path. `overlay_window.c` gains outline double-stroke rendering and cached-pixbuf PNG rendering. `options_window.c` is reorganized into `GtkFrame` sections and gains outline controls plus PNG import with validation.

**Tech Stack:** C, GTK3, Cairo, GdkPixbuf (already pulled in transitively by GTK3, no new pkg-config dependency), json-glib, GLib base64 (`g_base64_encode`/`g_base64_decode`).

## Global Constraints

- This is being implemented on a Windows machine with no C/GTK toolchain (established in the original project plan) — every task's verification step is written for the user to run on their Linux machine, not something this session executes. Self-review each task by re-reading the code for signature/logic consistency before committing.
- Outline settings are per-shape (Cross/Dot/Circle each keep their own toggle/color/thickness), matching how color/opacity already work per-shape.
- Custom Image is a 4th value in the existing Shape dropdown, not a separate overlay layer — exactly one shape is ever drawn, by construction.
- Imported PNGs must be `<= 16x16` in both dimensions or the import is rejected with an error dialog stating the actual size; no auto-downscaling.
- The custom PNG's raw bytes are base64-encoded directly into the same config/preset JSON (`custom_png_base64` field) — no separate asset file.
- When Shape = Custom Image, the Appearance section (Color/Opacity/Outline) is hidden entirely (not grayed out); Position/Size/Monitor remain visible and functional for every shape.
- No outline around a custom PNG; no auto-downscaling of oversized PNGs; no animated/multi-frame image support — all explicitly out of scope per the spec.

---

## File Structure

```
src/config.h            # Modify: outline fields, SHAPE_CUSTOM_PNG, custom_png_base64
src/config.c            # Modify: defaults, free, save/load, shape string mapping
tests/test_config.c     # Modify: round-trip coverage for outline + custom PNG shape
src/overlay_window.c    # Modify: outline rendering, custom PNG rendering, sizing
src/options_window.c    # Modify: framed layout, outline controls, PNG import UI
README.md               # Modify: new manual verification checklist lines
```

`src/options_window.h`, `src/overlay_window.h`, `src/main.c`, `src/tray.h/.c`, and `src/hotkey.h/.c` need no changes — this feature is entirely internal to the config/overlay/options modules and their existing public interfaces.

---

### Task 1: Config module — outline fields, Custom Image shape, base64 PNG storage

**Files:**
- Modify: `src/config.h`
- Modify: `src/config.c`
- Modify: `tests/test_config.c`

**Interfaces:**
- Consumes: nothing new.
- Produces (used by Tasks 2 and 3):
  - `CrossSettings`/`DotSettings`/`CircleSettings` each gain: `gboolean outline_enabled; double outline_r, outline_g, outline_b; double outline_thickness;`
  - `CrosshairShape` gains `SHAPE_CUSTOM_PNG` as its 4th value (after `SHAPE_CIRCLE`).
  - `CrosshairConfig` gains `char *custom_png_base64;` (NULL when unset; owned by the config, freed by `config_free_contents`).
  - `shape_to_string(SHAPE_CUSTOM_PNG)` returns `"custom_png"`; `shape_from_string("custom_png", ...)` returns `SHAPE_CUSTOM_PNG`.

- [ ] **Step 1: Write the failing test additions in `tests/test_config.c`**

Add two new test functions and update `test_defaults`. Replace the whole file with:

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
    CHECK(cfg.hotkey_count == 3, "defaults: hotkey has 3 keys (Ctrl+Alt+X)");
    CHECK(cfg.cross.outline_enabled == FALSE, "defaults: cross outline off");
    CHECK(cfg.dot.outline_enabled == FALSE, "defaults: dot outline off");
    CHECK(cfg.circle.outline_enabled == FALSE, "defaults: circle outline off");
    CHECK(cfg.cross.outline_thickness == 1.0, "defaults: cross outline thickness");
    CHECK(cfg.custom_png_base64 == NULL, "defaults: no custom png");
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

static void test_outline_round_trip(void) {
    CrosshairConfig cfg;
    config_set_defaults(&cfg);
    cfg.circle.outline_enabled = TRUE;
    cfg.circle.outline_r = 1.0; cfg.circle.outline_g = 1.0; cfg.circle.outline_b = 1.0;
    cfg.circle.outline_thickness = 2.5;

    const char *path = "/tmp/crosshair_overlay_test_outline.json";
    GError *error = NULL;
    gboolean saved = config_save(&cfg, path, &error);
    CHECK(saved && error == NULL, "outline: save succeeds");
    g_clear_error(&error);

    CrosshairConfig loaded;
    gboolean ok = config_load(&loaded, path, &error);
    CHECK(ok && error == NULL, "outline: load succeeds");
    g_clear_error(&error);

    CHECK(loaded.circle.outline_enabled == TRUE, "outline: enabled round-trips");
    CHECK(loaded.circle.outline_r == 1.0 && loaded.circle.outline_g == 1.0 && loaded.circle.outline_b == 1.0,
          "outline: color round-trips");
    CHECK(loaded.circle.outline_thickness > 2.4 && loaded.circle.outline_thickness < 2.6,
          "outline: thickness round-trips");

    config_free_contents(&cfg);
    config_free_contents(&loaded);
    remove(path);
}

static void test_custom_png_round_trip(void) {
    CrosshairConfig cfg;
    config_set_defaults(&cfg);
    cfg.shape = SHAPE_CUSTOM_PNG;
    cfg.custom_png_base64 = g_strdup("aGVsbG8gd29ybGQ=");

    const char *path = "/tmp/crosshair_overlay_test_png.json";
    GError *error = NULL;
    gboolean saved = config_save(&cfg, path, &error);
    CHECK(saved && error == NULL, "custom png: save succeeds");
    g_clear_error(&error);

    CrosshairConfig loaded;
    gboolean ok = config_load(&loaded, path, &error);
    CHECK(ok && error == NULL, "custom png: load succeeds");
    g_clear_error(&error);

    CHECK(loaded.shape == SHAPE_CUSTOM_PNG, "custom png: shape round-trips");
    CHECK(loaded.custom_png_base64 != NULL && strcmp(loaded.custom_png_base64, "aGVsbG8gd29ybGQ=") == 0,
          "custom png: base64 round-trips");

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
    test_outline_round_trip();
    test_custom_png_round_trip();
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

- [ ] **Step 2: Write `src/config.h`**

Replace the whole file with:

```c
#ifndef CONFIG_H
#define CONFIG_H

#include <glib.h>

#define HOTKEY_MAX_KEYS 4

typedef enum {
    SHAPE_CROSS = 0,
    SHAPE_DOT,
    SHAPE_CIRCLE,
    SHAPE_CUSTOM_PNG
} CrosshairShape;

typedef struct {
    double length;
    double thickness;
    double gap;
    double r, g, b;
    double opacity;
    gboolean outline_enabled;
    double outline_r, outline_g, outline_b;
    double outline_thickness;
} CrossSettings;

typedef struct {
    double radius;
    double r, g, b;
    double opacity;
    gboolean outline_enabled;
    double outline_r, outline_g, outline_b;
    double outline_thickness;
} DotSettings;

typedef struct {
    double radius;
    double thickness;
    double r, g, b;
    double opacity;
    gboolean outline_enabled;
    double outline_r, outline_g, outline_b;
    double outline_thickness;
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
    char *custom_png_base64;
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

- [ ] **Step 3: Write `src/config.c`**

Replace the whole file with:

```c
#include "config.h"
#include <json-glib/json-glib.h>
#include <glib/gstdio.h>
#include <string.h>
#include <stdio.h>

static void set_outline_defaults(gboolean *enabled, double *r, double *g, double *b, double *thickness) {
    *enabled = FALSE;
    *r = 0.0; *g = 0.0; *b = 0.0;
    *thickness = 1.0;
}

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
    set_outline_defaults(&cfg->cross.outline_enabled, &cfg->cross.outline_r, &cfg->cross.outline_g,
                          &cfg->cross.outline_b, &cfg->cross.outline_thickness);

    cfg->dot.radius = 2;
    cfg->dot.r = 0.0; cfg->dot.g = 1.0; cfg->dot.b = 0.0;
    cfg->dot.opacity = 1.0;
    set_outline_defaults(&cfg->dot.outline_enabled, &cfg->dot.outline_r, &cfg->dot.outline_g,
                          &cfg->dot.outline_b, &cfg->dot.outline_thickness);

    cfg->circle.radius = 8;
    cfg->circle.thickness = 2;
    cfg->circle.r = 0.0; cfg->circle.g = 1.0; cfg->circle.b = 0.0;
    cfg->circle.opacity = 1.0;
    set_outline_defaults(&cfg->circle.outline_enabled, &cfg->circle.outline_r, &cfg->circle.outline_g,
                          &cfg->circle.outline_b, &cfg->circle.outline_thickness);

    cfg->custom_png_base64 = NULL;

    char *keys[3] = { "Ctrl", "Alt", "X" };
    config_set_hotkey(cfg, keys, 3);
}

void config_free_contents(CrosshairConfig *cfg) {
    for (int i = 0; i < cfg->hotkey_count; i++) {
        g_free(cfg->hotkey_keys[i]);
        cfg->hotkey_keys[i] = NULL;
    }
    cfg->hotkey_count = 0;
    g_free(cfg->custom_png_base64);
    cfg->custom_png_base64 = NULL;
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
        case SHAPE_CROSS:      return "cross";
        case SHAPE_DOT:        return "dot";
        case SHAPE_CIRCLE:     return "circle";
        case SHAPE_CUSTOM_PNG: return "custom_png";
        default:                return "cross";
    }
}

gboolean shape_from_string(const char *str, CrosshairShape *out) {
    if (g_strcmp0(str, "cross") == 0)      { *out = SHAPE_CROSS;      return TRUE; }
    if (g_strcmp0(str, "dot") == 0)        { *out = SHAPE_DOT;        return TRUE; }
    if (g_strcmp0(str, "circle") == 0)     { *out = SHAPE_CIRCLE;     return TRUE; }
    if (g_strcmp0(str, "custom_png") == 0) { *out = SHAPE_CUSTOM_PNG; return TRUE; }
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
                                double radius, double r, double g, double bl, double opacity,
                                gboolean outline_enabled, double outline_r, double outline_g, double outline_b,
                                double outline_thickness) {
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

    json_builder_set_member_name(b, "outline_enabled"); json_builder_add_boolean_value(b, outline_enabled);
    char *outline_hex = color_to_hex(outline_r, outline_g, outline_b);
    json_builder_set_member_name(b, "outline_color"); json_builder_add_string_value(b, outline_hex);
    g_free(outline_hex);
    json_builder_set_member_name(b, "outline_thickness"); json_builder_add_double_value(b, outline_thickness);

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
                        0, cfg->cross.r, cfg->cross.g, cfg->cross.b, cfg->cross.opacity,
                        cfg->cross.outline_enabled, cfg->cross.outline_r, cfg->cross.outline_g,
                        cfg->cross.outline_b, cfg->cross.outline_thickness);
    build_shape_object(b, "dot", FALSE, FALSE, 0, 0, 0,
                        cfg->dot.radius, cfg->dot.r, cfg->dot.g, cfg->dot.b, cfg->dot.opacity,
                        cfg->dot.outline_enabled, cfg->dot.outline_r, cfg->dot.outline_g,
                        cfg->dot.outline_b, cfg->dot.outline_thickness);
    build_shape_object(b, "circle", FALSE, TRUE, 0, cfg->circle.thickness, 0,
                        cfg->circle.radius, cfg->circle.r, cfg->circle.g, cfg->circle.b, cfg->circle.opacity,
                        cfg->circle.outline_enabled, cfg->circle.outline_r, cfg->circle.outline_g,
                        cfg->circle.outline_b, cfg->circle.outline_thickness);

    json_builder_set_member_name(b, "hotkey");
    json_builder_begin_array(b);
    for (int i = 0; i < cfg->hotkey_count; i++) {
        json_builder_add_string_value(b, cfg->hotkey_keys[i]);
    }
    json_builder_end_array(b);

    json_builder_set_member_name(b, "custom_png_base64");
    json_builder_add_string_value(b, cfg->custom_png_base64 ? cfg->custom_png_base64 : "");

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

static void load_outline(JsonObject *s, gboolean *outline_enabled, double *outline_r, double *outline_g,
                          double *outline_b, double *outline_thickness) {
    if (json_object_has_member(s, "outline_enabled"))
        *outline_enabled = json_object_get_boolean_member(s, "outline_enabled");
    *outline_thickness = get_double_or(s, "outline_thickness", *outline_thickness);
    if (json_object_has_member(s, "outline_color"))
        hex_to_color(json_object_get_string_member(s, "outline_color"), outline_r, outline_g, outline_b);
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
        load_outline(s, &cfg->cross.outline_enabled, &cfg->cross.outline_r, &cfg->cross.outline_g,
                     &cfg->cross.outline_b, &cfg->cross.outline_thickness);
    }
    if (json_object_has_member(obj, "dot")) {
        JsonObject *s = json_object_get_object_member(obj, "dot");
        cfg->dot.radius = get_double_or(s, "radius", cfg->dot.radius);
        cfg->dot.opacity = get_double_or(s, "opacity", cfg->dot.opacity);
        if (json_object_has_member(s, "color"))
            hex_to_color(json_object_get_string_member(s, "color"), &cfg->dot.r, &cfg->dot.g, &cfg->dot.b);
        load_outline(s, &cfg->dot.outline_enabled, &cfg->dot.outline_r, &cfg->dot.outline_g,
                     &cfg->dot.outline_b, &cfg->dot.outline_thickness);
    }
    if (json_object_has_member(obj, "circle")) {
        JsonObject *s = json_object_get_object_member(obj, "circle");
        cfg->circle.radius = get_double_or(s, "radius", cfg->circle.radius);
        cfg->circle.thickness = get_double_or(s, "thickness", cfg->circle.thickness);
        cfg->circle.opacity = get_double_or(s, "opacity", cfg->circle.opacity);
        if (json_object_has_member(s, "color"))
            hex_to_color(json_object_get_string_member(s, "color"), &cfg->circle.r, &cfg->circle.g, &cfg->circle.b);
        load_outline(s, &cfg->circle.outline_enabled, &cfg->circle.outline_r, &cfg->circle.outline_g,
                     &cfg->circle.outline_b, &cfg->circle.outline_thickness);
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

    if (json_object_has_member(obj, "custom_png_base64")) {
        const char *s = json_object_get_string_member(obj, "custom_png_base64");
        g_free(cfg->custom_png_base64);
        cfg->custom_png_base64 = (s && s[0]) ? g_strdup(s) : NULL;
    }

    g_object_unref(parser);
    return TRUE;
}
```

- [ ] **Step 4: Self-review**

Confirm every `CrossSettings`/`DotSettings`/`CircleSettings` field referenced in `config.c` (`outline_enabled`, `outline_r/g/b`, `outline_thickness`) matches exactly what's declared in `config.h`. Confirm `build_shape_object`'s new 5 trailing parameters are passed in the same order at all 3 call sites in `config_save`. Confirm `shape_to_string`/`shape_from_string` handle all 4 enum values symmetrically.

- [ ] **Step 5: Manual verification (user runs on Linux)**

```bash
make test
```
Expected: every line prints `PASS:`, ending in `All checks passed`, exit code 0.

- [ ] **Step 6: Commit**

```bash
git add src/config.h src/config.c tests/test_config.c
git commit -m "Add outline fields and custom PNG shape to config schema"
```

---

### Task 2: Overlay rendering — outline double-stroke and custom PNG drawing

**Files:**
- Modify: `src/overlay_window.c`

**Interfaces:**
- Consumes: `CrosshairConfig`'s new `outline_*` fields and `custom_png_base64`/`SHAPE_CUSTOM_PNG` from Task 1.
- Produces: no new public functions — `overlay_window_apply_config`'s existing signature is unchanged; it now also decodes/caches the custom PNG internally.

- [ ] **Step 1: Write `src/overlay_window.c`**

Replace the whole file with:

```c
#include "overlay_window.h"
#include <gdk/gdkx.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <X11/extensions/shape.h>
#include <math.h>

struct _OverlayWindow {
    GtkWidget *window;
    CrosshairConfig cfg;
    gboolean shaped_once;
    GdkPixbuf *custom_pixbuf; /* cached decode of cfg.custom_png_base64, NULL if none/invalid */
};

static void refresh_custom_pixbuf(OverlayWindow *ow) {
    if (ow->custom_pixbuf) {
        g_object_unref(ow->custom_pixbuf);
        ow->custom_pixbuf = NULL;
    }
    if (ow->cfg.shape != SHAPE_CUSTOM_PNG || !ow->cfg.custom_png_base64 || !ow->cfg.custom_png_base64[0]) {
        return;
    }

    gsize decoded_len = 0;
    guchar *decoded = g_base64_decode(ow->cfg.custom_png_base64, &decoded_len);
    if (!decoded) return;

    GdkPixbufLoader *loader = gdk_pixbuf_loader_new();
    GError *error = NULL;
    if (gdk_pixbuf_loader_write(loader, decoded, decoded_len, &error) &&
        gdk_pixbuf_loader_close(loader, &error)) {
        GdkPixbuf *pixbuf = gdk_pixbuf_loader_get_pixbuf(loader);
        if (pixbuf) {
            ow->custom_pixbuf = g_object_ref(pixbuf);
        }
    } else {
        g_warning("Failed to decode custom crosshair PNG: %s", error ? error->message : "unknown error");
    }
    g_clear_error(&error);
    g_object_unref(loader);
    g_free(decoded);
}

static int shape_bounding_size(OverlayWindow *ow) {
    const CrosshairConfig *cfg = &ow->cfg;
    double scale = cfg->size_percent / 100.0;
    double half;
    double pad = 4;
    switch (cfg->shape) {
        case SHAPE_CROSS:
            half = (cfg->cross.length + cfg->cross.gap) * scale;
            if (cfg->cross.outline_enabled) pad += cfg->cross.outline_thickness * scale * 2.0;
            break;
        case SHAPE_DOT:
            half = cfg->dot.radius * scale;
            if (cfg->dot.outline_enabled) pad += cfg->dot.outline_thickness * scale;
            break;
        case SHAPE_CIRCLE:
            half = (cfg->circle.radius + cfg->circle.thickness) * scale;
            if (cfg->circle.outline_enabled) pad += cfg->circle.outline_thickness * scale * 2.0;
            break;
        case SHAPE_CUSTOM_PNG: {
            int w = ow->custom_pixbuf ? gdk_pixbuf_get_width(ow->custom_pixbuf) : 16;
            int h = ow->custom_pixbuf ? gdk_pixbuf_get_height(ow->custom_pixbuf) : 16;
            half = (MAX(w, h) / 2.0) * scale;
            break;
        }
        default:
            half = 10;
    }
    int size = (int)ceil(half * 2.0) + (int)ceil(pad);
    return size < 8 ? 8 : size;
}

static void build_cross_path(cairo_t *cr, double cx, double cy, double len, double gap) {
    cairo_move_to(cr, cx - gap - len, cy);
    cairo_line_to(cr, cx - gap, cy);
    cairo_move_to(cr, cx + gap, cy);
    cairo_line_to(cr, cx + gap + len, cy);
    cairo_move_to(cr, cx, cy - gap - len);
    cairo_line_to(cr, cx, cy - gap);
    cairo_move_to(cr, cx, cy + gap);
    cairo_line_to(cr, cx, cy + gap + len);
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
            cairo_set_line_cap(cr, CAIRO_LINE_CAP_BUTT);

            if (ow->cfg.cross.outline_enabled) {
                double outline_thick = thick + 2.0 * ow->cfg.cross.outline_thickness * scale;
                cairo_set_source_rgba(cr, ow->cfg.cross.outline_r, ow->cfg.cross.outline_g,
                                      ow->cfg.cross.outline_b, ow->cfg.cross.opacity);
                cairo_set_line_width(cr, outline_thick);
                build_cross_path(cr, cx, cy, len, gap);
                cairo_stroke(cr);
            }

            cairo_set_source_rgba(cr, ow->cfg.cross.r, ow->cfg.cross.g, ow->cfg.cross.b, ow->cfg.cross.opacity);
            cairo_set_line_width(cr, thick);
            build_cross_path(cr, cx, cy, len, gap);
            cairo_stroke(cr);
            break;
        }
        case SHAPE_DOT: {
            double r = ow->cfg.dot.radius * scale;
            if (ow->cfg.dot.outline_enabled) {
                double outline_r = r + ow->cfg.dot.outline_thickness * scale;
                cairo_set_source_rgba(cr, ow->cfg.dot.outline_r, ow->cfg.dot.outline_g,
                                      ow->cfg.dot.outline_b, ow->cfg.dot.opacity);
                cairo_arc(cr, cx, cy, outline_r, 0, 2 * G_PI);
                cairo_fill(cr);
            }
            cairo_set_source_rgba(cr, ow->cfg.dot.r, ow->cfg.dot.g, ow->cfg.dot.b, ow->cfg.dot.opacity);
            cairo_arc(cr, cx, cy, r, 0, 2 * G_PI);
            cairo_fill(cr);
            break;
        }
        case SHAPE_CIRCLE: {
            double r = ow->cfg.circle.radius * scale;
            double thick = ow->cfg.circle.thickness * scale;
            if (ow->cfg.circle.outline_enabled) {
                double outline_thick = thick + 2.0 * ow->cfg.circle.outline_thickness * scale;
                cairo_set_source_rgba(cr, ow->cfg.circle.outline_r, ow->cfg.circle.outline_g,
                                      ow->cfg.circle.outline_b, ow->cfg.circle.opacity);
                cairo_set_line_width(cr, outline_thick);
                cairo_arc(cr, cx, cy, r, 0, 2 * G_PI);
                cairo_stroke(cr);
            }
            cairo_set_source_rgba(cr, ow->cfg.circle.r, ow->cfg.circle.g, ow->cfg.circle.b, ow->cfg.circle.opacity);
            cairo_set_line_width(cr, thick);
            cairo_arc(cr, cx, cy, r, 0, 2 * G_PI);
            cairo_stroke(cr);
            break;
        }
        case SHAPE_CUSTOM_PNG: {
            if (ow->custom_pixbuf) {
                int pw = gdk_pixbuf_get_width(ow->custom_pixbuf);
                int ph = gdk_pixbuf_get_height(ow->custom_pixbuf);
                cairo_save(cr);
                cairo_translate(cr, cx - (pw * scale) / 2.0, cy - (ph * scale) / 2.0);
                cairo_scale(cr, scale, scale);
                gdk_cairo_set_source_pixbuf(cr, ow->custom_pixbuf, 0, 0);
                cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_NEAREST);
                cairo_paint(cr);
                cairo_restore(cr);
            }
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
    int size = shape_bounding_size(ow);
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
    refresh_custom_pixbuf(ow);
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

- [ ] **Step 2: Self-review**

Confirm `shape_bounding_size` and `reposition` both now take `OverlayWindow *ow` (not `const CrosshairConfig *cfg`) consistently — `reposition`'s call site was updated to `shape_bounding_size(ow)`. Confirm `build_cross_path` is called identically (same 4 args) from both the outline pass and the main pass in `on_draw`'s `SHAPE_CROSS` case, so the two strokes trace the exact same geometry. Confirm `refresh_custom_pixbuf` is called before `reposition` inside `overlay_window_apply_config`, since `reposition` → `shape_bounding_size` reads `ow->custom_pixbuf` for `SHAPE_CUSTOM_PNG` sizing.

- [ ] **Step 3: Manual verification (user runs on Linux)**

```bash
make
./bin/crosshair-overlay
```
This task has no UI to toggle outline/PNG yet (that's Task 3) — for now, confirm the app still builds and the default green cross still renders exactly as before (no visual regression), since `outline_enabled` defaults to `FALSE` and `shape` defaults to `SHAPE_CROSS`.

- [ ] **Step 4: Commit**

```bash
git add src/overlay_window.c
git commit -m "Add outline and custom PNG rendering to the overlay window"
```

---

### Task 3: Options window — framed layout, outline controls, custom PNG import

**Files:**
- Modify: `src/options_window.c`

**Interfaces:**
- Consumes: everything from Tasks 1 and 2. No changes to `src/options_window.h` — all new UI is internal to this file's existing public API (`options_window_new`, `options_window_present`, etc.).
- Produces: no new public functions.

- [ ] **Step 1: Write `src/options_window.c`**

Replace the whole file with:

```c
#include "options_window.h"
#include "hotkey.h"
#include <json-glib/json-glib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#define MAX_CUSTOM_PNG_SIZE 16

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

    GtkWidget *appearance_frame;
    GtkWidget *color_button;
    GtkWidget *opacity_scale;
    GtkWidget *outline_check;
    GtkWidget *outline_color_button;
    GtkWidget *outline_thickness_spin;

    GtkWidget *custom_image_frame;
    GtkWidget *custom_image_preview;
    GtkWidget *custom_image_label;

    GtkWidget *hotkey_label;
    GtkWidget *hotkey_rebind_button;
    gboolean capturing_hotkey;
    GPtrArray *captured_keys; /* owns strdup'd key name strings during capture */
    HotkeyChangedCallback hotkey_changed_cb;
    gpointer hotkey_changed_user_data;

    GtkWidget *enabled_check;
    EnabledChangedCallback enabled_changed_cb;
    gpointer enabled_changed_user_data;

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
        case SHAPE_CUSTOM_PNG: *r = 0; *g = 0; *b = 0; *opacity = 1.0; break;
    }
}

static void set_active_shape_color(OptionsWindow *ow, double r, double g, double b) {
    switch (ow->cfg->shape) {
        case SHAPE_CROSS:  ow->cfg->cross.r = r;  ow->cfg->cross.g = g;  ow->cfg->cross.b = b;  break;
        case SHAPE_DOT:    ow->cfg->dot.r = r;    ow->cfg->dot.g = g;    ow->cfg->dot.b = b;    break;
        case SHAPE_CIRCLE: ow->cfg->circle.r = r; ow->cfg->circle.g = g; ow->cfg->circle.b = b; break;
        case SHAPE_CUSTOM_PNG: break;
    }
}

static void set_active_shape_opacity(OptionsWindow *ow, double opacity) {
    switch (ow->cfg->shape) {
        case SHAPE_CROSS:  ow->cfg->cross.opacity = opacity;  break;
        case SHAPE_DOT:    ow->cfg->dot.opacity = opacity;    break;
        case SHAPE_CIRCLE: ow->cfg->circle.opacity = opacity; break;
        case SHAPE_CUSTOM_PNG: break;
    }
}

static void get_active_shape_outline(OptionsWindow *ow, gboolean *enabled, double *r, double *g, double *b, double *thickness) {
    switch (ow->cfg->shape) {
        case SHAPE_CROSS:
            *enabled = ow->cfg->cross.outline_enabled; *r = ow->cfg->cross.outline_r;
            *g = ow->cfg->cross.outline_g; *b = ow->cfg->cross.outline_b;
            *thickness = ow->cfg->cross.outline_thickness; break;
        case SHAPE_DOT:
            *enabled = ow->cfg->dot.outline_enabled; *r = ow->cfg->dot.outline_r;
            *g = ow->cfg->dot.outline_g; *b = ow->cfg->dot.outline_b;
            *thickness = ow->cfg->dot.outline_thickness; break;
        case SHAPE_CIRCLE:
            *enabled = ow->cfg->circle.outline_enabled; *r = ow->cfg->circle.outline_r;
            *g = ow->cfg->circle.outline_g; *b = ow->cfg->circle.outline_b;
            *thickness = ow->cfg->circle.outline_thickness; break;
        case SHAPE_CUSTOM_PNG:
            *enabled = FALSE; *r = 0; *g = 0; *b = 0; *thickness = 1.0; break;
    }
}

static void set_active_shape_outline_enabled(OptionsWindow *ow, gboolean enabled) {
    switch (ow->cfg->shape) {
        case SHAPE_CROSS:  ow->cfg->cross.outline_enabled = enabled;  break;
        case SHAPE_DOT:    ow->cfg->dot.outline_enabled = enabled;    break;
        case SHAPE_CIRCLE: ow->cfg->circle.outline_enabled = enabled; break;
        case SHAPE_CUSTOM_PNG: break;
    }
}

static void set_active_shape_outline_color(OptionsWindow *ow, double r, double g, double b) {
    switch (ow->cfg->shape) {
        case SHAPE_CROSS:  ow->cfg->cross.outline_r = r;  ow->cfg->cross.outline_g = g;  ow->cfg->cross.outline_b = b;  break;
        case SHAPE_DOT:    ow->cfg->dot.outline_r = r;    ow->cfg->dot.outline_g = g;    ow->cfg->dot.outline_b = b;    break;
        case SHAPE_CIRCLE: ow->cfg->circle.outline_r = r; ow->cfg->circle.outline_g = g; ow->cfg->circle.outline_b = b; break;
        case SHAPE_CUSTOM_PNG: break;
    }
}

static void set_active_shape_outline_thickness(OptionsWindow *ow, double thickness) {
    switch (ow->cfg->shape) {
        case SHAPE_CROSS:  ow->cfg->cross.outline_thickness = thickness;  break;
        case SHAPE_DOT:    ow->cfg->dot.outline_thickness = thickness;    break;
        case SHAPE_CIRCLE: ow->cfg->circle.outline_thickness = thickness; break;
        case SHAPE_CUSTOM_PNG: break;
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

static void refresh_outline_widgets(OptionsWindow *ow) {
    gboolean enabled; double r, g, b, thickness;
    get_active_shape_outline(ow, &enabled, &r, &g, &b, &thickness);
    GdkRGBA rgba = { r, g, b, 1.0 };
    ow->updating_ui = TRUE;
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ow->outline_check), enabled);
    gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(ow->outline_color_button), &rgba);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(ow->outline_thickness_spin), thickness);
    ow->updating_ui = FALSE;
    gtk_widget_set_sensitive(ow->outline_color_button, enabled);
    gtk_widget_set_sensitive(ow->outline_thickness_spin, enabled);
}

static void refresh_custom_image_widgets(OptionsWindow *ow) {
    if (!ow->cfg->custom_png_base64 || !ow->cfg->custom_png_base64[0]) {
        gtk_image_clear(GTK_IMAGE(ow->custom_image_preview));
        gtk_label_set_text(GTK_LABEL(ow->custom_image_label), "No image loaded");
        return;
    }

    gsize decoded_len = 0;
    guchar *decoded = g_base64_decode(ow->cfg->custom_png_base64, &decoded_len);
    if (!decoded) {
        gtk_label_set_text(GTK_LABEL(ow->custom_image_label), "No image loaded");
        return;
    }

    GdkPixbufLoader *loader = gdk_pixbuf_loader_new();
    GError *error = NULL;
    GdkPixbuf *pixbuf = NULL;
    if (gdk_pixbuf_loader_write(loader, decoded, decoded_len, &error) &&
        gdk_pixbuf_loader_close(loader, &error)) {
        pixbuf = gdk_pixbuf_loader_get_pixbuf(loader);
    }

    if (pixbuf) {
        int w = gdk_pixbuf_get_width(pixbuf);
        int h = gdk_pixbuf_get_height(pixbuf);
        GdkPixbuf *scaled = gdk_pixbuf_scale_simple(pixbuf, w * 4, h * 4, GDK_INTERP_NEAREST);
        gtk_image_set_from_pixbuf(GTK_IMAGE(ow->custom_image_preview), scaled);
        g_object_unref(scaled);
        char *label_text = g_strdup_printf("%d x %d loaded", w, h);
        gtk_label_set_text(GTK_LABEL(ow->custom_image_label), label_text);
        g_free(label_text);
    } else {
        gtk_label_set_text(GTK_LABEL(ow->custom_image_label), "No image loaded");
    }
    g_clear_error(&error);
    g_object_unref(loader);
    g_free(decoded);
}

static void update_shape_section_visibility(OptionsWindow *ow) {
    gboolean is_custom = (ow->cfg->shape == SHAPE_CUSTOM_PNG);
    gtk_widget_set_visible(ow->appearance_frame, !is_custom);
    gtk_widget_set_visible(ow->custom_image_frame, is_custom);
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
    update_shape_section_visibility(ow);
    if (ow->cfg->shape == SHAPE_CUSTOM_PNG) {
        refresh_custom_image_widgets(ow);
    } else {
        refresh_color_widgets(ow);
        refresh_outline_widgets(ow);
    }
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

static void on_outline_toggled(GtkToggleButton *button, gpointer user_data) {
    OptionsWindow *ow = (OptionsWindow *)user_data;
    if (ow->updating_ui) return;
    gboolean enabled = gtk_toggle_button_get_active(button);
    set_active_shape_outline_enabled(ow, enabled);
    gtk_widget_set_sensitive(ow->outline_color_button, enabled);
    gtk_widget_set_sensitive(ow->outline_thickness_spin, enabled);
    apply_and_save(ow);
}

static void on_outline_color_changed(GtkColorButton *button, gpointer user_data) {
    OptionsWindow *ow = (OptionsWindow *)user_data;
    if (ow->updating_ui) return;
    GdkRGBA rgba;
    gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(button), &rgba);
    set_active_shape_outline_color(ow, rgba.red, rgba.green, rgba.blue);
    apply_and_save(ow);
}

static void on_outline_thickness_changed(GtkSpinButton *spin, gpointer user_data) {
    OptionsWindow *ow = (OptionsWindow *)user_data;
    if (ow->updating_ui) return;
    set_active_shape_outline_thickness(ow, gtk_spin_button_get_value(spin));
    apply_and_save(ow);
}

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
    (void)widget;
    OptionsWindow *ow = (OptionsWindow *)user_data;
    if (!ow->capturing_hotkey) return FALSE;

    if (ow->captured_keys->len < 2) {
        return TRUE;
    }

    char *keys[HOTKEY_MAX_KEYS];
    guint n = ow->captured_keys->len > HOTKEY_MAX_KEYS ? HOTKEY_MAX_KEYS : ow->captured_keys->len;
    for (guint i = 0; i < n; i++) {
        keys[i] = (char *)g_ptr_array_index(ow->captured_keys, i);
    }

    config_set_hotkey(ow->cfg, keys, (int)n);

    ow->capturing_hotkey = FALSE;
    gtk_button_set_label(GTK_BUTTON(ow->hotkey_rebind_button), "Rebind");
    refresh_hotkey_label(ow);
    apply_and_save(ow);

    if (ow->hotkey_changed_cb) ow->hotkey_changed_cb(ow->hotkey_changed_user_data);

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
    update_shape_section_visibility(ow);
    if (ow->cfg->shape == SHAPE_CUSTOM_PNG) {
        refresh_custom_image_widgets(ow);
    } else {
        refresh_color_widgets(ow);
        refresh_outline_widgets(ow);
    }
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

                for (int i = 0; i < ow->cfg->hotkey_count; i++) {
                    g_free(ow->cfg->hotkey_keys[i]);
                }
                g_free(ow->cfg->custom_png_base64);
                /* Struct copy transfers ownership of imported's heap-allocated
                   hotkey_keys strings and custom_png_base64 into *ow->cfg. Do not
                   call config_free_contents(&imported) - that would free strings
                   that ow->cfg now also points to. */
                *ow->cfg = imported;

                apply_and_save(ow);
                refresh_all_widgets_from_cfg(ow);
            }
        }
        g_free(filename);
    }
    gtk_widget_destroy(dialog);
}

static void on_import_png_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    OptionsWindow *ow = (OptionsWindow *)user_data;

    GtkWidget *dialog = gtk_file_chooser_dialog_new("Import Custom Crosshair PNG", GTK_WINDOW(ow->window),
        GTK_FILE_CHOOSER_ACTION_OPEN,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Open", GTK_RESPONSE_ACCEPT,
        NULL);
    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "PNG images");
    gtk_file_filter_add_pattern(filter, "*.png");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        GError *error = NULL;
        GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file(filename, &error);

        if (!pixbuf) {
            GtkWidget *err_dialog = gtk_message_dialog_new(GTK_WINDOW(ow->window), GTK_DIALOG_MODAL,
                GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "Failed to load image: %s",
                error ? error->message : "unknown error");
            gtk_dialog_run(GTK_DIALOG(err_dialog));
            gtk_widget_destroy(err_dialog);
            g_clear_error(&error);
        } else {
            int w = gdk_pixbuf_get_width(pixbuf);
            int h = gdk_pixbuf_get_height(pixbuf);
            if (w > MAX_CUSTOM_PNG_SIZE || h > MAX_CUSTOM_PNG_SIZE) {
                GtkWidget *err_dialog = gtk_message_dialog_new(GTK_WINDOW(ow->window), GTK_DIALOG_MODAL,
                    GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "Image is %dx%d - must be %dx%d or smaller.",
                    w, h, MAX_CUSTOM_PNG_SIZE, MAX_CUSTOM_PNG_SIZE);
                gtk_dialog_run(GTK_DIALOG(err_dialog));
                gtk_widget_destroy(err_dialog);
            } else {
                gchar *file_contents = NULL;
                gsize file_length = 0;
                if (g_file_get_contents(filename, &file_contents, &file_length, &error)) {
                    char *encoded = g_base64_encode((const guchar *)file_contents, file_length);
                    g_free(ow->cfg->custom_png_base64);
                    ow->cfg->custom_png_base64 = encoded;
                    ow->cfg->shape = SHAPE_CUSTOM_PNG;

                    ow->updating_ui = TRUE;
                    gtk_combo_box_set_active(GTK_COMBO_BOX(ow->shape_combo), (int)SHAPE_CUSTOM_PNG);
                    ow->updating_ui = FALSE;

                    update_shape_section_visibility(ow);
                    refresh_custom_image_widgets(ow);
                    apply_and_save(ow);
                    g_free(file_contents);
                } else {
                    GtkWidget *err_dialog = gtk_message_dialog_new(GTK_WINDOW(ow->window), GTK_DIALOG_MODAL,
                        GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "Failed to read image file: %s",
                        error ? error->message : "unknown error");
                    gtk_dialog_run(GTK_DIALOG(err_dialog));
                    gtk_widget_destroy(err_dialog);
                    g_clear_error(&error);
                }
            }
            g_object_unref(pixbuf);
        }
        g_free(filename);
    }
    gtk_widget_destroy(dialog);
}

static void on_enabled_toggled(GtkToggleButton *button, gpointer user_data) {
    OptionsWindow *ow = (OptionsWindow *)user_data;
    if (ow->updating_ui) return;
    ow->cfg->enabled = gtk_toggle_button_get_active(button);
    apply_and_save(ow);
    if (ow->enabled_changed_cb) ow->enabled_changed_cb(ow->enabled_changed_user_data);
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

static GtkWidget *begin_framed_grid(GtkWidget *main_box, const char *title, GtkWidget **out_grid) {
    GtkWidget *frame = gtk_frame_new(title);
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 8);
    gtk_container_add(GTK_CONTAINER(frame), grid);
    gtk_box_pack_start(GTK_BOX(main_box), frame, FALSE, FALSE, 0);
    *out_grid = grid;
    return frame;
}

OptionsWindow *options_window_new(CrosshairConfig *cfg, OverlayWindow *overlay, const char *config_path) {
    OptionsWindow *ow = g_new0(OptionsWindow, 1);
    ow->cfg = cfg;
    ow->overlay = overlay;
    ow->config_path = g_strdup(config_path);
    ow->updating_ui = FALSE;

    ow->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(ow->window), "Crosshair Overlay - Options");
    gtk_window_set_default_size(GTK_WINDOW(ow->window), 380, -1);
    g_signal_connect(ow->window, "delete-event", G_CALLBACK(gtk_widget_hide_on_delete), NULL);

    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(main_box), 12);
    gtk_container_add(GTK_CONTAINER(ow->window), main_box);

    /* --- Position --- */
    GtkWidget *position_grid;
    begin_framed_grid(main_box, "Position", &position_grid);

    gtk_grid_attach(GTK_GRID(position_grid), gtk_label_new("X offset"), 0, 0, 1, 1);
    ow->x_spin = gtk_spin_button_new_with_range(-4096, 4096, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(ow->x_spin), cfg->offset_x);
    g_signal_connect(ow->x_spin, "value-changed", G_CALLBACK(on_x_changed), ow);
    gtk_grid_attach(GTK_GRID(position_grid), ow->x_spin, 1, 0, 1, 1);

    gtk_grid_attach(GTK_GRID(position_grid), gtk_label_new("Y offset"), 0, 1, 1, 1);
    ow->y_spin = gtk_spin_button_new_with_range(-4096, 4096, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(ow->y_spin), cfg->offset_y);
    g_signal_connect(ow->y_spin, "value-changed", G_CALLBACK(on_y_changed), ow);
    gtk_grid_attach(GTK_GRID(position_grid), ow->y_spin, 1, 1, 1, 1);

    gtk_grid_attach(GTK_GRID(position_grid), gtk_label_new("Size %"), 0, 2, 1, 1);
    ow->size_spin = gtk_spin_button_new_with_range(10, 500, 5);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(ow->size_spin), cfg->size_percent);
    g_signal_connect(ow->size_spin, "value-changed", G_CALLBACK(on_size_changed), ow);
    gtk_grid_attach(GTK_GRID(position_grid), ow->size_spin, 1, 2, 1, 1);

    gtk_grid_attach(GTK_GRID(position_grid), gtk_label_new("Monitor"), 0, 3, 1, 1);
    ow->monitor_combo = gtk_combo_box_text_new();
    populate_monitors(ow);
    gtk_combo_box_set_active(GTK_COMBO_BOX(ow->monitor_combo), cfg->monitor);
    g_signal_connect(ow->monitor_combo, "changed", G_CALLBACK(on_monitor_changed), ow);
    gtk_grid_attach(GTK_GRID(position_grid), ow->monitor_combo, 1, 3, 1, 1);

    /* --- Shape --- */
    GtkWidget *shape_grid;
    begin_framed_grid(main_box, "Shape", &shape_grid);

    gtk_grid_attach(GTK_GRID(shape_grid), gtk_label_new("Shape"), 0, 0, 1, 1);
    ow->shape_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(ow->shape_combo), "Cross");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(ow->shape_combo), "Dot");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(ow->shape_combo), "Circle");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(ow->shape_combo), "Custom Image");
    gtk_combo_box_set_active(GTK_COMBO_BOX(ow->shape_combo), (int)cfg->shape);
    g_signal_connect(ow->shape_combo, "changed", G_CALLBACK(on_shape_changed), ow);
    gtk_grid_attach(GTK_GRID(shape_grid), ow->shape_combo, 1, 0, 1, 1);

    /* --- Appearance (Cross/Dot/Circle only) --- */
    GtkWidget *appearance_grid;
    ow->appearance_frame = begin_framed_grid(main_box, "Appearance", &appearance_grid);

    gtk_grid_attach(GTK_GRID(appearance_grid), gtk_label_new("Color"), 0, 0, 1, 1);
    ow->color_button = gtk_color_button_new();
    g_signal_connect(ow->color_button, "color-set", G_CALLBACK(on_color_changed), ow);
    gtk_grid_attach(GTK_GRID(appearance_grid), ow->color_button, 1, 0, 1, 1);

    gtk_grid_attach(GTK_GRID(appearance_grid), gtk_label_new("Opacity %"), 0, 1, 1, 1);
    ow->opacity_scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 100, 1);
    g_signal_connect(ow->opacity_scale, "value-changed", G_CALLBACK(on_opacity_changed), ow);
    gtk_grid_attach(GTK_GRID(appearance_grid), ow->opacity_scale, 1, 1, 1, 1);

    ow->outline_check = gtk_check_button_new_with_label("Outline");
    g_signal_connect(ow->outline_check, "toggled", G_CALLBACK(on_outline_toggled), ow);
    gtk_grid_attach(GTK_GRID(appearance_grid), ow->outline_check, 0, 2, 2, 1);

    gtk_grid_attach(GTK_GRID(appearance_grid), gtk_label_new("Outline color"), 0, 3, 1, 1);
    ow->outline_color_button = gtk_color_button_new();
    g_signal_connect(ow->outline_color_button, "color-set", G_CALLBACK(on_outline_color_changed), ow);
    gtk_grid_attach(GTK_GRID(appearance_grid), ow->outline_color_button, 1, 3, 1, 1);

    gtk_grid_attach(GTK_GRID(appearance_grid), gtk_label_new("Outline thickness"), 0, 4, 1, 1);
    ow->outline_thickness_spin = gtk_spin_button_new_with_range(0.5, 10, 0.5);
    g_signal_connect(ow->outline_thickness_spin, "value-changed", G_CALLBACK(on_outline_thickness_changed), ow);
    gtk_grid_attach(GTK_GRID(appearance_grid), ow->outline_thickness_spin, 1, 4, 1, 1);

    /* --- Custom Image (Custom Image shape only) --- */
    ow->custom_image_frame = gtk_frame_new("Custom Image");
    GtkWidget *custom_image_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(custom_image_box), 8);
    gtk_container_add(GTK_CONTAINER(ow->custom_image_frame), custom_image_box);
    gtk_box_pack_start(GTK_BOX(main_box), ow->custom_image_frame, FALSE, FALSE, 0);

    ow->custom_image_preview = gtk_image_new();
    gtk_box_pack_start(GTK_BOX(custom_image_box), ow->custom_image_preview, FALSE, FALSE, 0);

    ow->custom_image_label = gtk_label_new("No image loaded");
    gtk_box_pack_start(GTK_BOX(custom_image_box), ow->custom_image_label, FALSE, FALSE, 0);

    GtkWidget *import_png_button = gtk_button_new_with_label("Import PNG… (max 16x16)");
    g_signal_connect(import_png_button, "clicked", G_CALLBACK(on_import_png_clicked), ow);
    gtk_box_pack_start(GTK_BOX(custom_image_box), import_png_button, FALSE, FALSE, 0);

    /* --- Hotkey --- */
    GtkWidget *hotkey_grid;
    begin_framed_grid(main_box, "Hotkey", &hotkey_grid);

    ow->captured_keys = g_ptr_array_new();
    ow->capturing_hotkey = FALSE;

    gtk_grid_attach(GTK_GRID(hotkey_grid), gtk_label_new("Toggle hotkey"), 0, 0, 1, 1);
    GtkWidget *hotkey_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    ow->hotkey_label = gtk_label_new("");
    ow->hotkey_rebind_button = gtk_button_new_with_label("Rebind");
    g_signal_connect(ow->hotkey_rebind_button, "clicked", G_CALLBACK(on_rebind_clicked), ow);
    gtk_box_pack_start(GTK_BOX(hotkey_box), ow->hotkey_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hotkey_box), ow->hotkey_rebind_button, FALSE, FALSE, 0);
    gtk_grid_attach(GTK_GRID(hotkey_grid), hotkey_box, 1, 0, 1, 1);

    g_signal_connect(ow->window, "key-press-event", G_CALLBACK(on_capture_key_press), ow);
    g_signal_connect(ow->window, "key-release-event", G_CALLBACK(on_capture_key_release), ow);

    /* --- Presets --- */
    GtkWidget *presets_frame = gtk_frame_new("Presets");
    GtkWidget *presets_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(presets_box), 8);
    gtk_container_add(GTK_CONTAINER(presets_frame), presets_box);
    gtk_box_pack_start(GTK_BOX(main_box), presets_frame, FALSE, FALSE, 0);

    GtkWidget *import_button = gtk_button_new_with_label("Import…");
    GtkWidget *export_button = gtk_button_new_with_label("Export…");
    g_signal_connect(import_button, "clicked", G_CALLBACK(on_import_clicked), ow);
    g_signal_connect(export_button, "clicked", G_CALLBACK(on_export_clicked), ow);
    gtk_box_pack_start(GTK_BOX(presets_box), import_button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(presets_box), export_button, FALSE, FALSE, 0);

    /* --- Enabled + limitation note --- */
    ow->enabled_check = gtk_check_button_new_with_label("Enabled");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ow->enabled_check), cfg->enabled);
    g_signal_connect(ow->enabled_check, "toggled", G_CALLBACK(on_enabled_toggled), ow);
    gtk_box_pack_start(GTK_BOX(main_box), ow->enabled_check, FALSE, FALSE, 0);

    GtkWidget *note = gtk_label_new(
        "Note: true fullscreen-exclusive games may hide the overlay (a Linux/X11\n"
        "limitation shared by all overlay tools). Use borderless/windowed-fullscreen.");
    gtk_label_set_xalign(GTK_LABEL(note), 0.0);
    gtk_style_context_add_class(gtk_widget_get_style_context(note), "dim-label");
    gtk_box_pack_start(GTK_BOX(main_box), note, FALSE, FALSE, 0);

    /* Reveal every widget once, then hide whichever of Appearance/Custom Image
       doesn't apply to the current shape - gtk_widget_show_all() would otherwise
       override that hiding every time it's called, so it's called exactly once
       here rather than in options_window_present(). */
    gtk_widget_show_all(main_box);

    update_shape_section_visibility(ow);
    refresh_hotkey_label(ow);
    refresh_color_widgets(ow);
    refresh_outline_widgets(ow);
    refresh_custom_image_widgets(ow);

    return ow;
}

void options_window_set_hotkey_changed_callback(OptionsWindow *ow, HotkeyChangedCallback cb, gpointer user_data) {
    ow->hotkey_changed_cb = cb;
    ow->hotkey_changed_user_data = user_data;
}

void options_window_set_enabled_changed_callback(OptionsWindow *ow, EnabledChangedCallback cb, gpointer user_data) {
    ow->enabled_changed_cb = cb;
    ow->enabled_changed_user_data = user_data;
}

void options_window_sync_enabled(OptionsWindow *ow, gboolean enabled) {
    ow->updating_ui = TRUE;
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ow->enabled_check), enabled);
    ow->updating_ui = FALSE;
}

void options_window_present(OptionsWindow *ow) {
    gtk_widget_show(ow->window);
    gtk_window_present(GTK_WINDOW(ow->window));
}

GtkWidget *options_window_get_widget(OptionsWindow *ow) {
    return ow->window;
}
```

- [ ] **Step 2: Self-review**

Confirm `options_window_present` no longer calls `gtk_widget_show_all` (that call now happens exactly once, inside `options_window_new`, followed immediately by `update_shape_section_visibility` to re-hide the inapplicable frame) — if `show_all` were called again on every present, it would undo the frame hiding every time the Options window is reopened. Confirm every `switch (ow->cfg->shape)` in this file has all 4 cases (`SHAPE_CROSS`, `SHAPE_DOT`, `SHAPE_CIRCLE`, `SHAPE_CUSTOM_PNG`) so `-Wswitch` (enabled by `-Wall`) doesn't warn. Confirm `on_import_png_clicked` guards the `gtk_combo_box_set_active` call with `ow->updating_ui = TRUE/FALSE` so it doesn't re-enter `on_shape_changed`, and that it manually calls `update_shape_section_visibility`/`refresh_custom_image_widgets`/`apply_and_save` afterward since the guard suppressed those from firing via the signal. Confirm `on_import_clicked` frees `ow->cfg->custom_png_base64` before the `*ow->cfg = imported;` struct copy, alongside the existing `hotkey_keys` freeing, so re-importing a preset doesn't leak the previous custom image string.

- [ ] **Step 3: Manual verification (user runs on Linux)**

```bash
make
./bin/crosshair-overlay
```
Then, in Options:
- Confirm the window now shows framed sections: Position, Shape, Appearance, Hotkey, Presets, plus Enabled and the note.
- With Shape = Cross, check the Outline checkbox, pick a contrasting outline color and a thickness — the crosshair immediately shows a colored border around the cross.
- Switch Shape to Custom Image — the Appearance frame disappears and a "Custom Image" frame with "Import PNG… (max 16x16)" appears instead. Click it, pick a PNG that's 16x16 or smaller — it becomes the crosshair, and the preview thumbnail + "WxH loaded" label update.
- Try importing a PNG larger than 16x16 — confirm the error dialog states the actual size and the crosshair is unchanged.
- Switch back to Cross — Appearance reappears with your outline settings intact, Custom Image frame disappears.
- Export a preset while Custom Image is active, then Import it back (or on another machine) — confirm the exact same custom image reappears.
- Close and reopen the Options window (via the tray) a few times — confirm the correct frame (Appearance or Custom Image) is shown each time, matching the current shape, without any GTK-runtime warnings printed to the terminal about invisible/visible widget state.

- [ ] **Step 4: Commit**

```bash
git add src/options_window.c
git commit -m "Add framed Options layout, outline controls, and custom PNG import"
```

---

### Task 4: README updates and final self-review

**Files:**
- Modify: `README.md`

**Interfaces:**
- Consumes: everything from Tasks 1-3.
- Produces: nothing (documentation only).

- [ ] **Step 1: Modify `README.md`**

Add these lines to the existing "Manual verification checklist" section, after the last existing bullet (`- [ ] "Crosshair Overlay" appears...` / `make uninstall` lines from the Mint install work):

```markdown
- [ ] Toggling Outline on/off for the active shape, and changing its color/thickness, is immediately visible on the crosshair.
- [ ] Switching Shape to "Custom Image" hides Color/Opacity/Outline and shows an "Import PNG…" button instead; switching back to Cross/Dot/Circle restores them with your previous settings intact.
- [ ] Importing a PNG that is 16x16 or smaller sets it as the crosshair and shows a live preview + dimensions.
- [ ] Importing a PNG larger than 16x16 shows an error stating the actual size and leaves the current crosshair unchanged.
- [ ] Exporting a preset that uses a custom image, then importing it (even on a different machine), reproduces the exact same custom crosshair.
```

- [ ] **Step 2: Self-review the whole feature against the spec**

Re-read `docs/superpowers/specs/2026-08-13-outline-and-custom-png-design.md` top to bottom and confirm each requirement has a corresponding implemented piece:
- Per-shape outline toggle/color/thickness → Tasks 1-3.
- Custom Image as 4th Shape option, single active shape → Tasks 1-3.
- Appearance hidden when Custom Image active → Task 3 (`update_shape_section_visibility`).
- Position/Size/Monitor still apply to Custom Image → unchanged, already shape-agnostic in `overlay_window.c`/`options_window.c`.
- Base64-embedded PNG in the same config/preset JSON → Task 1 (`custom_png_base64` in `config_save`/`config_load`), Task 3 (`on_import_png_clicked` encodes, `on_import_clicked`'s existing generic preset-import path handles it automatically since it's just another config field).
- Max 16x16 validation with error dialog stating actual size → Task 3.
- Nearest-neighbor scaling for crisp pixel-art → Task 2.
- Outline sizing doesn't get clipped by the overlay window bounds → Task 2 (`shape_bounding_size` padding).

- [ ] **Step 3: Commit**

```bash
git add README.md
git commit -m "Add manual verification checklist lines for outline and custom PNG"
```

---

## Self-Review Notes (from writing this plan)

- **Spec coverage:** all sections of the design doc map to a task as listed in Task 4's self-review checklist above.
- **A real bug found and fixed during planning, not left for testing to catch:** the original `options_window_present` called `gtk_widget_show_all(ow->window)` on every open, which would have undone `update_shape_section_visibility`'s hiding of the inapplicable frame each time the Options window was reopened. Fixed by moving the one-time `show_all` into `options_window_new` (called once, immediately followed by re-applying visibility) and making `options_window_present` just show/present the already-configured window.
- **Preserved an existing defensive ordering pattern:** every combo box / spin button / toggle button in the constructor sets its initial value *before* connecting its "changed"/"value-changed"/"toggled" signal handler (matching the original file's existing pattern for `shape_combo`, `monitor_combo`, etc.), so no handler fires against not-yet-created sibling widgets during construction. The new `outline_check`/`outline_color_button`/`outline_thickness_spin`/`shape_combo`'s 4th entry all rely on the final unconditional `refresh_*` calls at the end of `options_window_new` (guarded by `updating_ui`) for their correct initial state instead.
- **Type consistency:** `CrosshairShape`, `CrossSettings`/`DotSettings`/`CircleSettings`'s new `outline_*` fields, and `custom_png_base64` are used with identical names/types across `config.h`, `config.c`, `overlay_window.c`, and `options_window.c`.
