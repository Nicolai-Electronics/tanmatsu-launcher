
// SPDX-Copyright-Text: 2025 Julian Scheffers
// SPDX-License-Identifier: MIT

#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef pthread_t TaskHandle_t;
typedef int       QueueHandle_t;

#define portMAX_DELAY UINT64_MAX
#define pdTRUE        1
#define pdFALSE       0

QueueHandle_t xQueueCreate(size_t cap, size_t ent_size);
int           xQueueReceive(QueueHandle_t queue, void* message, uint64_t delay);
int           xQueueSend(QueueHandle_t queue, void const* message, uint64_t delay);

void xTaskCreate(void (*main_func)(void*), char const* name, size_t stack_depth, void* arg, int prio,
                 TaskHandle_t* out_handle);
