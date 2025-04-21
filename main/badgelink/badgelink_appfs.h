
// SPDX-Copyright-Text: 2025 Julian Scheffers
// SPDX-License-Identifier: MIT

#pragma once

#include "badgelink_internal.h"

// Handle an AppFS request packet.
void badgelink_appfs_handle();
// Handle an AppFS upload (host->badge) transfer.
void badgelink_appfs_xfer_upload();
// Handle an AppFS download (badge->host) transfer.
void badgelink_appfs_xfer_download();
// Finish an AppFS transfer.
void badgelink_appfs_xfer_stop(bool abnormal);

// Handle an AppFS list request.
void badgelink_appfs_list();
// Handle an AppFS delete request.
void badgelink_appfs_delete();
// Handle an AppFS upload request.
void badgelink_appfs_upload();
// Handle an AppFS download request.
void badgelink_appfs_download();
// Handle an AppFS stat request.
void badgelink_appfs_stat();
// Handle an AppFS crc32 request.
void badgelink_appfs_crc32();
// Handle an AppFS usage statistics request.
void badgelink_appfs_usage();
