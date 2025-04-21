
// SPDX-Copyright-Text: 2025 Julian Scheffers
// SPDX-License-Identifier: MIT

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

typedef int appfs_handle_t;

#define APPFS_INVALID_FD        -1
#define SPI_FLASH_MMU_PAGE_SIZE 0x10000

int            appfsExists(const char* filename);
appfs_handle_t appfsOpen(const char* filename);
esp_err_t      appfsDeleteFile(const char* filename);
esp_err_t      appfsCreateFileExt(const char* filename, const char* title, uint16_t version, size_t size,
                                  appfs_handle_t* handle);
esp_err_t      appfsErase(appfs_handle_t fd, size_t start, size_t len);
esp_err_t      appfsWrite(appfs_handle_t fd, size_t start, uint8_t* buf, size_t len);
esp_err_t      appfsRead(appfs_handle_t fd, size_t start, void* buf, size_t len);

void appfsEntryInfoExt(appfs_handle_t fd, const char** name, const char** title, uint16_t* version, int* size);
appfs_handle_t appfsNextEntry(appfs_handle_t fd);
size_t         appfsGetFreeMem();
size_t         appfsGetTotalMem();

static inline void appfsEntryInfo(appfs_handle_t fd, const char** name, int* size) {
    appfsEntryInfoExt(fd, name, NULL, NULL, size);
}
