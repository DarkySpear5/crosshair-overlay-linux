#ifndef HOTKEY_H
#define HOTKEY_H

#include <glib.h>

typedef void (*HotkeyToggleCallback)(gpointer user_data);

gboolean hotkey_init(void);
void hotkey_shutdown(void);
gboolean hotkey_grab(char * const *keys, int count, HotkeyToggleCallback cb, gpointer user_data, GError **error);
void hotkey_ungrab(void);

#endif
