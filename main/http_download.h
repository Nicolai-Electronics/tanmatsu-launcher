#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef void (*download_callback_t)(size_t download_position, size_t file_size, const char* text);

// Coarse classification of the outcome of the last request made on a session, used to give
// callers (and ultimately the UI) a more specific reason for a failure than a plain boolean.
typedef enum {
    HTTP_RESULT_OK = 0,
    HTTP_RESULT_TIMEOUT,       // Could not connect to the server, or it did not respond in time
    HTTP_RESULT_CLIENT_ERROR,  // Server responded with a 4xx status code
    HTTP_RESULT_SERVER_ERROR,  // Server responded with a 5xx status code
    HTTP_RESULT_OTHER,         // Any other failure (DNS, out of memory, malformed response, ...)
} http_result_t;

typedef struct http_session* http_session_t;
http_session_t               http_session_begin(const char* initial_url);
void http_session_set_callback(http_session_t session, download_callback_t callback, const char* text);
bool http_session_download_ram(http_session_t session, const char* url, uint8_t** ptr, size_t* size);
bool http_session_post_ram(http_session_t session, const char* url, const char* content_type, const char* body,
                           size_t body_length, uint8_t** ptr, size_t* size);
bool http_session_download_file(http_session_t session, const char* url, const char* path);
// Classification of the outcome of the most recent download/post made on this session.
http_result_t http_session_get_last_result(http_session_t session);
void http_session_end(http_session_t session);
