#ifndef TRAY_H
#define TRAY_H

#include <gtk/gtk.h>

typedef struct _TrayIcon TrayIcon;
typedef void (*TrayCallback)(gpointer user_data);

TrayIcon *tray_icon_new(gboolean initial_enabled, TrayCallback on_toggle, TrayCallback on_options,
                         TrayCallback on_quit, gpointer user_data);
void tray_icon_set_enabled(TrayIcon *tray, gboolean enabled);

#endif
