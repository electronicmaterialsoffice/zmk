/*
 * Copyright (c) 2026 Electronic Materials Office Limited
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_STUDIO_LOG_LEVEL);

#include <zmk/magdial.h>
#include <zmk/studio/rpc.h>

#include <pb_encode.h>

ZMK_RPC_SUBSYSTEM(magdial)

#define MAGDIAL_RESPONSE(type, ...) ZMK_RPC_RESPONSE(magdial, type, __VA_ARGS__)

static void slot_to_pb(const struct zmk_magdial_slot_binding *slot,
                       zmk_keymap_BehaviorBinding *out) {
    out->behavior_id = slot->local_id;
    out->param1 = slot->param1;
    out->param2 = slot->param2;
}

static struct zmk_magdial_slot_binding slot_from_pb(const zmk_keymap_BehaviorBinding *in) {
    return (struct zmk_magdial_slot_binding){
        .local_id = (zmk_behavior_local_id_t)in->behavior_id,
        .param1 = in->param1,
        .param2 = in->param2,
    };
}

// A slot is valid when empty (id 0) or when its id resolves to a behavior.
// Params are the client's responsibility in v1 (the app curates them);
// unknown ids are rejected so a stale client can never store a mis-bind.
static bool slot_is_valid(const struct zmk_magdial_slot_binding *slot) {
    if (slot->local_id == 0) {
        return true;
    }

    return zmk_behavior_find_behavior_name_from_local_id(slot->local_id) != NULL;
}

static zmk_studio_Response get_config(const zmk_studio_Request *req) {
    LOG_DBG("");

    struct zmk_magdial_slot_binding slots[ZMK_MAGDIAL_SLOT_COUNT];
    bool press_enabled;
    uint8_t sensitivity;

    zmk_magdial_get_config(slots, &press_enabled, &sensitivity);

    zmk_magdial_GetConfigResponse resp = zmk_magdial_GetConfigResponse_init_zero;
    resp.has_config = true;
    resp.config.has_cw_action = true;
    slot_to_pb(&slots[ZMK_MAGDIAL_SLOT_CW], &resp.config.cw_action);
    resp.config.has_ccw_action = true;
    slot_to_pb(&slots[ZMK_MAGDIAL_SLOT_CCW], &resp.config.ccw_action);
    resp.config.press_enabled = press_enabled;
    resp.config.has_single_press = true;
    slot_to_pb(&slots[ZMK_MAGDIAL_SLOT_SINGLE], &resp.config.single_press);
    resp.config.has_double_press = true;
    slot_to_pb(&slots[ZMK_MAGDIAL_SLOT_DOUBLE], &resp.config.double_press);
    resp.config.has_triple_press = true;
    slot_to_pb(&slots[ZMK_MAGDIAL_SLOT_TRIPLE], &resp.config.triple_press);
    resp.config.has_long_press = true;
    slot_to_pb(&slots[ZMK_MAGDIAL_SLOT_LONG], &resp.config.long_press);
    resp.config.sensitivity = sensitivity;

    return MAGDIAL_RESPONSE(get_config, resp);
}

static zmk_studio_Response set_config(const zmk_studio_Request *req) {
    LOG_DBG("");

    const zmk_magdial_SetConfigRequest *sc = &req->subsystem.magdial.request_type.set_config;

    zmk_magdial_SetConfigResponse resp = zmk_magdial_SetConfigResponse_init_zero;

    if (!sc->has_config || sc->config.sensitivity > ZMK_MAGDIAL_SENSITIVITY_MAX) {
        resp.result = zmk_magdial_SetConfigResponse_Result_RESULT_INVALID;
        return MAGDIAL_RESPONSE(set_config, resp);
    }

    // An absent submessage decodes as a zeroed binding == empty slot.
    struct zmk_magdial_slot_binding slots[ZMK_MAGDIAL_SLOT_COUNT] = {
        [ZMK_MAGDIAL_SLOT_CW] = slot_from_pb(&sc->config.cw_action),
        [ZMK_MAGDIAL_SLOT_CCW] = slot_from_pb(&sc->config.ccw_action),
        [ZMK_MAGDIAL_SLOT_SINGLE] = slot_from_pb(&sc->config.single_press),
        [ZMK_MAGDIAL_SLOT_DOUBLE] = slot_from_pb(&sc->config.double_press),
        [ZMK_MAGDIAL_SLOT_TRIPLE] = slot_from_pb(&sc->config.triple_press),
        [ZMK_MAGDIAL_SLOT_LONG] = slot_from_pb(&sc->config.long_press),
    };

    // Reject the whole set on any invalid slot — never partial-apply, so the
    // persisted blob stays internally consistent.
    for (int i = 0; i < ZMK_MAGDIAL_SLOT_COUNT; i++) {
        if (!slot_is_valid(&slots[i])) {
            LOG_WRN("magdial set_config rejected: slot %d has unknown behavior id %d", i,
                    slots[i].local_id);
            resp.result = zmk_magdial_SetConfigResponse_Result_RESULT_INVALID;
            return MAGDIAL_RESPONSE(set_config, resp);
        }
    }

    int ret = zmk_magdial_set_config(slots, sc->config.press_enabled,
                                     (uint8_t)sc->config.sensitivity);
    if (ret < 0) {
        LOG_ERR("magdial: failed to persist config (err %d)", ret);
        return ZMK_RPC_SIMPLE_ERR(GENERIC);
    }

    resp.result = zmk_magdial_SetConfigResponse_Result_RESULT_OK;
    return MAGDIAL_RESPONSE(set_config, resp);
}

ZMK_RPC_SUBSYSTEM_HANDLER(magdial, get_config, ZMK_STUDIO_RPC_HANDLER_SECURED);
ZMK_RPC_SUBSYSTEM_HANDLER(magdial, set_config, ZMK_STUDIO_RPC_HANDLER_SECURED);

static int magdial_settings_reset(void) { return zmk_magdial_reset_settings(); }

ZMK_RPC_SUBSYSTEM_SETTINGS_RESET(magdial, magdial_settings_reset);

static int magdial_event_mapper(const zmk_event_t *eh, zmk_studio_Notification *n) {
    return -ENOTSUP;
}

ZMK_RPC_EVENT_MAPPER(magdial, magdial_event_mapper);
