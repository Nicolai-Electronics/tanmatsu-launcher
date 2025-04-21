
// SPDX-Copyright-Text: 2025 Julian Scheffers
// SPDX-License-Identifier: MIT

#pragma once

#include "badgelink_internal.h"

// Handle a FS request packet.
void badgelink_fs_handle();
// Handle a FS upload (host->badge) transfer.
void badgelink_fs_xfer_upload();
// Handle a FS download (badge->host) transfer.
void badgelink_fs_xfer_download();
// Finish a FS transfer.
void badgelink_fs_xfer_stop(bool abnormal);

// Handle a FS list request.
void badgelink_fs_list();
// Handle a FS delete request.
void badgelink_fs_delete();
// Handle a FS mkdir request.
void badgelink_fs_mkdir();
// Handle a FS upload request.
void badgelink_fs_upload();
// Handle a FS download request.
void badgelink_fs_download();
// Handle a FS stat request.
void badgelink_fs_stat();
// Handle a FS crc32 request.
void badgelink_fs_crc32();
// Handle a FS usage statistics request.
void badgelink_fs_usage();
// Handle a FS rmdir request.
void badgelink_fs_rmdir();
