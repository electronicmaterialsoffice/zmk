/*
 * Copyright (c) 2026 Electronic Materials Office Limited
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_STUDIO_LOG_LEVEL);

#include <altar_ii/events/altar_ii_audio_state_changed.h>
#include <altar_ii/peripherals/audio/altar_ii_audio.h>
#include <zmk/studio/rpc.h>

#include <pb_encode.h>

ZMK_RPC_SUBSYSTEM(audio)

#define AUDIO_RESPONSE(type, ...) ZMK_RPC_RESPONSE(audio, type, __VA_ARGS__)

static zmk_studio_Response get_config(const zmk_studio_Request *req) {
    LOG_DBG("");

    struct altar_ii_audio_config cfg;
    altar_ii_audio_get_config(&cfg);

    zmk_audio_GetConfigResponse resp = zmk_audio_GetConfigResponse_init_zero;
    resp.has_config = true;
    resp.config.on = cfg.on;
    resp.config.volume_attenuation = cfg.volume_attenuation;
    resp.config.event_mask = cfg.event_mask;

    return AUDIO_RESPONSE(get_config, resp);
}

static zmk_studio_Response set_config(const zmk_studio_Request *req) {
    LOG_DBG("");

    const zmk_audio_SetConfigRequest *sc = &req->subsystem.audio.request_type.set_config;

    zmk_audio_SetConfigResponse resp = zmk_audio_SetConfigResponse_init_zero;

    if (!sc->has_config || sc->config.volume_attenuation > ALTAR_II_AUDIO_VOLUME_ATT_MAX) {
        resp.result = zmk_audio_SetConfigResponse_Result_RESULT_INVALID;
        return AUDIO_RESPONSE(set_config, resp);
    }

    struct altar_ii_audio_config cfg = {
        .on = sc->config.on,
        .volume_attenuation = (uint16_t)sc->config.volume_attenuation,
        .event_mask = sc->config.event_mask,
    };

    int ret = altar_ii_audio_set_config(&cfg);
    if (ret < 0) {
        LOG_ERR("audio: failed to apply config (err %d)", ret);
        return ZMK_RPC_SIMPLE_ERR(GENERIC);
    }

    resp.result = zmk_audio_SetConfigResponse_Result_RESULT_OK;
    return AUDIO_RESPONSE(set_config, resp);
}

static zmk_studio_Response play_test(const zmk_studio_Request *req) {
    LOG_DBG("");

    int ret = altar_ii_audio_play_test();

    return AUDIO_RESPONSE(play_test, ret == 0);
}

ZMK_RPC_SUBSYSTEM_HANDLER(audio, get_config, ZMK_STUDIO_RPC_HANDLER_SECURED);
ZMK_RPC_SUBSYSTEM_HANDLER(audio, set_config, ZMK_STUDIO_RPC_HANDLER_SECURED);
ZMK_RPC_SUBSYSTEM_HANDLER(audio, play_test, ZMK_STUDIO_RPC_HANDLER_SECURED);

static int audio_settings_reset(void) { return altar_ii_audio_reset_settings(); }

ZMK_RPC_SUBSYSTEM_SETTINGS_RESET(audio, audio_settings_reset);

static int audio_event_mapper(const zmk_event_t *eh, zmk_studio_Notification *n) {
    struct altar_ii_audio_state_changed *state_ev = as_altar_ii_audio_state_changed(eh);

    if (!state_ev) {
        return -ENOTSUP;
    }

    zmk_audio_Config cfg = zmk_audio_Config_init_zero;
    cfg.on = state_ev->on;
    cfg.volume_attenuation = state_ev->volume_attenuation;
    cfg.event_mask = state_ev->event_mask;

    *n = ZMK_RPC_NOTIFICATION(audio, config_changed, cfg);
    return 0;
}

ZMK_RPC_EVENT_MAPPER(audio, audio_event_mapper, altar_ii_audio_state_changed);
