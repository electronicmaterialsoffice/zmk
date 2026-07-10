/*
 * Copyright (c) 2026 Electronic Materials Office Limited
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <zephyr/kernel.h>
#include <zmk/event_manager.h>

struct zmk_backlight_state_changed {
    bool on;
    uint8_t brightness;
};

ZMK_EVENT_DECLARE(zmk_backlight_state_changed);
