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
