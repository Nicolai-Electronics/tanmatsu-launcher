
// SPDX-Copyright-Text: 2025 Julian Scheffers
// SPDX-License-Identifier: MIT

#pragma once

#include "badgelink.h"
#include "badgelink.pb.h"
#include "cobs.h"

// What the current file transfer is for.
typedef enum {
    // Not currently transferring.
    BADGELINK_XFER_NONE,
    // AppFS file transfer.
    BADGELINK_XFER_APPFS,
    // Internal filesystem file transfer.
    BADGELINK_XFER_FS,
} badgelink_xfer_t;

// Capacity for the transmit/receive buffer.
#define BADGELINK_BUF_CAP COBS_ENCODED_MAX_LENGTH(badgelink_Packet_size + 4)

// Badgelink packet singleton used for both the request and its response.
extern badgelink_Packet badgelink_packet;
// What the current file transfer is for.
extern badgelink_xfer_t badgelink_xfer_type;
// Whether the current file transfer is host->badge.
extern bool             badgelink_xfer_is_upload;
// Current transfer position.
extern uint32_t         badgelink_xfer_pos;
// Transfer file size.
extern uint32_t         badgelink_xfer_size;

// Send raw bytes of data.
void   badgelink_raw_tx(void const* buf, size_t len);
// Receive raw bytes of data.
size_t badgelink_raw_rx(void* buf, size_t max_len);

// Encode and send a packet.
void badgelink_send_packet();
// Send a status response packet.
void badgelink_send_status(badgelink_StatusCode code);

// Send an "ok" status.
static inline void badgelink_status_ok() {
    badgelink_send_status(badgelink_StatusCode_StatusOk);
}

// Send an "unsupported" status.
static inline void badgelink_status_unsupported() {
    badgelink_send_status(badgelink_StatusCode_StatusNotSupported);
}

// Send a "not found" status.
static inline void badgelink_status_not_found() {
    badgelink_send_status(badgelink_StatusCode_StatusNotFound);
}

// Send a "malformed request" status.
static inline void badgelink_status_malformed() {
    badgelink_send_status(badgelink_StatusCode_StatusMalformed);
}

// Send an "internal error" status.
static inline void badgelink_status_int_err() {
    badgelink_send_status(badgelink_StatusCode_StatusInternalError);
}

// Send an "illegal state" status.
static inline void badgelink_status_ill_state() {
    badgelink_send_status(badgelink_StatusCode_StatusIllegalState);
}

// Send an "already exists" status.
static inline void badgelink_status_no_space() {
    badgelink_send_status(badgelink_StatusCode_StatusNoSpace);
}

// Send a "not empty" status.
static inline void badgelink_status_not_empty() {
    badgelink_send_status(badgelink_StatusCode_StatusNotEmpty);
}

// Send an "is a file" status.
static inline void badgelink_status_is_file() {
    badgelink_send_status(badgelink_StatusCode_StatusIsFile);
}

// Send an "is a directory" status.
static inline void badgelink_status_is_dir() {
    badgelink_send_status(badgelink_StatusCode_StatusIsDir);
}

// Send a "file exists" status.
static inline void badgelink_status_exists() {
    badgelink_send_status(badgelink_StatusCode_StatusExists);
}
