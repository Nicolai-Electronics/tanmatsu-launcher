#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "chatdb_location.h"
#include "chatdb_networks.h"

#define CHAT_CONTACT_VERSION      1
#define CHAT_CONTACT_NAME_SIZE    40  // 32 for Meshcore, 40 for Meshtastic
#define CHAT_CONTACT_MC_KEY_SIZE  32
#define CHAT_CONTACT_MC_PATH_SIZE 64

#define CHAT_CONTACT_MESHTASTIC_FLAG_IS_LICENSED         (1 << 0)
#define CHAT_CONTACT_MESHTASTIC_FLAG_HAS_IS_UNMESSAGABLE (1 << 1)
#define CHAT_CONTACT_MESHTASTIC_FLAG_IS_UNMESSAGABLE     (1 << 2)

#define CHAT_CONTACT_MESHTASTIC_TYPE_CLIENT         0
#define CHAT_CONTACT_MESHTASTIC_TYPE_CLIENT_MUTE    1
#define CHAT_CONTACT_MESHTASTIC_TYPE_ROUTER         2
#define CHAT_CONTACT_MESHTASTIC_TYPE_ROUTER_CLIENT  3
#define CHAT_CONTACT_MESHTASTIC_TYPE_REPEATER       4
#define CHAT_CONTACT_MESHTASTIC_TYPE_TRACKER        5
#define CHAT_CONTACT_MESHTASTIC_TYPE_SENSOR         6
#define CHAT_CONTACT_MESHTASTIC_TYPE_TAK            7
#define CHAT_CONTACT_MESHTASTIC_TYPE_CLIENT_HIDDEN  8
#define CHAT_CONTACT_MESHTASTIC_TYPE_LOST_AND_FOUND 9
#define CHAT_CONTACT_MESHTASTIC_TYPE_TAK_TRACKER    10
#define CHAT_CONTACT_MESHTASTIC_TYPE_ROUTER_LATE    11
#define CHAT_CONTACT_MESHTASTIC_TYPE_CLIENT_BASE    12

typedef struct {
    uint8_t  public_key[CHAT_CONTACT_MC_KEY_SIZE];
    uint8_t  type;
    uint8_t  flags;
    int8_t   out_path_len;
    uint8_t  out_path[CHAT_CONTACT_MC_PATH_SIZE];
    uint8_t  shared_secret[CHAT_CONTACT_MC_KEY_SIZE];
    uint32_t sync_since;
} chat_contact_meshcore_t;

typedef struct {
    uint8_t  public_key[CHAT_CONTACT_MC_KEY_SIZE];
    uint8_t  type;
    uint8_t  flags;
    char     id[16];
    char     short_name[5];
    uint8_t  mac_address[6];
    uint32_t hardware_model;
} chat_contact_meshtastic_t;

typedef struct {
    uint32_t        version;                       // Version of the contact structure
    char            name[CHAT_CONTACT_NAME_SIZE];  // Contact name (UTF-8)
    uint32_t        last_modified;                 // Unix timestamp of last modification (local clock)
    uint32_t        last_seen;                     // Unix timestamp of last seen online (remote clock)
    chat_location_t location;                      // Location
    chat_network_t  network;                       // Network type

    // Network specific fields
    union {
        chat_contact_meshcore_t   meshcore;
        chat_contact_meshtastic_t meshtastic;
    };

    uint8_t reserved[50];  // Reserved for future use

} __attribute__((packed, aligned(4))) chat_contact_t;

static_assert(sizeof(chat_contact_t) == 252, "Size of chat_contact_t must be 252 bytes");

int chat_contact_store(const char* filename, chat_contact_t* contact);
int chat_contact_load(const char* filename, chat_contact_t* out_contact);
int chat_contact_delete(const char* filename);

int chat_contact_database_load(const char* base_path);
