#include <gtk/gtk.h>
#include "config.h"
#include "overlay_window.h"
#include "tray.h"
#include "options_window.h"
#include "hotkey.h"

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
    options_window_sync_enabled(app->options, app->cfg.enabled);
    save_current_config(app);
}

static void on_enabled_changed_from_options(gpointer user_data) {
    AppState *app = (AppState *)user_data;
    overlay_window_apply_config(app->overlay, &app->cfg);
    tray_icon_set_enabled(app->tray, app->cfg.enabled);
}

static void on_options(gpointer user_data) {
    AppState *app = (AppState *)user_data;
    options_window_present(app->options);
}

static void on_quit(gpointer user_data) {
    (void)user_data;
    gtk_main_quit();
}

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
    options_window_set_hotkey_changed_callback(app.options, on_hotkey_changed, &app);
    options_window_set_enabled_changed_callback(app.options, on_enabled_changed_from_options, &app);

    app.tray = tray_icon_new(app.cfg.enabled, on_toggle, on_options, on_quit, &app);

    if (hotkey_init()) {
        regrab_hotkey(&app);
    } else {
        g_warning("Global hotkey unavailable (not running under X11)");
    }

    gtk_main();

    hotkey_shutdown();
    config_free_contents(&app.cfg);
    g_free(app.config_path);
    return 0;
}
