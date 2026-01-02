#include <dirent.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "chatdb_contacts.h"

// Individual contact

int chat_contact_load(const char* filename, chat_contact_t* out_contact) {
    // Placeholder implementation
    printf("Loading contact from file: %s\n", filename);

    FILE* fd = fopen(filename, "rb");
    if (fd == NULL) {
        printf("Failed to open file for reading: %s\n", filename);
        return -1;  // Failure
    }

    fseek(fd, 0, SEEK_END);
    size_t fsize = ftell(fd);
    fseek(fd, 0, SEEK_SET);

    if (fsize != sizeof(chat_contact_t) + sizeof(uint32_t)) {
        printf("Invalid file size: %s\n", filename);
        fclose(fd);
        return -1;  // Failure
    }

    uint32_t stored_checksum = 0;

    fread(out_contact, sizeof(chat_contact_t), 1, fd);
    fread(&stored_checksum, sizeof(uint32_t), 1, fd);
    fclose(fd);

    uint8_t* data_ptr = (uint8_t*)out_contact;
    uint32_t checksum = 0;
    for (size_t i = 0; i < sizeof(chat_contact_t); i++) {
        checksum += data_ptr[i];
    }

    if (stored_checksum != checksum) {
        printf("Invalid checksum for file: %s\n", filename);
        return -1;  // Failure
    }

    if (out_contact->version != CHAT_CONTACT_VERSION) {
        printf("Unsupported contact version: %" PRIu32 "\n", out_contact->version);
        return -1;  // Failure
    }

    out_contact->name[CHAT_CONTACT_NAME_SIZE - 1] = '\0';  // Ensure null-termination

    printf("Loaded contact: %s\n", out_contact->name);

    return 0;  // Success
}

int chat_contact_store(const char* filename, chat_contact_t* contact) {
    // Placeholder implementation
    printf("Storing contact: %s\n", contact->name);

    FILE* fd = fopen(filename, "wb");
    if (fd == NULL) {
        printf("Failed to open file for writing: %s\n", filename);
        return -1;  // Failure
    }

    contact->version = CHAT_CONTACT_VERSION;

    uint8_t* data_ptr = (uint8_t*)contact;
    uint32_t checksum = 0;
    for (size_t i = 0; i < sizeof(chat_contact_t); i++) {
        checksum += data_ptr[i];
    }

    fwrite(contact, sizeof(chat_contact_t), 1, fd);
    fwrite(&checksum, sizeof(uint32_t), 1, fd);
    fclose(fd);

    return 0;  // Success
}

int chat_contact_delete(const char* filename) {
    // Placeholder implementation
    printf("Loading contact from file: %s\n", filename);
    return 0;  // Success
}

// Database

int chat_contact_database_load(const char* base_path) {
    char path_buffer[257];
    printf("Loading all contacts from %s\n", base_path);
    DIR* dir = opendir(base_path);
    if (dir == NULL) {
        printf("Folder does not exist\n");
        return 0;
    }
    struct dirent* entry;
    size_t         count = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR) {
            // Folder
        } else {
            // File
            printf("Found contact file: %s\n", entry->d_name);
            snprintf(path_buffer, sizeof(path_buffer), "%s/%s", base_path, entry->d_name);
            chat_contact_t contact;
            chat_contact_load(path_buffer, &contact);
            count++;
        }
    }
    closedir(dir);
    return 0;  // Success
}
