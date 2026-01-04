// SPDX-License-Identifier: MIT
// Tanmatsu Plugin Context Structure
// Internal structure passed to plugins for state management.

#pragma once

#include "tanmatsu_plugin.h"

#ifdef __cplusplus
extern "C" {
#endif

// Forward declare FreeRTOS types to avoid pulling in full headers
struct tskTaskControlBlock;
typedef struct tskTaskControlBlock* TaskHandle_t;

// Plugin context structure (opaque to plugins, defined here for host implementation)
struct plugin_context {
    // Plugin identification
    char* plugin_path;          // Full path to plugin directory
    char* plugin_slug;          // Plugin unique identifier

    // State management
    plugin_state_t state;

    // Loaded ELF info (kbelf handles)
    void* elf_handle;           // kbelf_dyn handle
    void* entrypoint;           // Resolved entry point address

    // Plugin registration data (from loaded ELF)
    const plugin_registration_t* registration;
    const plugin_info_t* info;

    // Storage sandbox base path
    char* storage_base_path;

    // Settings namespace (prefixed with plugin slug)
    char* settings_namespace;

    // Service task handle (for PLUGIN_TYPE_SERVICE)
    void* task_handle;

    // Static task allocation - we own this memory so no cleanup race with FreeRTOS
    void* task_stack;           // Stack buffer (StackType_t*) allocated by us
    void* task_tcb;             // StaticTask_t allocated by us

    // Service stop control
    volatile bool stop_requested;   // Set by stop_service to signal shutdown
    volatile bool task_running;     // Set by task to indicate it's still running

    // Status widget ID (if registered)
    int status_widget_id;

    // Private data for plugin use
    void* user_data;
};

// Helper macros for plugin context access
#define PLUGIN_CTX_GET_PATH(ctx)      ((ctx)->plugin_path)
#define PLUGIN_CTX_GET_SLUG(ctx)      ((ctx)->plugin_slug)
#define PLUGIN_CTX_GET_STATE(ctx)     ((ctx)->state)
#define PLUGIN_CTX_GET_INFO(ctx)      ((ctx)->info)
#define PLUGIN_CTX_GET_USER_DATA(ctx) ((ctx)->user_data)
#define PLUGIN_CTX_SET_USER_DATA(ctx, data) ((ctx)->user_data = (data))

#ifdef __cplusplus
}
#endif
