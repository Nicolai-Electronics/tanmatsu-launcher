
// SPDX-License-Identifier: MIT
// SPDX-CopyrightText: 2025 Julian Scheffers <julian@scheffers.net>

#pragma once

#include <esp_err.h>
#include "menu/texteditor/types.h"

// Delete a text file.
void text_file_delete(text_file_t file);

// Load a blob of UTF-8 text.
esp_err_t text_file_from_blob(text_file_t* file_out, const char* data, size_t data_len);
// Load a file from a handle.
esp_err_t text_file_load(text_file_t* file_out, FILE* fd);

// Save a file into a blob of UTF-8 text.
esp_err_t text_file_to_blob(const text_file_t* file, char** ptr_out, size_t* len_out);
// Save a file to a handle.
esp_err_t text_file_save(const text_file_t* file, FILE* fd);
