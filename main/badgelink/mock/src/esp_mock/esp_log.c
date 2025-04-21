
// SPDX-Copyright-Text: 2025 Julian Scheffers
// SPDX-License-Identifier: MIT

#include "esp_log.h"
#include <stdarg.h>
#include <time.h>

static struct timespec timebase;

__attribute__((constructor)) static void get_timebase() {
    timespec_get(&timebase, TIME_UTC);
}

void mock__log(mock__loglevel_t level, char const* tag, char const* fmt, ...) {
    switch (level) {
        case MOCK__LOGLEVEL_INFO:
            fputs("\033[32m", stdout);
            break;
        case MOCK__LOGLEVEL_WARN:
            fputs("\033[33m", stdout);
            break;
        case MOCK__LOGLEVEL_ERROR:
            fputs("\033[31m", stdout);
            break;
        case MOCK__LOGLEVEL_DEBUG:
            fputs("\033[34m", stdout);
            break;
    }
    struct timespec now;
    timespec_get(&now, TIME_UTC);
    now.tv_sec  -= timebase.tv_sec;
    now.tv_nsec -= timebase.tv_nsec;
    if (now.tv_nsec < 0) {
        now.tv_nsec += 1000000000;
        now.tv_sec--;
    }
    printf("[%3d.%03d] ", (int)now.tv_sec, (int)(now.tv_nsec / 1000000));
    switch (level) {
        case MOCK__LOGLEVEL_INFO:
            fputs("INFO  ", stdout);
            break;
        case MOCK__LOGLEVEL_WARN:
            fputs("WARN  ", stdout);
            break;
        case MOCK__LOGLEVEL_ERROR:
            fputs("ERROR ", stdout);
            break;
        case MOCK__LOGLEVEL_DEBUG:
            fputs("DEBUG ", stdout);
            break;
    }
    va_list ls;
    va_start(ls, fmt);
    vprintf(fmt, ls);
    va_end(ls);
    fputs("\033[0m\n", stdout);
}
