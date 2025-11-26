/*
 * Copyright (c) 2025 Electronic Materials Office Ltd.
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_STUDIO_LOG_LEVEL);

#include <altar_ii/peripherals/altar_ii_haptics.h>
#include <zmk/studio/rpc.h>

#include <pb_encode.h>

ZMK_RPC_SUBSYSTEM(haptics)

#define HAPTICS_RESPONSE(type, ...) ZMK_RPC_RESPONSE(haptics, type, __VA_ARGS__)

zmk_studio_Response haptics_on(const zmk_studio_Request *req) {
    LOG_DBG("");

    int ret = altar_ii_haptics_on();
    if (ret != 0) {
        return HAPTICS_RESPONSE(haptics_on, false);
    }
    return HAPTICS_RESPONSE(haptics_on, true);
}

zmk_studio_Response haptics_off(const zmk_studio_Request *req) {
    LOG_DBG("");

    int ret = altar_ii_haptics_off();
    if (ret != 0) {
        return HAPTICS_RESPONSE(haptics_off, false);
    }
    return HAPTICS_RESPONSE(haptics_off, true);
}

zmk_studio_Response haptics_toggle(const zmk_studio_Request *req) {
    LOG_DBG("");

    int ret = altar_ii_haptics_toggle();
    if (ret != 0) {
        return HAPTICS_RESPONSE(haptics_toggle, false);
    }
    return HAPTICS_RESPONSE(haptics_toggle, true);
}

zmk_studio_Response haptics_is_on(const zmk_studio_Request *req) {
    LOG_DBG("");

    return HAPTICS_RESPONSE(haptics_is_on, altar_ii_haptics_is_on());
}

zmk_studio_Response haptics_get_rated_voltage(const zmk_studio_Request *req) {
    LOG_DBG("");

    uint8_t rated_voltage = altar_ii_haptics_get_rated_voltage();
    return HAPTICS_RESPONSE(haptics_get_rated_voltage, rated_voltage);
}

zmk_studio_Response haptics_set_rated_voltage(const zmk_studio_Request *req) {
    LOG_DBG("");
    const uint8_t rated_voltage =
        (uint8_t)req->subsystem.haptics.request_type.haptics_set_rated_voltage;

    int ret = altar_ii_haptics_set_rated_voltage(rated_voltage);
    if (ret != 0) {
        return HAPTICS_RESPONSE(haptics_set_rated_voltage, false);
    }
    return HAPTICS_RESPONSE(haptics_set_rated_voltage, true);
}

ZMK_RPC_SUBSYSTEM_HANDLER(haptics, haptics_on, ZMK_STUDIO_RPC_HANDLER_SECURED);
ZMK_RPC_SUBSYSTEM_HANDLER(haptics, haptics_off, ZMK_STUDIO_RPC_HANDLER_SECURED);
ZMK_RPC_SUBSYSTEM_HANDLER(haptics, haptics_toggle, ZMK_STUDIO_RPC_HANDLER_SECURED);
ZMK_RPC_SUBSYSTEM_HANDLER(haptics, haptics_is_on, ZMK_STUDIO_RPC_HANDLER_SECURED);
ZMK_RPC_SUBSYSTEM_HANDLER(haptics, haptics_get_rated_voltage, ZMK_STUDIO_RPC_HANDLER_SECURED);
ZMK_RPC_SUBSYSTEM_HANDLER(haptics, haptics_set_rated_voltage, ZMK_STUDIO_RPC_HANDLER_SECURED);

static int haptics_event_mapper(const zmk_event_t *eh, zmk_studio_Notification *n) { return 0; }

ZMK_RPC_EVENT_MAPPER(haptics, haptics_event_mapper);