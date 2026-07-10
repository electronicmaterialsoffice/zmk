/*
 * Copyright (c) 2026 Electronic Materials Office Limited
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_behavior_magdial_rotate

#include <zephyr/device.h>
#include <zephyr/logging/log.h>

#include <drivers/behavior.h>

#include <zmk/behavior_queue.h>
#include <zmk/magdial.h>
#include <zmk/virtual_key_position.h>

#include "behavior_sensor_rotate_common.h"

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

// The dial's base rotation dispatch. Sits as a keymap sensor-binding (so a
// higher active layer's own sensor-bindings still preempt it by the normal
// top-down/opaque precedence) and taps the RPC-configured cw/ccw binding once
// per detent trigger. Detent accounting reuses the sensor-rotate common code.

struct behavior_magdial_rotate_config {
    int tap_ms;
};

static int magdial_rotate_process(struct zmk_behavior_binding *binding,
                                  struct zmk_behavior_binding_event event,
                                  enum behavior_sensor_binding_process_mode mode) {
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    const struct behavior_magdial_rotate_config *cfg = dev->config;
    struct behavior_sensor_rotate_data *data = dev->data;

    const int sensor_index = ZMK_SENSOR_POSITION_FROM_VIRTUAL_KEY_POSITION(event.position);

    if (mode != BEHAVIOR_SENSOR_BINDING_PROCESS_MODE_TRIGGER) {
        data->triggers[sensor_index][event.layer] = 0;
        return ZMK_BEHAVIOR_TRANSPARENT;
    }

    int triggers = data->triggers[sensor_index][event.layer];

    enum zmk_magdial_slot slot;
    if (triggers > 0) {
        slot = ZMK_MAGDIAL_SLOT_CW;
    } else if (triggers < 0) {
        triggers = -triggers;
        slot = ZMK_MAGDIAL_SLOT_CCW;
    } else {
        return ZMK_BEHAVIOR_TRANSPARENT;
    }

    struct zmk_behavior_binding target;
    if (!zmk_magdial_resolve_slot(slot, &target)) {
        // Configured empty (or unresolvable): consume the rotation.
        return ZMK_BEHAVIOR_OPAQUE;
    }

    LOG_DBG("magdial rotate -> %s (x%d)", target.behavior_dev, triggers);

#if IS_ENABLED(CONFIG_ZMK_SPLIT)
    event.source = ZMK_POSITION_STATE_CHANGE_SOURCE_LOCAL;
#endif

    for (int i = 0; i < triggers; i++) {
        zmk_behavior_queue_add(&event, target, true, cfg->tap_ms);
        zmk_behavior_queue_add(&event, target, false, 0);
    }

    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api behavior_magdial_rotate_driver_api = {
    .sensor_binding_accept_data = zmk_behavior_sensor_rotate_common_accept_data,
    .sensor_binding_process = magdial_rotate_process,
};

#define MAGDIAL_ROTATE_INST(n)                                                                     \
    static const struct behavior_magdial_rotate_config behavior_magdial_rotate_config_##n = {      \
        .tap_ms = DT_INST_PROP(n, tap_ms),                                                         \
    };                                                                                             \
    static struct behavior_sensor_rotate_data behavior_magdial_rotate_data_##n = {};               \
    BEHAVIOR_DT_INST_DEFINE(n, NULL, NULL, &behavior_magdial_rotate_data_##n,                      \
                            &behavior_magdial_rotate_config_##n, POST_KERNEL,                      \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                                   \
                            &behavior_magdial_rotate_driver_api);

DT_INST_FOREACH_STATUS_OKAY(MAGDIAL_ROTATE_INST)
