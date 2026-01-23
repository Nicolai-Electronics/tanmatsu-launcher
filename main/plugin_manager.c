// SPDX-License-Identifier: MIT
// Tanmatsu Plugin Manager Implementation
// Handles plugin discovery, loading, unloading, and lifecycle management.

#include "plugin_manager.h"
#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>

#include "esp_log.h"
#include "esp_heap_caps.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "cJSON.h"
#define KBELF_REVEAL_PRIVATE
#include "kbelf.h"
#include "fastopen.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "hal/cache_hal.h"
#include "soc/soc.h"

static const char* TAG = "plugin_mgr";

// Plugin search paths
static const char* PLUGIN_PATHS[] = {
    "/int/plugins",
    "/sd/plugins",
    NULL
};

// NVS namespace for plugin settings
#define PLUGIN_NVS_NAMESPACE "plugins"

// Loaded plugins registry
static plugin_context_t* loaded_plugins[PLUGIN_MAX_LOADED] = {0};
static size_t loaded_plugin_count = 0;
static SemaphoreHandle_t plugin_mutex = NULL;

// External functions from plugin_api.c
extern size_t plugin_api_get_status_widgets(plugin_icontext_t* out, size_t max, int start_x, int start_y);
extern int plugin_api_dispatch_event(uint32_t event_type, void* event_data);
extern void plugin_api_init(void);
extern void plugin_api_cleanup_for_plugin(plugin_context_t* ctx);

// Forward declaration of internal unload function
static bool _plugin_manager_unload(plugin_context_t* ctx);

// ============================================
// Plugin Manager Lifecycle
// ============================================

bool plugin_manager_init(void) {
    ESP_LOGI(TAG, "Initializing plugin manager");

    plugin_mutex = xSemaphoreCreateMutex();
    if (plugin_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return false;
    }

    // Initialize plugin API (creates mutex for registry protection)
    plugin_api_init();

    // Create plugin directories if they don't exist
    for (const char** path = PLUGIN_PATHS; *path != NULL; path++) {
        struct stat st;
        if (stat(*path, &st) != 0) {
            if (mkdir(*path, 0755) == 0) {
                ESP_LOGI(TAG, "Created plugin directory: %s", *path);
            }
        }
    }

    return true;
}

void plugin_manager_shutdown(void) {
    ESP_LOGI(TAG, "Shutting down plugin manager");

    if (plugin_mutex == NULL) return;

    xSemaphoreTake(plugin_mutex, portMAX_DELAY);

    // Unload all plugins (iterate backwards since _plugin_manager_unload modifies the array)
    while (loaded_plugin_count > 0) {
        _plugin_manager_unload(loaded_plugins[0]);
    }

    xSemaphoreGive(plugin_mutex);
    vSemaphoreDelete(plugin_mutex);
    plugin_mutex = NULL;
}

// ============================================
// Plugin Metadata Parsing
// ============================================

static bool parse_plugin_metadata(const char* path, plugin_discovery_info_t* info) {
    char metadata_path[256];
    snprintf(metadata_path, sizeof(metadata_path), "%s/plugin.json", path);

    FILE* fd = fastopen(metadata_path, "r");
    if (fd == NULL) {
        return false;
    }

    fseek(fd, 0, SEEK_END);
    size_t size = ftell(fd);
    fseek(fd, 0, SEEK_SET);

    if (size == 0 || size > 8192) {
        fastclose(fd);  // Must use fastclose to free DMA buffer
        return false;
    }

    char* json_data = malloc(size + 1);
    if (json_data == NULL) {
        fastclose(fd);  // Must use fastclose to free DMA buffer
        return false;
    }

    size_t read = fread(json_data, 1, size, fd);
    json_data[read] = '\0';
    fastclose(fd);  // Must use fastclose to free DMA buffer

    cJSON* root = cJSON_Parse(json_data);
    free(json_data);

    if (root == NULL) {
        ESP_LOGW(TAG, "Failed to parse plugin.json at %s", metadata_path);
        return false;
    }

    // Extract metadata
    cJSON* name = cJSON_GetObjectItem(root, "name");
    cJSON* slug = cJSON_GetObjectItem(root, "slug");
    cJSON* version = cJSON_GetObjectItem(root, "version");
    cJSON* type = cJSON_GetObjectItem(root, "type");

    if (!cJSON_IsString(name) || !cJSON_IsString(slug)) {
        cJSON_Delete(root);
        return false;
    }

    info->path = strdup(path);
    info->slug = strdup(slug->valuestring);
    info->name = strdup(name->valuestring);
    info->version = version && cJSON_IsString(version) ? strdup(version->valuestring) : strdup("1.0.0");
    info->type = PLUGIN_TYPE_MENU;  // Default

    // Parse type
    if (type && cJSON_IsString(type)) {
        if (strcmp(type->valuestring, "menu") == 0) {
            info->type = PLUGIN_TYPE_MENU;
        } else if (strcmp(type->valuestring, "service") == 0) {
            info->type = PLUGIN_TYPE_SERVICE;
        } else if (strcmp(type->valuestring, "hook") == 0) {
            info->type = PLUGIN_TYPE_HOOK;
        }
    }

    cJSON_Delete(root);
    return true;
}

// ============================================
// Plugin Discovery
// ============================================

size_t plugin_manager_discover(plugin_discovery_info_t** out_plugins) {
    // Memory debugging (commented out)
    // size_t internal_before = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    // ESP_LOGI(TAG, "Discovering plugins... (internal before: %u)", (unsigned)internal_before);

    // Allocate discovery list
    plugin_discovery_info_t* plugins = calloc(PLUGIN_MAX_LOADED,
                                               sizeof(plugin_discovery_info_t));
    if (plugins == NULL) {
        *out_plugins = NULL;
        return 0;
    }

    size_t count = 0;

    for (const char** base_path = PLUGIN_PATHS; *base_path != NULL; base_path++) {
        DIR* dir = opendir(*base_path);
        if (dir == NULL) continue;

        struct dirent* entry;
        while ((entry = readdir(dir)) != NULL && count < PLUGIN_MAX_LOADED) {
            if (entry->d_type != DT_DIR) continue;
            if (entry->d_name[0] == '.') continue;

            char plugin_path[512];
            snprintf(plugin_path, sizeof(plugin_path), "%s/%s",
                     *base_path, entry->d_name);

            if (parse_plugin_metadata(plugin_path, &plugins[count])) {
                // Check if already loaded
                plugins[count].is_loaded =
                    (plugin_manager_get_by_slug(plugins[count].slug) != NULL);

                ESP_LOGI(TAG, "Found plugin: %s (%s) at %s",
                         plugins[count].name, plugins[count].slug, plugin_path);
                count++;
            }
        }

        closedir(dir);
    }

    // Memory debugging (commented out)
    // size_t internal_after = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    // ESP_LOGI(TAG, "Discovery complete: %zu plugins, internal used: %u",
    //          count, (unsigned)(internal_before - internal_after));

    *out_plugins = plugins;
    return count;
}

void plugin_manager_free_discovery(plugin_discovery_info_t* plugins, size_t count) {
    if (plugins == NULL) return;

    // Memory debugging (commented out)
    // size_t internal_before = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);

    for (size_t i = 0; i < count; i++) {
        free(plugins[i].path);
        free(plugins[i].slug);
        free(plugins[i].name);
        free(plugins[i].version);
    }
    free(plugins);

    // Memory debugging (commented out)
    // size_t internal_after = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    // ESP_LOGI(TAG, "Discovery freed: internal recovered: %u",
    //          (unsigned)(internal_after - internal_before));
}

// ============================================
// Plugin Loading
// ============================================

static char* find_plugin_elf(const char* plugin_path) {
    // Look for .plugin file in the directory
    DIR* dir = opendir(plugin_path);
    if (dir == NULL) return NULL;

    char* result = NULL;
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        size_t len = strlen(entry->d_name);
        if (len > 7 && strcmp(entry->d_name + len - 7, ".plugin") == 0) {
            result = malloc(strlen(plugin_path) + strlen(entry->d_name) + 2);
            if (result) {
                sprintf(result, "%s/%s", plugin_path, entry->d_name);
            }
            break;
        }
    }
    closedir(dir);

    // Fallback to plugin.plugin
    if (result == NULL) {
        result = malloc(strlen(plugin_path) + 16);
        if (result) {
            sprintf(result, "%s/plugin.plugin", plugin_path);
            struct stat st;
            if (stat(result, &st) != 0) {
                free(result);
                result = NULL;
            }
        }
    }

    return result;
}

plugin_context_t* plugin_manager_load(const char* plugin_path) {
    ESP_LOGI(TAG, "Loading plugin from: %s", plugin_path);

    // Memory debugging (commented out)
    // size_t free_heap = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
    // size_t largest_block = heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT);
    // size_t free_internal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    // size_t largest_internal = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
    // ESP_LOGI(TAG, "Heap before load: %u free (largest: %u), INTERNAL: %u free (largest: %u)",
    //          (unsigned)free_heap, (unsigned)largest_block,
    //          (unsigned)free_internal, (unsigned)largest_internal);
    // if (!heap_caps_check_integrity_all(true)) {
    //     ESP_LOGE(TAG, "HEAP CORRUPTED at start of plugin_manager_load!");
    // }

    xSemaphoreTake(plugin_mutex, portMAX_DELAY);

    if (loaded_plugin_count >= PLUGIN_MAX_LOADED) {
        ESP_LOGE(TAG, "Maximum plugin limit reached");
        xSemaphoreGive(plugin_mutex);
        return NULL;
    }

    // Parse metadata first
    plugin_discovery_info_t discovery_info = {0};
    if (!parse_plugin_metadata(plugin_path, &discovery_info)) {
        ESP_LOGE(TAG, "Failed to parse plugin metadata");
        xSemaphoreGive(plugin_mutex);
        return NULL;
    }

    // Check if already loaded
    if (plugin_manager_get_by_slug(discovery_info.slug) != NULL) {
        ESP_LOGW(TAG, "Plugin %s is already loaded", discovery_info.slug);
        free(discovery_info.path);
        free(discovery_info.slug);
        free(discovery_info.name);
        free(discovery_info.version);
        xSemaphoreGive(plugin_mutex);
        return NULL;
    }

    // Find plugin ELF file
    char* elf_path = find_plugin_elf(plugin_path);
    if (elf_path == NULL) {
        ESP_LOGE(TAG, "No .plugin file found in %s", plugin_path);
        free(discovery_info.path);
        free(discovery_info.slug);
        free(discovery_info.name);
        free(discovery_info.version);
        xSemaphoreGive(plugin_mutex);
        return NULL;
    }

    ESP_LOGI(TAG, "Loading ELF: %s", elf_path);

    // Create plugin context
    plugin_context_t* ctx = calloc(1, sizeof(plugin_context_t));
    if (ctx == NULL) {
        ESP_LOGE(TAG, "Failed to allocate plugin context");
        free(elf_path);
        free(discovery_info.path);
        free(discovery_info.slug);
        free(discovery_info.name);
        free(discovery_info.version);
        xSemaphoreGive(plugin_mutex);
        return NULL;
    }

    ctx->plugin_path = strdup(plugin_path);
    ctx->plugin_slug = discovery_info.slug;
    ctx->storage_base_path = strdup(plugin_path);
    ctx->settings_namespace = malloc(strlen("plugin_") + strlen(discovery_info.slug) + 1);
    if (ctx->settings_namespace) {
        sprintf(ctx->settings_namespace, "plugin_%s", discovery_info.slug);
    }
    ctx->state = PLUGIN_STATE_UNLOADED;
    ctx->status_widget_id = -1;

    // Free unused discovery fields
    free(discovery_info.path);
    free(discovery_info.name);
    free(discovery_info.version);

    // Load ELF using kbelf
    // Memory debugging (commented out)
    // size_t internal_before_create = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);

    kbelf_dyn dyn = kbelf_dyn_create(0);
    if (!dyn) {
        ESP_LOGE(TAG, "Failed to create kbelf context");
        goto error_cleanup;
    }

    // Memory debugging (commented out)
    // size_t internal_after_create = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    // ESP_LOGI(TAG, "After kbelf_dyn_create: internal used %u",
    //          (unsigned)(internal_before_create - internal_after_create));

    if (!kbelf_dyn_set_exec(dyn, elf_path, NULL)) {
        ESP_LOGE(TAG, "Failed to set executable: %s", elf_path);
        kbelf_dyn_destroy(dyn);
        goto error_cleanup;
    }

    // Memory debugging (commented out)
    // size_t internal_after_setexec = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    // ESP_LOGI(TAG, "After kbelf_dyn_set_exec: internal used %u (total so far: %u)",
    //          (unsigned)(internal_after_create - internal_after_setexec),
    //          (unsigned)(internal_before_create - internal_after_setexec));

    if (!kbelf_dyn_load(dyn)) {
        ESP_LOGE(TAG, "Failed to load plugin ELF");
        kbelf_dyn_destroy(dyn);
        goto error_cleanup;
    }

    // Memory debugging (commented out)
    // size_t internal_after_load = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    // ESP_LOGI(TAG, "After kbelf_dyn_load: internal used %u (total so far: %u)",
    //          (unsigned)(internal_after_setexec - internal_after_load),
    //          (unsigned)(internal_before_create - internal_after_load));
    // if (!heap_caps_check_integrity_all(true)) {
    //     ESP_LOGE(TAG, "HEAP CORRUPTED after kbelf_dyn_load!");
    // }

    free(elf_path);
    elf_path = NULL;  // Prevent double-free if we goto error_cleanup later
    ctx->elf_handle = dyn;
    ctx->state = PLUGIN_STATE_LOADED;

#if SOC_CACHE_WRITEBACK_SUPPORTED
    // Sync caches
    cache_hal_writeback_addr(SOC_DRAM_LOW, SOC_DRAM_HIGH - SOC_DRAM_LOW);
#endif

    // Run preinit and init functions
    size_t preinit_count = kbelf_dyn_preinit_len(dyn);
    ESP_LOGI(TAG, "Running %zu preinit functions", preinit_count);
    for (size_t i = 0; i < preinit_count; i++) {
        void (*func)(void) = (void*)kbelf_dyn_preinit_get(dyn, i);
        func();
    }

    size_t init_count = kbelf_dyn_init_len(dyn);
    ESP_LOGI(TAG, "Running %zu init functions", init_count);
    for (size_t i = 0; i < init_count; i++) {
        void (*func)(void) = (void*)kbelf_dyn_init_get(dyn, i);
        func();
    }

    // Find plugin registration
    // The _plugin_registration is placed in .plugin_info section at VMA 0
    kbelf_addr reg_addr = kbelf_inst_getvaddr(dyn->exec_inst, 0);
    if (reg_addr != 0) {
        plugin_registration_t* reg = (plugin_registration_t*)reg_addr;

        // Validate the registration
        if (reg->magic == TANMATSU_PLUGIN_MAGIC) {
            ctx->registration = reg;
            ESP_LOGI(TAG, "Found plugin registration at %p", (void*)reg_addr);

            // Memory debugging (commented out)
            // if (!heap_caps_check_integrity_all(true)) {
            //     ESP_LOGE(TAG, "HEAP CORRUPTED before plugin init!");
            // }

            // Call the plugin's init function if available
            if (reg->entry.init != NULL) {
                int init_result = reg->entry.init(ctx);

                // Memory debugging (commented out)
                // if (!heap_caps_check_integrity_all(true)) {
                //     ESP_LOGE(TAG, "HEAP CORRUPTED after plugin init!");
                // }

                if (init_result != 0) {
                    ESP_LOGE(TAG, "Plugin init failed with code %d", init_result);
                    goto error_cleanup;
                }
            }
        } else {
            ESP_LOGE(TAG, "Invalid plugin magic: 0x%08lx (expected 0x%08x)",
                     (unsigned long)reg->magic, TANMATSU_PLUGIN_MAGIC);
            goto error_cleanup;
        }
    } else {
        ESP_LOGE(TAG, "Could not find plugin registration");
        goto error_cleanup;
    }

    // Add to loaded plugins
    loaded_plugins[loaded_plugin_count++] = ctx;
    ctx->state = PLUGIN_STATE_INITIALIZED;

    ESP_LOGI(TAG, "Plugin loaded successfully: %s", ctx->plugin_slug);

    xSemaphoreGive(plugin_mutex);
    return ctx;

error_cleanup:
    // Memory debugging (commented out)
    // ESP_LOGE(TAG, "error_cleanup: checking heap before cleanup");
    // if (!heap_caps_check_integrity_all(true)) {
    //     ESP_LOGE(TAG, "HEAP CORRUPTED at start of error_cleanup!");
    // }

    // Clean up ELF if it was loaded
    if (ctx && ctx->elf_handle) {
        kbelf_dyn dyn = (kbelf_dyn)ctx->elf_handle;
        kbelf_dyn_unload(dyn);
        kbelf_dyn_destroy(dyn);
        ctx->elf_handle = NULL;
    }

    free(elf_path);

    if (ctx) {
        free(ctx->plugin_path);
        free(ctx->plugin_slug);
        free(ctx->storage_base_path);
        free(ctx->settings_namespace);
        free(ctx);
    }
    xSemaphoreGive(plugin_mutex);
    return NULL;
}

// Internal unload - caller must hold plugin_mutex
static bool _plugin_manager_unload(plugin_context_t* ctx) {
    if (ctx == NULL) return false;

    ESP_LOGI(TAG, "Unloading plugin: %s", ctx->plugin_slug);

    // Memory debugging (commented out)
    // size_t heap_before = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
    // size_t internal_before = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    // ESP_LOGI(TAG, "Unloading plugin: %s (heap: %u, internal: %u)",
    //          ctx->plugin_slug, (unsigned)heap_before, (unsigned)internal_before);
    // if (!heap_caps_check_integrity_all(true)) {
    //     ESP_LOGE(TAG, "HEAP CORRUPTED at start of unload for %s!", ctx->plugin_slug);
    // }

    // Stop service if running
    if (ctx->state == PLUGIN_STATE_RUNNING) {
        plugin_manager_stop_service(ctx);
    }

    // Automatically clean up any API registrations (widgets, hooks, events)
    // that the plugin may have forgotten to unregister
    plugin_api_cleanup_for_plugin(ctx);

    // Call cleanup if available (plugin may also try to unregister, which is fine)
    if (ctx->registration && ctx->registration->entry.cleanup) {
        ctx->registration->entry.cleanup(ctx);
    }

    // Run fini functions
    kbelf_dyn dyn = (kbelf_dyn)ctx->elf_handle;
    if (dyn) {
        size_t fini_count = kbelf_dyn_fini_len(dyn);
        ESP_LOGI(TAG, "Running %zu fini functions", fini_count);
        for (size_t i = 0; i < fini_count; i++) {
            void (*func)(void) = (void*)kbelf_dyn_fini_get(dyn, i);
            func();
        }

        // Memory debugging (commented out)
        // size_t internal_before_unload = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        // ESP_LOGI(TAG, "Before kbelf_dyn_unload: internal %u", (unsigned)internal_before_unload);

        kbelf_dyn_unload(dyn);

        // Memory debugging (commented out)
        // size_t internal_after_unload = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        // ESP_LOGI(TAG, "After kbelf_dyn_unload: internal freed %u",
        //          (unsigned)(internal_after_unload - internal_before_unload));

        kbelf_dyn_destroy(dyn);

        // Memory debugging (commented out)
        // size_t internal_after_destroy = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        // ESP_LOGI(TAG, "After kbelf_dyn_destroy: internal freed %u (total kbelf freed: %u)",
        //          (unsigned)(internal_after_destroy - internal_after_unload),
        //          (unsigned)(internal_after_destroy - internal_before_unload));
    }

    // Remove from loaded plugins
    for (size_t i = 0; i < loaded_plugin_count; i++) {
        if (loaded_plugins[i] == ctx) {
            // Shift remaining plugins
            for (size_t j = i; j < loaded_plugin_count - 1; j++) {
                loaded_plugins[j] = loaded_plugins[j + 1];
            }
            loaded_plugins[--loaded_plugin_count] = NULL;
            break;
        }
    }

    // Free task memory if not already freed (e.g., task stopped on its own)
    free(ctx->task_stack);
    free(ctx->task_tcb);

    // Free context
    free(ctx->plugin_path);
    free(ctx->plugin_slug);
    free(ctx->storage_base_path);
    free(ctx->settings_namespace);
    free(ctx);

    // Memory debugging (commented out)
    // if (!heap_caps_check_integrity_all(true)) {
    //     ESP_LOGE(TAG, "HEAP CORRUPTED at end of unload!");
    // }
    // size_t heap_after = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
    // size_t largest_block = heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT);
    // size_t internal_after = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    // size_t largest_internal = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
    // ESP_LOGI(TAG, "Heap after unload: %u free (largest: %u), INTERNAL: %u free (largest: %u)",
    //          (unsigned)heap_after, (unsigned)largest_block,
    //          (unsigned)internal_after, (unsigned)largest_internal);
    // if (heap_after < heap_before) {
    //     ESP_LOGW(TAG, "MEMORY LEAK: %u bytes not freed after unload!",
    //              (unsigned)(heap_before - heap_after));
    // }

    return true;
}

bool plugin_manager_unload(plugin_context_t* ctx) {
    if (ctx == NULL) return false;

    xSemaphoreTake(plugin_mutex, portMAX_DELAY);
    bool result = _plugin_manager_unload(ctx);
    xSemaphoreGive(plugin_mutex);

    return result;
}

// ============================================
// Plugin Query
// ============================================

plugin_context_t* plugin_manager_get_by_slug(const char* slug) {
    if (slug == NULL) return NULL;

    for (size_t i = 0; i < loaded_plugin_count; i++) {
        if (loaded_plugins[i] != NULL &&
            loaded_plugins[i]->plugin_slug != NULL &&
            strcmp(loaded_plugins[i]->plugin_slug, slug) == 0) {
            return loaded_plugins[i];
        }
    }
    return NULL;
}

size_t plugin_manager_get_by_type(plugin_type_t type,
                                   plugin_context_t** out_plugins,
                                   size_t max_count) {
    size_t count = 0;
    for (size_t i = 0; i < loaded_plugin_count && count < max_count; i++) {
        if (loaded_plugins[i] != NULL &&
            loaded_plugins[i]->info != NULL &&
            loaded_plugins[i]->info->type == type) {
            out_plugins[count++] = loaded_plugins[i];
        }
    }
    return count;
}

size_t plugin_manager_get_loaded_count(void) {
    return loaded_plugin_count;
}

// ============================================
// Service Plugin Management
// ============================================

// Deferred unload task - runs with sufficient stack for plugin cleanup
static void deferred_unload_task(void* arg) {
    plugin_context_t* ctx = (plugin_context_t*)arg;
    if (ctx) {
        // Small delay to ensure service task has fully exited
        vTaskDelay(pdMS_TO_TICKS(50));
        ESP_LOGI(TAG, "Auto-unloading service plugin: %s", ctx->plugin_slug);
        plugin_manager_unload(ctx);
    }
    vTaskDelete(NULL);
}

static void plugin_service_task(void* arg) {
    plugin_context_t* ctx = (plugin_context_t*)arg;

    ESP_LOGI(TAG, "Service task started for plugin: %s", ctx->plugin_slug);
    ctx->task_running = true;

    if (ctx->registration && ctx->registration->entry.service_run) {
        ctx->registration->entry.service_run(ctx);
    }

    ESP_LOGI(TAG, "Service task ended for plugin: %s", ctx->plugin_slug);

    // Clear handle BEFORE marking as not running to prevent race with stop_service()
    // This ensures stop_service() won't try to delete an already-deleting task
    ctx->task_handle = NULL;
    ctx->task_running = false;
    ctx->state = PLUGIN_STATE_STOPPED;

    // Create a small task to handle deferred unload with sufficient stack
    // This runs after the service task exits, so it's safe to unload the plugin
    BaseType_t ret = xTaskCreate(deferred_unload_task, "unload", 4096, ctx, 5, NULL);
    if (ret != pdPASS) {
        ESP_LOGW(TAG, "Failed to create deferred unload task for plugin: %s", ctx->plugin_slug);
    }

    // FreeRTOS idiom: vTaskDelete(NULL) deletes the calling task
    vTaskDelete(NULL);
}

// Stack size for service tasks (in StackType_t units, typically 4 bytes each)
#define SERVICE_TASK_STACK_SIZE 8192

bool plugin_manager_start_service(plugin_context_t* ctx) {
    if (ctx == NULL || ctx->state == PLUGIN_STATE_RUNNING) {
        return false;
    }

    ESP_LOGI(TAG, "Starting service plugin: %s", ctx->plugin_slug);

    // Memory debugging (commented out)
    // size_t internal_before = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);

    // Initialize control flags
    ctx->stop_requested = false;
    ctx->task_running = false;

    // Allocate task stack and TCB - we own this memory, no cleanup race with FreeRTOS
    ctx->task_stack = malloc(SERVICE_TASK_STACK_SIZE * sizeof(StackType_t));
    if (ctx->task_stack == NULL) {
        ESP_LOGE(TAG, "Failed to allocate task stack");
        return false;
    }

    // Memory debugging (commented out)
    // size_t internal_after_stack = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    // ESP_LOGI(TAG, "Task stack allocated: %u bytes at %p (internal used: %u)",
    //          (unsigned)(SERVICE_TASK_STACK_SIZE * sizeof(StackType_t)),
    //          ctx->task_stack,
    //          (unsigned)(internal_before - internal_after_stack));

    ctx->task_tcb = malloc(sizeof(StaticTask_t));
    if (ctx->task_tcb == NULL) {
        ESP_LOGE(TAG, "Failed to allocate task TCB");
        free(ctx->task_stack);
        ctx->task_stack = NULL;
        return false;
    }

    // Memory debugging (commented out)
    // size_t internal_after_tcb = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    // ESP_LOGI(TAG, "Task TCB allocated: %u bytes (internal used: %u)",
    //          (unsigned)sizeof(StaticTask_t),
    //          (unsigned)(internal_after_stack - internal_after_tcb));

    // Use static task creation - FreeRTOS won't free our memory
    TaskHandle_t task = xTaskCreateStatic(
        plugin_service_task,
        ctx->plugin_slug,
        SERVICE_TASK_STACK_SIZE,
        ctx,
        5,     // Priority
        (StackType_t*)ctx->task_stack,
        (StaticTask_t*)ctx->task_tcb
    );

    if (task == NULL) {
        ESP_LOGE(TAG, "Failed to create service task");
        free(ctx->task_tcb);
        free(ctx->task_stack);
        ctx->task_tcb = NULL;
        ctx->task_stack = NULL;
        return false;
    }

    ctx->task_handle = task;
    ctx->state = PLUGIN_STATE_RUNNING;

    return true;
}

bool plugin_manager_stop_service(plugin_context_t* ctx) {
    if (ctx == NULL) {
        return false;
    }

    // Check if already stopped (task may have ended on its own)
    if (ctx->state != PLUGIN_STATE_RUNNING && !ctx->task_running) {
        ESP_LOGI(TAG, "Service plugin already stopped: %s", ctx->plugin_slug);
        // Still need to free stack/TCB if task ended on its own
        goto cleanup_task_memory;
    }

    ESP_LOGI(TAG, "Stopping service plugin: %s", ctx->plugin_slug);

    // Signal the plugin to stop
    ctx->stop_requested = true;

    // Wait for the task to finish (with timeout)
    int timeout_ms = 5000;  // 5 second timeout
    while (ctx->task_running && timeout_ms > 0) {
        vTaskDelay(pdMS_TO_TICKS(100));
        timeout_ms -= 100;
    }

    if (ctx->task_running) {
        // Task didn't stop gracefully, force delete it
        ESP_LOGW(TAG, "Force stopping service plugin: %s", ctx->plugin_slug);

        // Atomic read-and-clear to prevent race with task self-deletion
        TaskHandle_t task = (TaskHandle_t)ctx->task_handle;
        ctx->task_handle = NULL;  // Clear immediately to prevent double-delete

        if (task != NULL) {
            vTaskDelete(task);
        }

        ctx->task_running = false;
    }

cleanup_task_memory:
    // Free task stack and TCB that we allocated
    // Since we used xTaskCreateStatic(), FreeRTOS doesn't touch this memory
    // after vTaskDelete() - we own it and can free it immediately

    // Memory debugging (commented out)
    // size_t internal_before_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    // ESP_LOGI(TAG, "Freeing task memory (internal before: %u)", (unsigned)internal_before_free);

    if (ctx->task_stack) {
        free(ctx->task_stack);
        ctx->task_stack = NULL;
    }

    // Memory debugging (commented out)
    // size_t internal_after_stack_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    // ESP_LOGI(TAG, "After stack free: internal freed %u",
    //          (unsigned)(internal_after_stack_free - internal_before_free));

    if (ctx->task_tcb) {
        free(ctx->task_tcb);
        ctx->task_tcb = NULL;
    }

    // Memory debugging (commented out)
    // size_t internal_after_tcb_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    // ESP_LOGI(TAG, "After TCB free: internal freed %u (total freed: %u)",
    //          (unsigned)(internal_after_tcb_free - internal_after_stack_free),
    //          (unsigned)(internal_after_tcb_free - internal_before_free));

    ctx->state = PLUGIN_STATE_STOPPED;
    ctx->stop_requested = false;

    return true;
}

// ============================================
// Event Dispatch
// ============================================

int plugin_manager_dispatch_event(uint32_t event_type, void* event_data) {
    return plugin_api_dispatch_event(event_type, event_data);
}

// ============================================
// Autostart Management
// ============================================

// Generate a short hash key from slug (NVS keys limited to 15 chars)
static void make_autostart_key(const char* slug, char* out_key, size_t out_len) {
    // Simple hash to create a short key: "a_" + 8 hex chars from hash
    uint32_t hash = 5381;
    for (const char* p = slug; *p; p++) {
        hash = ((hash << 5) + hash) + (uint8_t)*p;
    }
    snprintf(out_key, out_len, "a_%08lx", (unsigned long)hash);
}

bool plugin_manager_set_autostart(const char* slug, bool enabled) {
    if (slug == NULL) return false;

    nvs_handle_t handle;
    esp_err_t err = nvs_open(PLUGIN_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS for autostart: %s", esp_err_to_name(err));
        return false;
    }

    char key[16];  // NVS keys max 15 chars + null
    make_autostart_key(slug, key, sizeof(key));

    err = nvs_set_u8(handle, key, enabled ? 1 : 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set autostart key %s: %s", key, esp_err_to_name(err));
    }
    if (err == ESP_OK) {
        err = nvs_commit(handle);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to commit NVS: %s", esp_err_to_name(err));
        }
    }
    nvs_close(handle);

    return err == ESP_OK;
}

// Helper to read autostart default from plugin.json
static bool get_autostart_from_json(const char* plugin_path) {
    char metadata_path[256];
    snprintf(metadata_path, sizeof(metadata_path), "%s/plugin.json", plugin_path);

    FILE* fd = fopen(metadata_path, "r");
    if (fd == NULL) return false;

    fseek(fd, 0, SEEK_END);
    long size = ftell(fd);
    fseek(fd, 0, SEEK_SET);

    if (size <= 0 || size > 4096) {
        fclose(fd);
        return false;
    }

    char* json_data = malloc(size + 1);
    if (json_data == NULL) {
        fclose(fd);
        return false;
    }

    size_t read = fread(json_data, 1, size, fd);
    json_data[read] = '\0';
    fclose(fd);

    cJSON* root = cJSON_Parse(json_data);
    free(json_data);
    if (root == NULL) return false;

    cJSON* autostart = cJSON_GetObjectItem(root, "autostart");
    bool result = autostart && cJSON_IsBool(autostart) && cJSON_IsTrue(autostart);
    cJSON_Delete(root);

    return result;
}

bool plugin_manager_get_autostart(const char* slug) {
    if (slug == NULL) return false;

    // First check NVS for user override
    nvs_handle_t handle;
    esp_err_t err = nvs_open(PLUGIN_NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err == ESP_OK) {
        char key[16];
        make_autostart_key(slug, key, sizeof(key));

        uint8_t value = 0;
        err = nvs_get_u8(handle, key, &value);
        nvs_close(handle);

        if (err == ESP_OK) {
            // User has explicitly set autostart
            return value != 0;
        }
    }

    // No user override - check plugin.json default
    // Need to find the plugin path from slug
    plugin_discovery_info_t* plugins = NULL;
    size_t count = plugin_manager_discover(&plugins);

    bool result = false;
    for (size_t i = 0; i < count; i++) {
        if (plugins[i].slug && strcmp(plugins[i].slug, slug) == 0) {
            result = get_autostart_from_json(plugins[i].path);
            break;
        }
    }

    plugin_manager_free_discovery(plugins, count);
    return result;
}

bool plugin_manager_has_running_services(void) {
    for (size_t i = 0; i < loaded_plugin_count; i++) {
        if (loaded_plugins[i] && loaded_plugins[i]->state == PLUGIN_STATE_RUNNING) {
            return true;
        }
    }
    return false;
}

// Helper to check autostart for a specific plugin (with path already known)
static bool check_autostart_for_plugin(const char* slug, const char* path) {
    // First check NVS for user override
    nvs_handle_t handle;
    esp_err_t err = nvs_open(PLUGIN_NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err == ESP_OK) {
        char key[16];
        make_autostart_key(slug, key, sizeof(key));

        uint8_t value = 0;
        err = nvs_get_u8(handle, key, &value);
        nvs_close(handle);

        if (err == ESP_OK) {
            // User has explicitly set autostart
            return value != 0;
        }
    }

    // No user override - check plugin.json default
    return get_autostart_from_json(path);
}

void plugin_manager_load_autostart(void) {
    ESP_LOGI(TAG, "Loading autostart plugins...");

    // Discover all available plugins
    plugin_discovery_info_t* plugins = NULL;
    size_t plugin_count = plugin_manager_discover(&plugins);

    if (plugins == NULL || plugin_count == 0) {
        ESP_LOGI(TAG, "No plugins found for autostart");
        return;
    }

    // Load plugins with autostart enabled
    int loaded_count = 0;
    for (size_t i = 0; i < plugin_count; i++) {
        // Use optimized check with path already available
        if (check_autostart_for_plugin(plugins[i].slug, plugins[i].path)) {
            ESP_LOGI(TAG, "Autostarting plugin: %s", plugins[i].slug);

            plugin_context_t* ctx = plugin_manager_load(plugins[i].path);
            if (ctx) {
                // Start service if it's a service plugin
                if (plugins[i].type == PLUGIN_TYPE_SERVICE) {
                    plugin_manager_start_service(ctx);
                }
                loaded_count++;
            } else {
                ESP_LOGW(TAG, "Failed to autostart plugin: %s", plugins[i].slug);
            }
        }
    }

    plugin_manager_free_discovery(plugins, plugin_count);
    ESP_LOGI(TAG, "Autostart complete: %d plugins loaded", loaded_count);
}

// ============================================
// Status Widget Integration
// ============================================

size_t plugin_manager_get_status_widgets(plugin_icontext_t* out, size_t max,
                                          int start_x, int start_y) {
    return plugin_api_get_status_widgets(out, max, start_x, start_y);
}
