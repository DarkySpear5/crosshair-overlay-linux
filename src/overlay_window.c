#include "overlay_window.h"
#include <gdk/gdkx.h>
#include <X11/extensions/shape.h>
#include <math.h>

struct _OverlayWindow {
    GtkWidget *window;
    CrosshairConfig cfg;
    gboolean shaped_once;
};

static int shape_bounding_size(const CrosshairConfig *cfg) {
    double scale = cfg->size_percent / 100.0;
    double half;
    switch (cfg->shape) {
        case SHAPE_CROSS:
            half = (cfg->cross.length + cfg->cross.gap) * scale;
            break;
        case SHAPE_DOT:
            half = cfg->dot.radius * scale;
            break;
        case SHAPE_CIRCLE:
            half = (cfg->circle.radius + cfg->circle.thickness) * scale;
            break;
        default:
            half = 10;
    }
    int size = (int)ceil(half * 2.0) + 4;
    return size < 8 ? 8 : size;
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
            cairo_set_source_rgba(cr, ow->cfg.cross.r, ow->cfg.cross.g, ow->cfg.cross.b, ow->cfg.cross.opacity);
            cairo_set_line_width(cr, thick);
            cairo_set_line_cap(cr, CAIRO_LINE_CAP_BUTT);

            cairo_move_to(cr, cx - gap - len, cy);
            cairo_line_to(cr, cx - gap, cy);
            cairo_move_to(cr, cx + gap, cy);
            cairo_line_to(cr, cx + gap + len, cy);
            cairo_move_to(cr, cx, cy - gap - len);
            cairo_line_to(cr, cx, cy - gap);
            cairo_move_to(cr, cx, cy + gap);
            cairo_line_to(cr, cx, cy + gap + len);
            cairo_stroke(cr);
            break;
        }
        case SHAPE_DOT: {
            double r = ow->cfg.dot.radius * scale;
            cairo_set_source_rgba(cr, ow->cfg.dot.r, ow->cfg.dot.g, ow->cfg.dot.b, ow->cfg.dot.opacity);
            cairo_arc(cr, cx, cy, r, 0, 2 * G_PI);
            cairo_fill(cr);
            break;
        }
        case SHAPE_CIRCLE: {
            double r = ow->cfg.circle.radius * scale;
            double thick = ow->cfg.circle.thickness * scale;
            cairo_set_source_rgba(cr, ow->cfg.circle.r, ow->cfg.circle.g, ow->cfg.circle.b, ow->cfg.circle.opacity);
            cairo_set_line_width(cr, thick);
            cairo_arc(cr, cx, cy, r, 0, 2 * G_PI);
            cairo_stroke(cr);
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
    int size = shape_bounding_size(&ow->cfg);
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
