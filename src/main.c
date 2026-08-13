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
