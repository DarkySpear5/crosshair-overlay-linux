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
