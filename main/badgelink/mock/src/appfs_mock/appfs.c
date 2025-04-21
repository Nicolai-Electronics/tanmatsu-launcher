
// SPDX-Copyright-Text: 2025 Julian Scheffers
// SPDX-License-Identifier: MIT

#include "appfs.h"
#include <malloc.h>
#include <stdlib.h>
#include <string.h>

int appfsExists(const char* filename) {
    printf("TODO\n");
    abort();
}
appfs_handle_t appfsOpen(const char* filename) {
    printf("TODO\n");
    abort();
}
esp_err_t appfsDeleteFile(const char* filename) {
    printf("TODO\n");
    abort();
}
esp_err_t appfsCreateFileExt(const char* filename, const char* title, uint16_t version, size_t size,
                             appfs_handle_t* handle) {
    printf("TODO\n");
    abort();
}
esp_err_t appfsErase(appfs_handle_t fd, size_t start, size_t len) {
    printf("TODO\n");
    abort();
}
esp_err_t appfsWrite(appfs_handle_t fd, size_t start, uint8_t* buf, size_t len) {
    printf("TODO\n");
    abort();
}
esp_err_t appfsRead(appfs_handle_t fd, size_t start, void* buf, size_t len) {
    printf("TODO\n");
    abort();
}

void appfsEntryInfoExt(appfs_handle_t fd, const char** name, const char** title, uint16_t* version, int* size) {
    printf("TODO\n");
    abort();
}
appfs_handle_t appfsNextEntry(appfs_handle_t fd) {
    printf("TODO\n");
    abort();
}
size_t appfsGetFreeMem() {
    printf("TODO\n");
    abort();
}
size_t appfsGetTotalMem() {
    printf("TODO\n");
    abort();
}
