#include <dirent.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "chatdb_messages.h"

int chat_message_get_amount(const char* filename, size_t* amount) {
    FILE* fd = fopen(filename, "rb");
    if (fd == NULL) {
        printf("Failed to open file for reading: %s\n", filename);
        return -1;  // Failure
    }

    fseek(fd, 0, SEEK_END);
    size_t fsize = ftell(fd);
    fseek(fd, 0, SEEK_SET);

    fclose(fd);

    *amount = fsize / (sizeof(chat_message_t) + sizeof(uint32_t));
    return 0;  // Success
}

int chat_message_load(const char* filename, size_t message, chat_message_t* out_message) {
    FILE* fd = fopen(filename, "rb");
    if (fd == NULL) {
        printf("Failed to open file for reading: %s\n", filename);
        return -1;  // Failure
    }

    size_t offset = message * (sizeof(chat_message_t) + sizeof(uint32_t));
    fseek(fd, offset, SEEK_SET);

    uint32_t stored_checksum  = 0;
    bool     success          = fread(out_message, sizeof(chat_message_t), 1, fd) == 1;
    success                  &= fread(&stored_checksum, sizeof(uint32_t), 1, fd) == 1;
    fclose(fd);

    if (!success) {
        printf("Failed to read complete message from file: %s\n", filename);
        return -1;  // Failure
    }

    uint8_t* data_ptr = (uint8_t*)out_message;
    uint32_t checksum = 0;
    for (size_t i = 0; i < sizeof(chat_message_t); i++) {
        checksum += data_ptr[i];
    }

    if (stored_checksum != checksum) {
        printf("Invalid checksum for message: %s:%" PRIu16 "\n", filename, message);
        return -1;  // Failure
    }

    return 0;  // Success
}

int chat_message_store(const char* filename, chat_message_t* message) {
    FILE* fd = fopen(filename, "ab");
    if (fd == NULL) {
        printf("Failed to open file for writing: %s\n", filename);
        return -1;  // Failure
    }

    uint8_t* data_ptr = (uint8_t*)message;
    uint32_t checksum = 0;
    for (size_t i = 0; i < sizeof(chat_message_t); i++) {
        checksum += data_ptr[i];
    }

    bool success  = fwrite(message, sizeof(chat_message_t), 1, fd) == 1;
    success      &= fwrite(&checksum, sizeof(uint32_t), 1, fd) == 1;
    fclose(fd);

    if (!success) {
        printf("Failed to write complete message to file: %s\n", filename);
        return -1;  // Failure
    }

    return 0;  // Success
}
