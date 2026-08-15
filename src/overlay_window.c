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
        case SHAPE_CROSS: {
            double outline_extra = cfg->cross.outline_enabled ? cfg->cross.outline_thickness * scale : 0;
            half = (cfg->cross.length + cfg->cross.gap) * scale + outline_extra;
            if (cfg->cross.outline_enabled) pad += cfg->cross.outline_thickness * scale * 2.0;
            break;
        }
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

/* inner/outer are distances from center to the near and far end of each arm. */
static void build_cross_path(cairo_t *cr, double cx, double cy, double inner, double outer) {
    cairo_move_to(cr, cx - outer, cy);
    cairo_line_to(cr, cx - inner, cy);
    cairo_move_to(cr, cx + inner, cy);
    cairo_line_to(cr, cx + outer, cy);
    cairo_move_to(cr, cx, cy - outer);
    cairo_line_to(cr, cx, cy - inner);
    cairo_move_to(cr, cx, cy + inner);
    cairo_line_to(cr, cx, cy + outer);
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
                double outline_extra = ow->cfg.cross.outline_thickness * scale;
                double outline_thick = thick + 2.0 * outline_extra;
                double inner = gap - outline_extra;
                if (inner < 0) inner = 0;
                double outer = gap + len + outline_extra;
                cairo_set_source_rgba(cr, ow->cfg.cross.outline_r, ow->cfg.cross.outline_g,
                                      ow->cfg.cross.outline_b, ow->cfg.cross.opacity);
                cairo_set_line_width(cr, outline_thick);
                build_cross_path(cr, cx, cy, inner, outer);
                cairo_stroke(cr);
            }

            cairo_set_source_rgba(cr, ow->cfg.cross.r, ow->cfg.cross.g, ow->cfg.cross.b, ow->cfg.cross.opacity);
            cairo_set_line_width(cr, thick);
            build_cross_path(cr, cx, cy, gap, gap + len);
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
