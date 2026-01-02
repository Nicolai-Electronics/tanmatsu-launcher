#pragma once

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "chatdb_location.h"
#include "chatdb_networks.h"

#define CHAT_MESSAGE_VERSION   1
#define CHAT_MESSAGE_NAME_SIZE 40  // 32 for Meshcore, 40 for Meshtastic
#define CHAT_MESSAGE_TEXT_SIZE 200

typedef struct {
    uint8_t channel_hash;
    uint8_t mac[2];
    uint8_t text_type;  // Message type (0 for plain text)
} chat_message_meshcore_t;

typedef struct {
    uint8_t reserved;
} chat_message_meshtastic_t;

typedef struct {
    uint32_t       version;                       // Version of the message structure
    char           name[CHAT_MESSAGE_NAME_SIZE];  // sender name (UTF-8)
    char           text[CHAT_MESSAGE_TEXT_SIZE];  // Message text (UTF-8)
    uint32_t       received_at;                   // Unix timestamp (local clock)
    uint32_t       timestamp;                     // Unix timestamp (remote clock)
    chat_network_t network;                       // Network type

    // Network specific fields
    union {
        chat_message_meshcore_t   meshcore;
        chat_message_meshtastic_t meshtastic;
    };

    uint8_t reserved[16];  // Reserved for future use

} __attribute__((packed, aligned(4))) chat_message_t;

static_assert(sizeof(chat_message_t) == 276, "Size of chat_message_t must be 276 bytes");

int chat_message_get_amount(const char* filename, size_t* amount);
int chat_message_load(const char* filename, size_t message, chat_message_t* out_message);
int chat_message_store(const char* filename, chat_message_t* message);