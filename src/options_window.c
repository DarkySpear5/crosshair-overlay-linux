#include "options_window.h"
#include "hotkey.h"
#include <json-glib/json-glib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#define MAX_CUSTOM_PNG_SIZE 64

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
        double preview_scale = 64.0 / MAX(w, h);
        int pw = (int)(w * preview_scale + 0.5);
        int ph = (int)(h * preview_scale + 0.5);
        if (pw < 1) pw = 1;
        if (ph < 1) ph = 1;
        GdkPixbuf *scaled = gdk_pixbuf_scale_simple(pixbuf, pw, ph, GDK_INTERP_NEAREST);
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

    GtkWidget *import_png_button = gtk_button_new_with_label("Import PNG… (max 64x64)");
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
