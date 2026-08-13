#ifndef OPTIONS_WINDOW_H
#define OPTIONS_WINDOW_H

#include <gtk/gtk.h>
#include "config.h"
#include "overlay_window.h"

typedef struct _OptionsWindow OptionsWindow;

OptionsWindow *options_window_new(CrosshairConfig *cfg, OverlayWindow *overlay, const char *config_path);
void options_window_present(OptionsWindow *ow);
GtkWidget *options_window_get_widget(OptionsWindow *ow);

typedef void (*HotkeyChangedCallback)(gpointer user_data);
void options_window_set_hotkey_changed_callback(OptionsWindow *ow, HotkeyChangedCallback cb, gpointer user_data);

#endif
