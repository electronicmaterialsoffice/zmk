/*
 * Copyright (c) 2025 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_STUDIO_LOG_LEVEL);

#include <zmk/backlight.h>
#include <zmk/events/backlight_state_changed.h>
#include <zmk/studio/rpc.h>

#include <pb_encode.h>

ZMK_RPC_SUBSYSTEM(backlight)

#define BACKLIGHT_RESPONSE(type, ...) ZMK_RPC_RESPONSE(backlight, type, __VA_ARGS__)

zmk_studio_Response backlight_on(const zmk_studio_Request *req) {
    LOG_DBG("");

    int ret = zmk_backlight_on();
    if (ret != 0) {
        return BACKLIGHT_RESPONSE(backlight_on, false);
    }
    return BACKLIGHT_RESPONSE(backlight_on, true);
}

zmk_studio_Response backlight_off(const zmk_studio_Request *req) {
    LOG_DBG("");

    int ret = zmk_backlight_off();
    if (ret != 0) {
        return BACKLIGHT_RESPONSE(backlight_off, false);
    }
    return BACKLIGHT_RESPONSE(backlight_off, true);
}

zmk_studio_Response backlight_toggle(const zmk_studio_Request *req) {
    LOG_DBG("");

    int ret = zmk_backlight_toggle();
    if (ret != 0) {
        return BACKLIGHT_RESPONSE(backlight_toggle, false);
    }
    return BACKLIGHT_RESPONSE(backlight_toggle, true);
}

zmk_studio_Response backlight_is_on(const zmk_studio_Request *req) {
    LOG_DBG("");

    return BACKLIGHT_RESPONSE(backlight_is_on, zmk_backlight_is_on());
}

zmk_studio_Response backlight_get_brightness(const zmk_studio_Request *req) {
    LOG_DBG("");

    uint8_t brightness = zmk_backlight_get_brt();
    return BACKLIGHT_RESPONSE(backlight_get_brightness, brightness);
}

zmk_studio_Response backlight_set_brightness(const zmk_studio_Request *req) {
    LOG_DBG("");
    const uint8_t brightness =
        (uint8_t)req->subsystem.backlight.request_type.backlight_set_brightness;

    int ret = zmk_backlight_set_brt(brightness);
    if (ret != 0) {
        return BACKLIGHT_RESPONSE(backlight_set_brightness, false);
    }
    return BACKLIGHT_RESPONSE(backlight_set_brightness, true);
}

ZMK_RPC_SUBSYSTEM_HANDLER(backlight, backlight_on, ZMK_STUDIO_RPC_HANDLER_SECURED);
ZMK_RPC_SUBSYSTEM_HANDLER(backlight, backlight_off, ZMK_STUDIO_RPC_HANDLER_SECURED);
ZMK_RPC_SUBSYSTEM_HANDLER(backlight, backlight_toggle, ZMK_STUDIO_RPC_HANDLER_SECURED);
ZMK_RPC_SUBSYSTEM_HANDLER(backlight, backlight_is_on, ZMK_STUDIO_RPC_HANDLER_SECURED);
ZMK_RPC_SUBSYSTEM_HANDLER(backlight, backlight_get_brightness, ZMK_STUDIO_RPC_HANDLER_SECURED);
ZMK_RPC_SUBSYSTEM_HANDLER(backlight, backlight_set_brightness, ZMK_STUDIO_RPC_HANDLER_SECURED);

static int backlight_settings_reset(void) { return zmk_backlight_reset_settings(); }

ZMK_RPC_SUBSYSTEM_SETTINGS_RESET(backlight, backlight_settings_reset);

// NOTE: a mapper must return a negative value for events it does not handle —
// the shared studio_rpc listener treats >= 0 as "mapped" and stops iterating,
// so an unconditional `return 0` here would swallow other subsystems'
// notifications (and emit empty ones) depending on link order.
static int backlight_event_mapper(const zmk_event_t *eh, zmk_studio_Notification *n) {
    struct zmk_backlight_state_changed *state_ev = as_zmk_backlight_state_changed(eh);

    if (!state_ev) {
        return -ENOTSUP;
    }

    zmk_backlight_State state = zmk_backlight_State_init_zero;
    state.on = state_ev->on;
    state.brightness = state_ev->brightness;

    *n = ZMK_RPC_NOTIFICATION(backlight, state_changed, state);
    return 0;
}

ZMK_RPC_EVENT_MAPPER(backlight, backlight_event_mapper, zmk_backlight_state_changed);
