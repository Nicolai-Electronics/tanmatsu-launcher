
// SPDX-Copyright-Text: 2025 Julian Scheffers
// SPDX-License-Identifier: MIT

#pragma once

#include <stddef.h>
#include <stdint.h>

// Prepare the data for the BadgeLink service to start.
void badgelink_init();

// Start the badgelink service.
void badgelink_start();

// Handle received data.
void badgelink_rxdata_cb(uint8_t const* data, size_t len);
