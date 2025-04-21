
// SPDX-Copyright-Text: 2025 Julian Scheffers
// SPDX-License-Identifier: MIT

#pragma once

#include "badgelink_internal.h"

// Handle an NVS request packet.
void badgelink_nvs_handle();

// Handle an NVS list request.
void badgelink_nvs_list();
// Handle an NVS read request.
void badgelink_nvs_read();
// Handle an NVS write request.
void badgelink_nvs_write();
// Handle an NVS delete request.
void badgelink_nvs_delete();
