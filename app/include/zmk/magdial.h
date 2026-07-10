/*
 * Copyright (c) 2026 Electronic Materials Office Limited
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <zmk/behavior.h>

#define ZMK_MAGDIAL_SLOT_COUNT 6

#define ZMK_MAGDIAL_SENSITIVITY_MAX 10
#define ZMK_MAGDIAL_SENSITIVITY_DEFAULT 5

enum zmk_magdial_slot {
    ZMK_MAGDIAL_SLOT_CW = 0,
    ZMK_MAGDIAL_SLOT_CCW,
    ZMK_MAGDIAL_SLOT_SINGLE,
    ZMK_MAGDIAL_SLOT_DOUBLE,
    ZMK_MAGDIAL_SLOT_TRIPLE,
    ZMK_MAGDIAL_SLOT_LONG,
};

// One configured slot as persisted: a behavior local id plus its params.
// local_id == 0 means the slot is empty (no-op), mirroring the keymap's
// "local_id == 0 -> no behavior" convention.
struct zmk_magdial_slot_binding {
    zmk_behavior_local_id_t local_id;
    uint32_t param1;
    uint32_t param2;
};

bool zmk_magdial_press_enabled(void);

// Resolve a slot to an invocable binding. Returns false for an empty slot or
// a stored id that no longer resolves to a behavior (never mis-binds).
// Resolution is lazy: local ids are only valid after settings have loaded,
// which is guaranteed by the time a dial event or RPC arrives.
bool zmk_magdial_resolve_slot(enum zmk_magdial_slot slot, struct zmk_behavior_binding *out);

// Snapshot the effective config (factory defaults resolved to local ids when
// nothing is persisted yet) for the Studio get_config response.
void zmk_magdial_get_config(struct zmk_magdial_slot_binding slots[ZMK_MAGDIAL_SLOT_COUNT],
                            bool *press_enabled, uint8_t *sensitivity);

// Validate-free apply + persist (the RPC layer validates first). Returns a
// negative errno if persisting failed; the config is applied regardless.
int zmk_magdial_set_config(const struct zmk_magdial_slot_binding slots[ZMK_MAGDIAL_SLOT_COUNT],
                           bool press_enabled, uint8_t sensitivity);

// Factory reset: delete the persisted blob and return to firmware defaults.
int zmk_magdial_reset_settings(void);
