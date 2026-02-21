#include <zephyr/device.h>
#include <zephyr/init.h>
#include <zephyr/drivers/display.h>
#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>

#include <lvgl.h>

#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

static enum display_orientation ori_a;
static enum display_orientation ori_b;
static enum display_orientation current_ori;

static enum display_orientation rotate_plus_90(enum display_orientation o) {
    switch (o) {
    case DISPLAY_ORIENTATION_NORMAL:
        return DISPLAY_ORIENTATION_ROTATED_270;
    case DISPLAY_ORIENTATION_ROTATED_90:
        return DISPLAY_ORIENTATION_NORMAL;
    case DISPLAY_ORIENTATION_ROTATED_180:
        return DISPLAY_ORIENTATION_ROTATED_90;
    case DISPLAY_ORIENTATION_ROTATED_270:
        return DISPLAY_ORIENTATION_ROTATED_180;
    default:
        return DISPLAY_ORIENTATION_NORMAL;
    }
}

static lv_disp_rot_t to_lvgl_rot(enum display_orientation o) {
    switch (o) {
    case DISPLAY_ORIENTATION_NORMAL:
        return LV_DISP_ROT_NONE;
    case DISPLAY_ORIENTATION_ROTATED_90:
        return LV_DISP_ROT_90;
    case DISPLAY_ORIENTATION_ROTATED_180:
        return LV_DISP_ROT_180;
    case DISPLAY_ORIENTATION_ROTATED_270:
        return LV_DISP_ROT_270;
    default:
        return LV_DISP_ROT_NONE;
    }
}

static void lvgl_full_refresh(enum display_orientation o) {
    lv_disp_t *d = lv_disp_get_default();
    if (!d) {
        return;
    }

    /* Tell LVGL the new rotation */
    lv_disp_set_rotation(d, to_lvgl_rot(o));

    /* Force full redraw */
    lv_obj_invalidate(lv_scr_act());
    lv_refr_now(d);
}

static int apply_orientation(enum display_orientation o) {
    const struct device *display = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
    if (!device_is_ready(display)) {
        LOG_ERR("Display device not ready");
        return -EIO;
    }

    /*
     * Some panels behave better if you blank around orientation changes.
     * If this causes issues on your setup, you can remove these two calls.
     */
    (void)display_blanking_on(display);

    int ret = display_set_orientation(display, o);

    (void)display_blanking_off(display);

    if (ret == 0) {
        current_ori = o;
        LOG_INF("Display orientation set to %d", (int)o);

        /* Prevent corruption: update LVGL and redraw everything */
        lvgl_full_refresh(o);
    } else {
        LOG_WRN("display_set_orientation(%d) failed: %d", (int)o, ret);
    }

    return ret;
}

static enum display_orientation boot_base_orientation(void) {
#ifdef CONFIG_DONGLE_SCREEN_HORIZONTAL
#  ifdef CONFIG_DONGLE_SCREEN_FLIPPED
    return DISPLAY_ORIENTATION_ROTATED_90;
#  else
    return DISPLAY_ORIENTATION_ROTATED_270;
#  endif
#else
#  ifdef CONFIG_DONGLE_SCREEN_FLIPPED
    return DISPLAY_ORIENTATION_NORMAL;
#  else
    return DISPLAY_ORIENTATION_ROTATED_180;
#  endif
#endif
}

static int disp_set_orientation_init(void) {
    ori_a = boot_base_orientation();
    ori_b = rotate_minus_90(ori_a);

    /* Start in A */
    return apply_orientation(ori_a);
}

SYS_INIT(disp_set_orientation_init, APPLICATION, 60);

#if IS_ENABLED(CONFIG_DONGLE_SCREEN_ROTATE_TOGGLE)

static int rotate_key_listener(const zmk_event_t *eh) {
    const struct zmk_keycode_state_changed *ev = as_zmk_keycode_state_changed(eh);
    if (!ev) {
        return 0;
    }

    /* Only act on key-down */
    if (!ev->state) {
        return 0;
    }

    if (ev->keycode == CONFIG_DONGLE_SCREEN_ROTATE_TOGGLE_KEYCODE) {
        enum display_orientation next = (current_ori == ori_a) ? ori_b : ori_a;
        (void)apply_orientation(next);
    }

    return 0;
}

ZMK_LISTENER(dongle_screen_rotate, rotate_key_listener);
ZMK_SUBSCRIPTION(dongle_screen_rotate, zmk_keycode_state_changed);

#endif /* CONFIG_DONGLE_SCREEN_ROTATE_TOGGLE */