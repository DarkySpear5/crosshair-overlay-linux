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
