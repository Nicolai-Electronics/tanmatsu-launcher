#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef void (*download_callback_t)(size_t download_position, size_t file_size, const char* text);

typedef struct http_session* http_session_t;
http_session_t               http_session_begin(const char* initial_url);
void http_session_set_callback(http_session_t session, download_callback_t callback, const char* text);
bool http_session_download_ram(http_session_t session, const char* url, uint8_t** ptr, size_t* size);
bool http_session_download_file(http_session_t session, const char* url, const char* path);
void http_session_end(http_session_t session);
