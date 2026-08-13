#ifndef OVERLAY_WINDOW_H
#define OVERLAY_WINDOW_H

#include <gtk/gtk.h>
#include "config.h"

typedef struct _OverlayWindow OverlayWindow;

OverlayWindow *overlay_window_new(void);
void overlay_window_apply_config(OverlayWindow *ow, const CrosshairConfig *cfg);
void overlay_window_set_visible(OverlayWindow *ow, gboolean visible);
GtkWidget *overlay_window_get_widget(OverlayWindow *ow);

#endif
