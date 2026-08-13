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
