/*
 * Copyright (c) 2026 Electronic Materials Office Limited
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/settings/settings.h>
#include <zephyr/sys/util.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <dt-bindings/zmk/keys.h>
#include <zmk/behavior.h>
#include <zmk/magdial.h>
#include <zmk/sensors.h>

#define MAGDIAL_STATE_VERSION 1
#define MAGDIAL_SETTINGS_KEY "magdial/state"

// The whole config as one versioned, fixed-size settings blob. Any layout
// change must bump MAGDIAL_STATE_VERSION; a mismatched blob is deleted and
// defaults restored (never misread).
struct magdial_state {
    uint8_t version;
    uint8_t sensitivity;
    bool press_enabled;
    struct zmk_magdial_slot_binding slots[ZMK_MAGDIAL_SLOT_COUNT];
} __packed;

// version == 0 means "nothing persisted": slots come from the defaults table.
static struct magdial_state state = {
    .version = 0,
    .sensitivity = ZMK_MAGDIAL_SENSITIVITY_DEFAULT,
    .press_enabled = true,
};

static bool state_stale = false;

// Guards `state`: the Studio RPC thread (lowest priority) writes it while the
// dial behaviors read it from the higher-priority system workqueue. Critical
// sections hold only plain memory copies — flash writes and behavior-registry
// lookups stay outside the lock.
static struct k_spinlock state_lock;

// Factory defaults are stored as behavior *names* and resolved lazily —
// local ids are only assigned once settings have loaded, and a board whose
// keymap lacks one of these behaviors degrades to an empty slot.
struct magdial_default {
    const char *behavior_name;
    uint32_t param1;
    uint32_t param2;
};

static const struct magdial_default magdial_defaults[ZMK_MAGDIAL_SLOT_COUNT] = {
    [ZMK_MAGDIAL_SLOT_CW] = {"key_press", C_VOL_UP, 0},
    [ZMK_MAGDIAL_SLOT_CCW] = {"key_press", C_VOL_DN, 0},
    [ZMK_MAGDIAL_SLOT_SINGLE] = {"key_press", C_PLAY_PAUSE, 0},
    [ZMK_MAGDIAL_SLOT_DOUBLE] = {"key_press", C_NEXT, 0},
    [ZMK_MAGDIAL_SLOT_TRIPLE] = {"key_press", C_PREV, 0},
    [ZMK_MAGDIAL_SLOT_LONG] = {"siri_macro", 0, 0},
};

// Sensitivity 0..10 -> percent of the board's devicetree triggers-per-rotation
// base. 5 = stock feel; 0 = quarter rate (coarse); 10 = triple rate (fine).
static const uint16_t sensitivity_pct[ZMK_MAGDIAL_SENSITIVITY_MAX + 1] = {
    25, 40, 55, 70, 85, 100, 125, 150, 200, 250, 300,
};

#if ZMK_KEYMAP_HAS_SENSORS

static uint16_t base_triggers_per_rotation = 0;

static void magdial_apply_sensitivity(void) {
    if (base_triggers_per_rotation == 0) {
        const struct zmk_sensor_config *cfg = zmk_sensors_get_config_at_index(0);
        if (!cfg) {
            return;
        }
        base_triggers_per_rotation = cfg->triggers_per_rotation;
    }

    uint8_t s = MIN(state.sensitivity, ZMK_MAGDIAL_SENSITIVITY_MAX);
    uint16_t triggers = MAX(1, (base_triggers_per_rotation * sensitivity_pct[s]) / 100);

    zmk_sensors_set_triggers_per_rotation(0, triggers);
    LOG_DBG("magdial sensitivity %d -> %d triggers/rotation (base %d)", s, triggers,
            base_triggers_per_rotation);
}

#else

static void magdial_apply_sensitivity(void) {}

#endif // ZMK_KEYMAP_HAS_SENSORS

bool zmk_magdial_press_enabled(void) {
    k_spinlock_key_t key = k_spin_lock(&state_lock);
    bool enabled = state.press_enabled;
    k_spin_unlock(&state_lock, key);
    return enabled;
}

bool zmk_magdial_resolve_slot(enum zmk_magdial_slot slot, struct zmk_behavior_binding *out) {
    if (slot >= ZMK_MAGDIAL_SLOT_COUNT) {
        return false;
    }

    k_spinlock_key_t key = k_spin_lock(&state_lock);
    uint8_t version = state.version;
    struct zmk_magdial_slot_binding sb = state.slots[slot];
    k_spin_unlock(&state_lock, key);

    if (version == 0) {
        const struct magdial_default *def = &magdial_defaults[slot];
        if (!zmk_behavior_get_binding(def->behavior_name)) {
            return false;
        }
        *out = (struct zmk_behavior_binding){
            .behavior_dev = def->behavior_name,
            .param1 = def->param1,
            .param2 = def->param2,
        };
        return true;
    }

    if (sb.local_id == 0) {
        return false;
    }

    const char *name = zmk_behavior_find_behavior_name_from_local_id(sb.local_id);
    if (!name) {
        LOG_WRN("magdial slot %d: no behavior for stored local id %d", slot, sb.local_id);
        return false;
    }

    *out = (struct zmk_behavior_binding){
        .behavior_dev = name,
        .param1 = sb.param1,
        .param2 = sb.param2,
    };
    return true;
}

void zmk_magdial_get_config(struct zmk_magdial_slot_binding slots[ZMK_MAGDIAL_SLOT_COUNT],
                            bool *press_enabled, uint8_t *sensitivity) {
    k_spinlock_key_t key = k_spin_lock(&state_lock);
    struct magdial_state snapshot = state;
    k_spin_unlock(&state_lock, key);

    if (snapshot.version == 0) {
        for (int i = 0; i < ZMK_MAGDIAL_SLOT_COUNT; i++) {
            zmk_behavior_local_id_t id = zmk_behavior_get_local_id(magdial_defaults[i].behavior_name);
            if (id == UINT16_MAX) {
                slots[i] = (struct zmk_magdial_slot_binding){0};
            } else {
                slots[i] = (struct zmk_magdial_slot_binding){
                    .local_id = id,
                    .param1 = magdial_defaults[i].param1,
                    .param2 = magdial_defaults[i].param2,
                };
            }
        }
    } else {
        memcpy(slots, snapshot.slots, sizeof(snapshot.slots));
    }

    *press_enabled = snapshot.press_enabled;
    *sensitivity = MIN(snapshot.sensitivity, ZMK_MAGDIAL_SENSITIVITY_MAX);
}

int zmk_magdial_set_config(const struct zmk_magdial_slot_binding slots[ZMK_MAGDIAL_SLOT_COUNT],
                           bool press_enabled, uint8_t sensitivity) {
    struct magdial_state next = {
        .version = MAGDIAL_STATE_VERSION,
        .press_enabled = press_enabled,
        .sensitivity = MIN(sensitivity, ZMK_MAGDIAL_SENSITIVITY_MAX),
    };
    memcpy(next.slots, slots, sizeof(next.slots));

    k_spinlock_key_t key = k_spin_lock(&state_lock);
    state = next;
    k_spin_unlock(&state_lock, key);

    magdial_apply_sensitivity();

    // Persist the local copy: `state` may be rewritten by a later set while
    // the flash write is in flight, and the save must not hold the lock.
    return settings_save_one(MAGDIAL_SETTINGS_KEY, &next, sizeof(next));
}

int zmk_magdial_reset_settings(void) {
    k_spinlock_key_t key = k_spin_lock(&state_lock);
    state = (struct magdial_state){
        .version = 0,
        .sensitivity = ZMK_MAGDIAL_SENSITIVITY_DEFAULT,
        .press_enabled = true,
    };
    k_spin_unlock(&state_lock, key);
    magdial_apply_sensitivity();
    return settings_delete(MAGDIAL_SETTINGS_KEY);
}

static int magdial_settings_set(const char *name, size_t len, settings_read_cb read_cb,
                                void *cb_arg) {
    const char *next;

    if (!settings_name_steq(name, "state", &next) || next) {
        return 0;
    }

    if (len != sizeof(struct magdial_state)) {
        LOG_WRN("magdial: persisted blob has wrong size (%d != %d), will reset", (int)len,
                (int)sizeof(struct magdial_state));
        state_stale = true;
        return 0;
    }

    struct magdial_state loaded;
    int err = read_cb(cb_arg, &loaded, sizeof(loaded));
    if (err <= 0) {
        LOG_ERR("magdial: failed to read persisted state (err %d)", err);
        return err;
    }

    if (loaded.version != MAGDIAL_STATE_VERSION) {
        LOG_WRN("magdial: persisted blob version %d != %d, will reset", loaded.version,
                MAGDIAL_STATE_VERSION);
        state_stale = true;
        return 0;
    }

    k_spinlock_key_t key = k_spin_lock(&state_lock);
    state = loaded;
    k_spin_unlock(&state_lock, key);
    return 0;
}

static int magdial_settings_commit(void) {
    if (state_stale) {
        // Delete-and-reset rather than log-and-ignore: prevents a rollback
        // from resurrecting a partially-migrated newer-schema blob.
        state_stale = false;
        settings_delete(MAGDIAL_SETTINGS_KEY);
        LOG_WRN("magdial: stale config deleted, firmware defaults restored");
    }

    magdial_apply_sensitivity();
    return 0;
}

SETTINGS_STATIC_HANDLER_DEFINE(magdial, "magdial", NULL, magdial_settings_set,
                               magdial_settings_commit, NULL);
