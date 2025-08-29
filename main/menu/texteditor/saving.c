
// SPDX-License-Identifier: MIT
// SPDX-CopyrightText: 2025 Julian Scheffers <julian@scheffers.net>

#include "menu/texteditor/saving.h"
#include <errno.h>
#include "esp_err.h"
#include "menu/texteditor/editing.h"
#include "menu/texteditor/types.h"
#include "pax_gfx.h"

// Delete a text file.
void text_file_delete(text_file_t file) {
    for (size_t i = 0; i < file.lines_len; i++) {
        free(file.lines[i].data);
    }
    free(file.lines);
}

// Load a blob of UTF-8 text.
esp_err_t text_file_from_blob(text_file_t* file, const char* data, size_t data_len) {
    *file = (text_file_t){0};
    if (!text_file_new_line(file, 0)) {
        return ESP_ERR_NO_MEM;
    }

    size_t lineno = 0;
    for (size_t i = 0; i < data_len; i++) {
        if (data[i] == '\r' || data[i] == '\n') {
            // Insert newline.
            if (data[i] == '\r') {
                if (i < data_len - 1 && data[i + 1] == '\n') {
                    file->lines[lineno].line_end = LINE_END_CRLF;
                    i++;
                } else {
                    file->lines[lineno].line_end = LINE_END_CR;
                }
            } else {
                file->lines[lineno].line_end = LINE_END_LF;
            }
            if (!text_file_new_line(file, ++lineno)) {
                text_file_delete(*file);
                *file = (text_file_t){0};
                return ESP_ERR_NO_MEM;
            }
        } else {
            // Just append the character.
            if (!text_line_insert(file, lineno, file->lines[lineno].data_len, data + i, 1)) {
                text_file_delete(*file);
                *file = (text_file_t){0};
                return ESP_ERR_NO_MEM;
            }
        }
    }

    return ESP_OK;
}

// Load a file from a handle.
esp_err_t text_file_load(text_file_t* file, FILE* fd) {
    *file = (text_file_t){0};
    if (!text_file_new_line(file, 0)) {
        return ESP_ERR_NO_MEM;
    }

    size_t lineno = 0;
    while (1) {
        int cur = fgetc(fd);
        if (cur == EOF) {
            return errno == 0 ? ESP_OK : ESP_FAIL;
        }
        if (cur == '\r' || cur == '\n') {
            // Insert newline.
            if (cur == '\r') {
                int next = fgetc(fd);
                if (next == EOF && errno != 0) {
                    return ESP_FAIL;
                } else if (next == '\n') {
                    file->lines[lineno].line_end = LINE_END_CRLF;
                } else {
                    file->lines[lineno].line_end = LINE_END_CR;
                }
            } else {
                file->lines[lineno].line_end = LINE_END_CR;
            }
            if (!text_file_new_line(file, ++lineno)) {
                text_file_delete(*file);
                *file = (text_file_t){0};
                return ESP_ERR_NO_MEM;
            }
        } else {
            // Just append the character.
            if (!text_line_insert(file, lineno, file->lines[lineno].data_len, (char[]){cur}, 1)) {
                text_file_delete(*file);
                *file = (text_file_t){0};
                return ESP_ERR_NO_MEM;
            }
        }
    }
}

// Save a file into a blob of UTF-8 text.
esp_err_t text_file_to_blob(const text_file_t* file, char** ptr_out, size_t* len_out) {
    size_t required_cap = 0;
    for (size_t i = 0; i < file->lines_len; i++) {
        required_cap += file->lines[i].data_len + (file->lines[i].line_end == LINE_END_CRLF ? 2 : 1);
    }
    char* data = malloc(required_cap);
    if (!data) {
        return ESP_ERR_NO_MEM;
    }
    size_t offset = 0;
    for (size_t i = 0; i < file->lines_len; i++) {
        memcpy(data + offset, file->lines[i].data, file->lines[i].data_len);
        offset += file->lines[i].data_len;
        switch (file->lines[i].line_end) {
            case LINE_END_CR:
                data[offset++] = '\r';
                break;
            case LINE_END_LF:
                data[offset++] = '\n';
                break;
            case LINE_END_CRLF:
                data[offset++] = '\r';
                data[offset++] = '\n';
                break;
        }
    }
    *ptr_out = data;
    *len_out = offset;
    return ESP_OK;
}

// Save a file to a handle.
esp_err_t text_file_save(const text_file_t* file, FILE* fd) {
    for (size_t i = 0; i < file->lines_len; i++) {
        size_t line_len = fwrite(file->lines[i].data, 1, file->lines[i].data_len, fd);
        if (line_len != file->lines[i].data_len) {
            return ESP_FAIL;
        }
        int len = 0, req_len = 1;
        switch (file->lines[i].line_end) {
            case LINE_END_CR:
                len = fputc('\r', fd);
                break;
            case LINE_END_LF:
                len = fputc('\n', fd);
                break;
            case LINE_END_CRLF:
                len     = fputs("\r\n", fd);
                req_len = 2;
                break;
        }
        if (len != req_len) {
            return ESP_FAIL;
        }
    }
    return ESP_OK;
}
