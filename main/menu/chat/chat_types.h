#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "pax_types.h"

#ifndef PUB_KEY_SIZE
#define PUB_KEY_SIZE 32
#endif

#ifndef MAX_PATH_SIZE
#define MAX_PATH_SIZE 64
#endif

#define CHAT_CONTACT_NAME_SIZE 32

typedef struct {
    int32_t latitude;
    int32_t longitude;
} chat_location_t;

typedef enum {
    CHAT_CONTACT_TYPE_UNKNOWN    = 0x00,
    CHAT_CONTACT_TYPE_MESHCORE   = 0x01,
    CHAT_CONTACT_TYPE_MESHTASTIC = 0x02,
} chat_network_type_t;

typedef struct {
    uint8_t  public_key[PUB_KEY_SIZE];
    uint8_t  type;
    uint8_t  flags;
    int8_t   out_path_len;
    uint8_t  out_path[MAX_PATH_SIZE];
    uint8_t  shared_secret[PUB_KEY_SIZE];
    uint32_t sync_since;
} chat_contact_meshcore_t;

typedef struct {
    char                name[CHAT_CONTACT_NAME_SIZE];
    chat_location_t     location;
    uint32_t            last_modified;  // Unix timestamp of last modification (local clock)
    uint32_t            last_seen;      // Unix timestamp of last seen online (remote clock)
    chat_network_type_t network;
    pax_buf_t*          avatar;
    union {
        chat_contact_meshcore_t meshcore;
    };
} chat_contact_t;

typedef struct {
    uint32_t        message_id;
    uint32_t        timestamp;
    char*           message;
    bool            sent_by_user;
    chat_contact_t* sender;
} chat_message_t;
