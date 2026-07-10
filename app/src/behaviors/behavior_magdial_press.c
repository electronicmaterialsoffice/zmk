/*
 * Copyright (c) 2026 Electronic Materials Office Limited
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_behavior_magdial_press

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <drivers/behavior.h>

#include <zmk/behavior_queue.h>
#include <zmk/magdial.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

// Multi-press dispatcher for the dial's push button: single / double / triple
// tap and long press each tap their RPC-configured binding. Policy is
// wait-for-window (correctness over latency): a tap sequence resolves
// tapping-term-ms after the last release; triple resolves immediately on the
// third press; holding the FIRST press past hold-ms fires the long binding.
// With the press disabled over RPC, presses are swallowed entirely.

struct behavior_magdial_press_config {
    int tapping_term_ms;
    int hold_ms;
    int tap_ms;
};

struct behavior_magdial_press_data {
    const struct device *dev;
    struct k_work_delayable resolve_work;
    struct k_work_delayable hold_work;
    struct zmk_behavior_binding_event event;
    uint8_t count;
    bool pressed;
};

static void magdial_press_dispatch(const struct device *dev, enum zmk_magdial_slot slot) {
    const struct behavior_magdial_press_config *cfg = dev->config;
    struct behavior_magdial_press_data *data = dev->data;

    struct zmk_behavior_binding target;
    if (!zmk_magdial_resolve_slot(slot, &target)) {
        return;
    }

    LOG_DBG("magdial press slot %d -> %s", slot, target.behavior_dev);

    zmk_behavior_queue_add(&data->event, target, true, cfg->tap_ms);
    zmk_behavior_queue_add(&data->event, target, false, 0);
}

static void magdial_press_reset(struct behavior_magdial_press_data *data) {
    data->count = 0;
    k_work_cancel_delayable(&data->resolve_work);
    k_work_cancel_delayable(&data->hold_work);
}

static void magdial_press_resolve_cb(struct k_work *work) {
    struct k_work_delayable *dwork = k_work_delayable_from_work(work);
    struct behavior_magdial_press_data *data =
        CONTAINER_OF(dwork, struct behavior_magdial_press_data, resolve_work);

    enum zmk_magdial_slot slot;
    switch (data->count) {
    case 1:
        slot = ZMK_MAGDIAL_SLOT_SINGLE;
        break;
    case 2:
        slot = ZMK_MAGDIAL_SLOT_DOUBLE;
        break;
    default:
        slot = ZMK_MAGDIAL_SLOT_TRIPLE;
        break;
    }

    data->count = 0;
    magdial_press_dispatch(data->dev, slot);
}

static void magdial_press_hold_cb(struct k_work *work) {
    struct k_work_delayable *dwork = k_work_delayable_from_work(work);
    struct behavior_magdial_press_data *data =
        CONTAINER_OF(dwork, struct behavior_magdial_press_data, hold_work);

    // Long press only resolves from the first press of a sequence.
    if (data->pressed && data->count == 1) {
        data->count = 0; // consumed: the release must not schedule a tap resolve
        magdial_press_dispatch(data->dev, ZMK_MAGDIAL_SLOT_LONG);
    }
}

static int magdial_press_pressed(struct zmk_behavior_binding *binding,
                                 struct zmk_behavior_binding_event event) {
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    const struct behavior_magdial_press_config *cfg = dev->config;
    struct behavior_magdial_press_data *data = dev->data;

    if (!zmk_magdial_press_enabled()) {
        magdial_press_reset(data);
        return ZMK_BEHAVIOR_OPAQUE;
    }

    data->pressed = true;
    data->event = event;
    data->count++;

    k_work_cancel_delayable(&data->resolve_work);

    if (data->count == 1) {
        k_work_schedule(&data->hold_work, K_MSEC(cfg->hold_ms));
    } else if (data->count >= 3) {
        // Max tap count: resolve immediately, no further window.
        k_work_cancel_delayable(&data->hold_work);
        data->count = 0;
        magdial_press_dispatch(dev, ZMK_MAGDIAL_SLOT_TRIPLE);
    }

    return ZMK_BEHAVIOR_OPAQUE;
}

static int magdial_press_released(struct zmk_behavior_binding *binding,
                                  struct zmk_behavior_binding_event event) {
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    const struct behavior_magdial_press_config *cfg = dev->config;
    struct behavior_magdial_press_data *data = dev->data;

    data->pressed = false;
    k_work_cancel_delayable(&data->hold_work);

    // count == 0 here means the sequence already resolved (triple/long) or
    // the press was swallowed while disabled — nothing to schedule.
    if (data->count > 0) {
        k_work_schedule(&data->resolve_work, K_MSEC(cfg->tapping_term_ms));
    }

    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api behavior_magdial_press_driver_api = {
    .binding_pressed = magdial_press_pressed,
    .binding_released = magdial_press_released,
};

static int behavior_magdial_press_init(const struct device *dev) {
    struct behavior_magdial_press_data *data = dev->data;

    data->dev = dev;
    k_work_init_delayable(&data->resolve_work, magdial_press_resolve_cb);
    k_work_init_delayable(&data->hold_work, magdial_press_hold_cb);

    return 0;
}

#define MAGDIAL_PRESS_INST(n)                                                                      \
    static const struct behavior_magdial_press_config behavior_magdial_press_config_##n = {        \
        .tapping_term_ms = DT_INST_PROP(n, tapping_term_ms),                                       \
        .hold_ms = DT_INST_PROP(n, hold_ms),                                                       \
        .tap_ms = DT_INST_PROP(n, tap_ms),                                                         \
    };                                                                                             \
    static struct behavior_magdial_press_data behavior_magdial_press_data_##n = {};                \
    BEHAVIOR_DT_INST_DEFINE(n, behavior_magdial_press_init, NULL,                                  \
                            &behavior_magdial_press_data_##n,                                      \
                            &behavior_magdial_press_config_##n, POST_KERNEL,                       \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                                   \
                            &behavior_magdial_press_driver_api);

DT_INST_FOREACH_STATUS_OKAY(MAGDIAL_PRESS_INST)
