
// SPDX-Copyright-Text: 2025 Julian Scheffers
// SPDX-License-Identifier: MIT

#include "freertos/FreeRTOS.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct {
    uint8_t data[31];
    uint8_t len;
} fragment_t;

extern void (*mock__thread_func)(void*);
extern FILE* mock__infd;

QueueHandle_t xQueueCreate(size_t cap, size_t ent_size) {
    return 1;
}

int xQueueSend(QueueHandle_t queue, void const* message, uint64_t delay) {
    __builtin_trap();
}

int xQueueReceive(QueueHandle_t queue, void* message, uint64_t delay) {
    fragment_t frag;
    while ((frag.len = fread(frag.data, 1, sizeof(frag.data), mock__infd)) == 0) {
        if (feof(mock__infd)) {
            exit(0);
        }
        usleep(100000);
    }
    memcpy(message, &frag, sizeof(frag));
    return 1;
}

void xTaskCreate(void (*main_func)(void*), char const* name, size_t stack_depth, void* arg, int prio,
                 TaskHandle_t* out_handle) {
    mock__thread_func = main_func;
}
