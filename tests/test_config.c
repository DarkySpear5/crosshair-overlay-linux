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
