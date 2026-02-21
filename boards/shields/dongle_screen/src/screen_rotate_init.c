#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/logging/log.h>

#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

static enum display_orientation ori_a;
static enum display_orientation ori_b;
static enum display_orientation current_ori;

static enum display_orientation rotate_plus_90(enum display_orientation o) {
    switch (o) {
    case DISPLAY_ORIENTATION_NORMAL:      return DISPLAY_ORIENTATION_ROTATED_90;
    case DISPLAY_ORIENTATION_ROTATED_90:  return DISPLAY_ORIENTATION_ROTATED_180;
    case DISPLAY_ORIENTATION_ROTATED_180: return DISPLAY_ORIENTATION_ROTATED_270;
    case DISPLAY_ORIENTATION_ROTATED_270: return DISPLAY_ORIENTATION_NORMAL;
    default: return DISPLAY_ORIENTATION_NORMAL;
    }
}

static int apply_orientation(enum display_orientation o) {
    const struct device *display = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
    if (!device_is_ready(display)) {
        return -EIO;
    }
    int ret = display_set_orientation(display, o);
    if (ret == 0) {
        current_ori = o;
        LOG_INF("Display orientation set to %d", (int)o);
    }
    return ret;
}

int disp_set_orientation(void)
{
	// Set the orientation
	const struct device *display = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
	if (!device_is_ready(display))
	{
		return -EIO;
	}

#ifdef CONFIG_DONGLE_SCREEN_HORIZONTAL
#ifdef CONFIG_DONGLE_SCREEN_FLIPPED
	int ret = display_set_orientation(display, DISPLAY_ORIENTATION_ROTATED_90);
#else
	int ret = display_set_orientation(display, DISPLAY_ORIENTATION_ROTATED_270);
#endif
#else
#ifdef CONFIG_DONGLE_SCREEN_FLIPPED
	int ret = display_set_orientation(display, DISPLAY_ORIENTATION_NORMAL);
#else
	int ret = display_set_orientation(display, DISPLAY_ORIENTATION_ROTATED_180);
#endif
#endif

	if (ret < 0)
	{
		return ret;
	}

	return 0;
}

SYS_INIT(disp_set_orientation, APPLICATION, 60);

#if IS_ENABLED(CONFIG_DONGLE_SCREEN_ROTATE_TOGGLE)
static int rotate_key_listener(const zmk_event_t *eh) {
    const struct zmk_keycode_state_changed *ev = as_zmk_keycode_state_changed(eh);
    if (!ev || !ev->state) {
        return 0; // key upは無視
    }

    if (ev->keycode == CONFIG_DONGLE_SCREEN_ROTATE_TOGGLE_KEYCODE) {
        enum display_orientation next = (current_ori == ori_a) ? ori_b : ori_a;
        (void)apply_orientation(next);
        return 0;
    }
    return 0;
}

ZMK_LISTENER(dongle_screen_rotate, rotate_key_listener);
ZMK_SUBSCRIPTION(dongle_screen_rotate, zmk_keycode_state_changed);
#endif
