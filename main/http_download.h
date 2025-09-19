#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef void (*download_callback_t)(size_t download_position, size_t file_size, const char* text);

bool download_file(const char* url, const char* path, download_callback_t callback, const char* callback_text);
bool download_ram(const char* url, uint8_t** ptr, size_t* size, download_callback_t callback,
                  const char* callback_text);
