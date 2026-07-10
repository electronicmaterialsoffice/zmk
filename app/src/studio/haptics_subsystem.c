/*
 * Copyright (c) 2025 Electronic Materials Office Limited
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

    uint8_t rated_voltage = 0;
    int ret = altar_ii_haptics_get_rated_voltage(&rated_voltage);
    if (ret != 0) {
        return ZMK_RPC_SIMPLE_ERR(GENERIC);
    }
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

static zmk_studio_Response get_config(const zmk_studio_Request *req) {
    LOG_DBG("");

    struct altar_ii_haptics_config cfg;
    altar_ii_haptics_get_config(&cfg);

    zmk_haptics_GetConfigResponse resp = zmk_haptics_GetConfigResponse_init_zero;
    resp.has_config = true;
    resp.config.on = cfg.on;
    resp.config.rated_voltage = cfg.rated_voltage;
    resp.config.event_mask = cfg.event_mask;

    return HAPTICS_RESPONSE(get_config, resp);
}

static zmk_studio_Response set_config(const zmk_studio_Request *req) {
    LOG_DBG("");

    const zmk_haptics_SetConfigRequest *sc = &req->subsystem.haptics.request_type.set_config;

    zmk_haptics_SetConfigResponse resp = zmk_haptics_SetConfigResponse_init_zero;

    if (!sc->has_config || sc->config.rated_voltage > ALTAR_II_HAPTICS_RATED_VOLTAGE_MAX) {
        resp.result = zmk_haptics_SetConfigResponse_Result_RESULT_INVALID;
        return HAPTICS_RESPONSE(set_config, resp);
    }

    struct altar_ii_haptics_config cfg = {
        .on = sc->config.on,
        .rated_voltage = (uint8_t)sc->config.rated_voltage,
        .event_mask = sc->config.event_mask,
    };

    int ret = altar_ii_haptics_set_config(&cfg);
    if (ret < 0) {
        LOG_ERR("haptics: failed to apply config (err %d)", ret);
        return ZMK_RPC_SIMPLE_ERR(GENERIC);
    }

    resp.result = zmk_haptics_SetConfigResponse_Result_RESULT_OK;
    return HAPTICS_RESPONSE(set_config, resp);
}

ZMK_RPC_SUBSYSTEM_HANDLER(haptics, haptics_on, ZMK_STUDIO_RPC_HANDLER_SECURED);
ZMK_RPC_SUBSYSTEM_HANDLER(haptics, haptics_off, ZMK_STUDIO_RPC_HANDLER_SECURED);
ZMK_RPC_SUBSYSTEM_HANDLER(haptics, haptics_toggle, ZMK_STUDIO_RPC_HANDLER_SECURED);
ZMK_RPC_SUBSYSTEM_HANDLER(haptics, haptics_is_on, ZMK_STUDIO_RPC_HANDLER_SECURED);
ZMK_RPC_SUBSYSTEM_HANDLER(haptics, haptics_get_rated_voltage, ZMK_STUDIO_RPC_HANDLER_SECURED);
ZMK_RPC_SUBSYSTEM_HANDLER(haptics, haptics_set_rated_voltage, ZMK_STUDIO_RPC_HANDLER_SECURED);
ZMK_RPC_SUBSYSTEM_HANDLER(haptics, get_config, ZMK_STUDIO_RPC_HANDLER_SECURED);
ZMK_RPC_SUBSYSTEM_HANDLER(haptics, set_config, ZMK_STUDIO_RPC_HANDLER_SECURED);

static int haptics_settings_reset(void) { return altar_ii_haptics_reset_settings(); }

ZMK_RPC_SUBSYSTEM_SETTINGS_RESET(haptics, haptics_settings_reset);

// Haptics has no notifications (nothing device-side mutates its config), so
// the mapper declines every event — returning 0 here would tell the shared
// studio_rpc listener the event was mapped and swallow other subsystems'
// notifications.
static int haptics_event_mapper(const zmk_event_t *eh, zmk_studio_Notification *n) {
    return -ENOTSUP;
}

ZMK_RPC_EVENT_MAPPER(haptics, haptics_event_mapper);