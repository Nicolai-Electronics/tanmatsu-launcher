#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef void (*download_callback_t)(size_t download_position, size_t file_size, const char* text);

bool download_file(const char* url, const char* path, download_callback_t callback, const char* callback_text);
bool download_ram(const char* url, uint8_t** ptr, size_t* size, download_callback_t callback,
                  const char* callback_text);

// Session-based downloads: reuse a single TCP/TLS connection for multiple requests to the same server.
typedef struct http_session* http_session_t;
http_session_t http_session_begin(const char* initial_url);
bool           http_session_download_ram(http_session_t session, const char* url, uint8_t** ptr, size_t* size);
void           http_session_end(http_session_t session);
