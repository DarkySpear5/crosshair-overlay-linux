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
