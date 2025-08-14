#pragma once

#include <stdbool.h>
#include <stddef.h>

bool menu_filebrowser(const char* path, const char* filter[], size_t filter_length, char* out_filename,
                      size_t filename_size, const char* title);
