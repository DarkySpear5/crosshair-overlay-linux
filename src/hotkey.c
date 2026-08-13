#include "hotkey.h"
#include <gdk/gdkx.h>
#include <gdk/gdk.h>
#include <X11/Xlib.h>
#include <string.h>

static Display *xdisplay = NULL;
static Window root = 0;
static KeyCode grabbed_keycode = 0;
static unsigned int grabbed_modmask = 0;
static gboolean have_grab = FALSE;
static HotkeyToggleCallback active_callback = NULL;
static gpointer active_user_data = NULL;

/* X11 reports events with the currently-active lock modifiers (NumLock, CapsLock)
 * mixed into the state, so a single XGrabKey for our intended mask would silently
 * stop matching the moment NumLock or CapsLock is toggled. We grab all 4
 * combinations of {no lock, NumLock, CapsLock, both} to make the hotkey reliable
 * regardless of lock-key state, and ignore those bits when comparing incoming events. */
#define LOCK_IGNORE_MASKS_COUNT 4

static unsigned int name_to_modmask(const char *name) {
    if (g_ascii_strcasecmp(name, "Ctrl") == 0 || g_ascii_strcasecmp(name, "Control") == 0) return ControlMask;
    if (g_ascii_strcasecmp(name, "Alt") == 0) return Mod1Mask;
    if (g_ascii_strcasecmp(name, "Shift") == 0) return ShiftMask;
    if (g_ascii_strcasecmp(name, "Super") == 0 || g_ascii_strcasecmp(name, "Meta") == 0) return Mod4Mask;
    return 0;
}

static gboolean is_modifier_name(const char *name) {
    return name_to_modmask(name) != 0;
}

static GdkFilterReturn event_filter(GdkXEvent *xevent, GdkEvent *event, gpointer user_data) {
    (void)event;
    (void)user_data;
    XEvent *xev = (XEvent *)xevent;
    if (xev->type != KeyPress) return GDK_FILTER_CONTINUE;
    if (!have_grab) return GDK_FILTER_CONTINUE;

    unsigned int ignore = LockMask | Mod2Mask;
    unsigned int state = xev->xkey.state & ~ignore;

    if (xev->xkey.keycode == grabbed_keycode && state == grabbed_modmask) {
        if (active_callback) active_callback(active_user_data);
    }
    return GDK_FILTER_CONTINUE;
}

gboolean hotkey_init(void) {
    GdkDisplay *gdk_display = gdk_display_get_default();
    if (!gdk_display || !GDK_IS_X11_DISPLAY(gdk_display)) {
        return FALSE;
    }
    xdisplay = GDK_DISPLAY_XDISPLAY(gdk_display);
    root = DefaultRootWindow(xdisplay);

    GdkWindow *root_gdk = gdk_get_default_root_window();
    gdk_window_add_filter(root_gdk, event_filter, NULL);
    gdk_window_set_events(root_gdk, gdk_window_get_events(root_gdk) | GDK_KEY_PRESS_MASK);

    return TRUE;
}

void hotkey_ungrab(void) {
    if (have_grab && xdisplay) {
        unsigned int ignore_masks[LOCK_IGNORE_MASKS_COUNT] = { 0, LockMask, Mod2Mask, LockMask | Mod2Mask };
        for (int i = 0; i < LOCK_IGNORE_MASKS_COUNT; i++) {
            XUngrabKey(xdisplay, grabbed_keycode, grabbed_modmask | ignore_masks[i], root);
        }
        have_grab = FALSE;
    }
}

gboolean hotkey_grab(char * const *keys, int count, HotkeyToggleCallback cb, gpointer user_data, GError **error) {
    if (!xdisplay) {
        g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "hotkey_init() was not called or no X11 display available");
        return FALSE;
    }
    if (count < 2) {
        g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "Hotkey needs at least 2 keys");
        return FALSE;
    }

    unsigned int modmask = 0;
    const char *trigger_name = NULL;
    for (int i = 0; i < count; i++) {
        if (is_modifier_name(keys[i])) {
            modmask |= name_to_modmask(keys[i]);
        } else if (trigger_name == NULL) {
            trigger_name = keys[i];
        } else {
            g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "Only one non-modifier trigger key is allowed");
            return FALSE;
        }
    }
    if (!trigger_name) {
        g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "Hotkey needs exactly one non-modifier trigger key");
        return FALSE;
    }

    KeySym keysym = XStringToKeysym(trigger_name);
    if (keysym == NoSymbol) {
        g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "Unrecognized key name: %s", trigger_name);
        return FALSE;
    }
    KeyCode keycode = XKeysymToKeycode(xdisplay, keysym);
    if (keycode == 0) {
        g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "No keycode for key: %s", trigger_name);
        return FALSE;
    }

    hotkey_ungrab();

    gdk_x11_display_error_trap_push(gdk_display_get_default());

    unsigned int ignore_masks[LOCK_IGNORE_MASKS_COUNT] = { 0, LockMask, Mod2Mask, LockMask | Mod2Mask };
    for (int i = 0; i < LOCK_IGNORE_MASKS_COUNT; i++) {
        XGrabKey(xdisplay, keycode, modmask | ignore_masks[i], root, True, GrabModeAsync, GrabModeAsync);
    }
    XSync(xdisplay, False);

    gint xerror = gdk_x11_display_error_trap_pop(gdk_display_get_default());
    if (xerror != 0) {
        g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "Failed to grab hotkey (X error %d) - it may already be in use by another application", xerror);
        return FALSE;
    }

    grabbed_keycode = keycode;
    grabbed_modmask = modmask;
    have_grab = TRUE;
    active_callback = cb;
    active_user_data = user_data;

    return TRUE;
}

void hotkey_shutdown(void) {
    hotkey_ungrab();
    active_callback = NULL;
    active_user_data = NULL;
}
