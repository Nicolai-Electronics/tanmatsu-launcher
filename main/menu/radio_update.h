#pragma once

#include <stdbool.h>
#include <stdint.h>

void radio_update(char* path, bool compressed, uint32_t uncompressed_size);
void radio_install(const char* instructions_filename);