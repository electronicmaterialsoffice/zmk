/*
 * Copyright (c) 2025 Electronic Materials Office Ltd.
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_STUDIO_LOG_LEVEL);

#include <altar_ii/peripherals/altar_ii_als.h>
#include <altar_ii/peripherals/altar_ii_audio.h>
#include <altar_ii/peripherals/altar_ii_haptics.h>
#include <altar_ii/peripherals/altar_ii_indicators.h>
#include <zmk/studio/rpc.h>

#include <pb_encode.h>

ZMK_RPC_SUBSYSTEM(altar_ii)

#define ALTAR_II_RESPONSE(type, ...) ZMK_RPC_RESPONSE(altar_ii, type, __VA_ARGS__)

zmk_studio_Response als_get_state(const zmk_studio_Request *req) {
    LOG_DBG("");

#if CONFIG_ALTAR_II_ALS_BACKLIGHT
    return ALTAR_II_RESPONSE(als_get_state, altar_ii_als_get_state());
#else
    return ZMK_RPC_SIMPLE_ERR(GENERIC);
#endif // CONFIG_ALTAR_II_ALS_BACKLIGHT
}

zmk_studio_Response als_set_state(const zmk_studio_Request *req) {
    LOG_DBG("");

#if CONFIG_ALTAR_II_ALS_BACKLIGHT
    const enum altar_ii_als_state als_state = req->subsystem.altar_ii.request_type.als_set_state;
    altar_ii_als_set_state(als_state);
    return ALTAR_II_RESPONSE(als_set_state, true);
#else
    return ZMK_RPC_SIMPLE_ERR(GENERIC);
#endif // CONFIG_ALTAR_II_ALS_BACKLIGHT
}

zmk_studio_Response play_indicator(const zmk_studio_Request *req) {
    LOG_DBG("");

    const enum altar_ii_indicator indicator = req->subsystem.altar_ii.request_type.play_indicator;
    int ret = 0;
#if CONFIG_ALTAR_II_AUDIO
    ret = altar_ii_audio_play_indicator(indicator);
    if (ret < 0) {
        return ALTAR_II_RESPONSE(play_indicator, false);
    }
#endif // CONFIG_ALTAR_II_AUDIO
#if CONFIG_ALTAR_II_HAPTICS
    ret = altar_ii_haptics_play_indicator(indicator);
    if (ret < 0) {
        return ALTAR_II_RESPONSE(play_indicator, false);
    }
#endif // CONFIG_ALTAR_II_HAPTICS

    return ALTAR_II_RESPONSE(play_indicator, true);
}

ZMK_RPC_SUBSYSTEM_HANDLER(altar_ii, als_get_state, ZMK_STUDIO_RPC_HANDLER_SECURED);
ZMK_RPC_SUBSYSTEM_HANDLER(altar_ii, als_set_state, ZMK_STUDIO_RPC_HANDLER_SECURED);
ZMK_RPC_SUBSYSTEM_HANDLER(altar_ii, play_indicator, ZMK_STUDIO_RPC_HANDLER_SECURED);

static int altar_ii_event_mapper(const zmk_event_t *eh, zmk_studio_Notification *n) { return 0; }

ZMK_RPC_EVENT_MAPPER(altar_ii, altar_ii_event_mapper);