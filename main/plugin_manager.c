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
#include "nvs_flash.h"
#include "nvs.h"
#include "cJSON.h"
#define KBELF_REVEAL_PRIVATE
#include "kbelf.h"
#include "fastopen.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
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

// External function from plugin_api.c
extern size_t plugin_api_get_status_widgets(plugin_icontext_t* out, size_t max, int start_x, int start_y);
extern int plugin_api_dispatch_event(uint32_t event_type, void* event_data);

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

    // Unload all plugins
    for (size_t i = 0; i < loaded_plugin_count; i++) {
        if (loaded_plugins[i] != NULL) {
            // Stop service if running
            if (loaded_plugins[i]->state == PLUGIN_STATE_RUNNING) {
                plugin_manager_stop_service(loaded_plugins[i]);
            }

            // Call cleanup
            if (loaded_plugins[i]->registration &&
                loaded_plugins[i]->registration->entry.cleanup) {
                loaded_plugins[i]->registration->entry.cleanup(loaded_plugins[i]);
            }

            // Free resources
            if (loaded_plugins[i]->elf_handle) {
                kbelf_dyn dyn = (kbelf_dyn)loaded_plugins[i]->elf_handle;
                kbelf_dyn_unload(dyn);
                kbelf_dyn_destroy(dyn);
            }

            free(loaded_plugins[i]->plugin_path);
            free(loaded_plugins[i]->plugin_slug);
            free(loaded_plugins[i]->storage_base_path);
            free(loaded_plugins[i]->settings_namespace);
            free(loaded_plugins[i]);
            loaded_plugins[i] = NULL;
        }
    }

    loaded_plugin_count = 0;

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
    ESP_LOGI(TAG, "Discovering plugins...");

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

    *out_plugins = plugins;
    return count;
}

void plugin_manager_free_discovery(plugin_discovery_info_t* plugins, size_t count) {
    if (plugins == NULL) return;

    for (size_t i = 0; i < count; i++) {
        free(plugins[i].path);
        free(plugins[i].slug);
        free(plugins[i].name);
        free(plugins[i].version);
    }
    free(plugins);
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
    kbelf_dyn dyn = kbelf_dyn_create(0);
    if (!dyn) {
        ESP_LOGE(TAG, "Failed to create kbelf context");
        goto error_cleanup;
    }

    if (!kbelf_dyn_set_exec(dyn, elf_path, NULL)) {
        ESP_LOGE(TAG, "Failed to set executable: %s", elf_path);
        kbelf_dyn_destroy(dyn);
        goto error_cleanup;
    }

    if (!kbelf_dyn_load(dyn)) {
        ESP_LOGE(TAG, "Failed to load plugin ELF");
        kbelf_dyn_destroy(dyn);
        goto error_cleanup;
    }

    free(elf_path);
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

            // Call the plugin's init function if available
            if (reg->entry.init != NULL) {
                int init_result = reg->entry.init(ctx);
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
    free(elf_path);
    free(ctx->plugin_path);
    free(ctx->plugin_slug);
    free(ctx->storage_base_path);
    free(ctx->settings_namespace);
    free(ctx);
    xSemaphoreGive(plugin_mutex);
    return NULL;
}

bool plugin_manager_unload(plugin_context_t* ctx) {
    if (ctx == NULL) return false;

    ESP_LOGI(TAG, "Unloading plugin: %s", ctx->plugin_slug);

    xSemaphoreTake(plugin_mutex, portMAX_DELAY);

    // Stop service if running
    if (ctx->state == PLUGIN_STATE_RUNNING) {
        plugin_manager_stop_service(ctx);
    }

    // Call cleanup if available
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

        kbelf_dyn_unload(dyn);
        kbelf_dyn_destroy(dyn);
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

    // Free context
    free(ctx->plugin_path);
    free(ctx->plugin_slug);
    free(ctx->storage_base_path);
    free(ctx->settings_namespace);
    free(ctx);

    xSemaphoreGive(plugin_mutex);
    return true;
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

static void plugin_service_task(void* arg) {
    plugin_context_t* ctx = (plugin_context_t*)arg;

    ESP_LOGI(TAG, "Service task started for plugin: %s", ctx->plugin_slug);
    ctx->task_running = true;

    if (ctx->registration && ctx->registration->entry.service_run) {
        ctx->registration->entry.service_run(ctx);
    }

    ESP_LOGI(TAG, "Service task ended for plugin: %s", ctx->plugin_slug);
    ctx->task_running = false;
    ctx->state = PLUGIN_STATE_STOPPED;
    ctx->task_handle = NULL;
    vTaskDelete(NULL);
}

bool plugin_manager_start_service(plugin_context_t* ctx) {
    if (ctx == NULL || ctx->state == PLUGIN_STATE_RUNNING) {
        return false;
    }

    ESP_LOGI(TAG, "Starting service plugin: %s", ctx->plugin_slug);

    // Initialize control flags
    ctx->stop_requested = false;
    ctx->task_running = false;

    TaskHandle_t task;
    BaseType_t ret = xTaskCreate(
        plugin_service_task,
        ctx->plugin_slug,
        8192,  // Stack size
        ctx,
        5,     // Priority
        &task
    );

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create service task");
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
        return true;
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
        TaskHandle_t task = (TaskHandle_t)ctx->task_handle;
        if (task != NULL) {
            vTaskDelete(task);
            ctx->task_handle = NULL;
        }
    }

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
