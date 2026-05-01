#include "app_management.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "app_metadata_parser.h"
#include "app_usage.h"
#include "appfs.h"
#include "appfs_settings.h"
#include "bsp/device.h"
#include "cJSON.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "fastopen.h"
#include "filesystem_utils.h"
#include "http_download.h"
#ifdef CONFIG_ENABLE_LAUNCHERPLUGINS
#include "plugin_manager.h"
#endif
#include "repository_client.h"

static const char* TAG = "App management";

static const char* app_mgmt_location_to_path(app_mgmt_location_t location) {
    switch (location) {
        case APP_MGMT_LOCATION_INTERNAL:
            return "/int/apps";
        case APP_MGMT_LOCATION_SD:
            return "/sd/apps";
        case APP_MGMT_LOCATION_INTERNAL_PLUGINS:
            return "/int/plugins";
        case APP_MGMT_LOCATION_SD_PLUGINS:
            return "/sd/plugins";
        default:
            return NULL;
    }
}

static bool app_mgmt_location_is_plugin(app_mgmt_location_t location) {
    return location == APP_MGMT_LOCATION_INTERNAL_PLUGINS || location == APP_MGMT_LOCATION_SD_PLUGINS;
}

esp_err_t app_mgmt_install(const char* repository_url, const char* slug, app_mgmt_location_t location,
                           download_callback_t download_callback) {
    if (strlen(slug) < 1) {
        ESP_LOGE(TAG, "Slug is empty");
        return ESP_ERR_INVALID_ARG;
    }

    const char* base_path = app_mgmt_location_to_path(location);
    if (base_path == NULL) {
        ESP_LOGE(TAG, "Invalid base path");
        return ESP_ERR_INVALID_ARG;
    }

    // Create apps folder if it does not exist
    if (!fs_utils_exists(base_path)) {
        if (mkdir(base_path, 0777) != 0) {
            ESP_LOGE(TAG, "Failed to create apps directory (%s)", base_path);
            return ESP_FAIL;
        }
    }

    // Load latest version of metadata for the app
    repository_json_data_t metadata = {0};
    if (!load_project(repository_url, &metadata, slug)) {
        ESP_LOGE(TAG, "Failed to load project metadata for slug: %s", slug);
        return ESP_ERR_INVALID_RESPONSE;
    }

    repository_json_data_t information = {0};
    if (!load_information(repository_url, &information)) {
        free_repository_data_json(&metadata);
        ESP_LOGE(TAG, "Failed to load repository information");
        return ESP_ERR_INVALID_RESPONSE;
    }

    // Load URL of repository data
    cJSON* repository_data_url_obj = cJSON_GetObjectItem(information.json, "data_path");
    if (repository_data_url_obj == NULL || !cJSON_IsString(repository_data_url_obj)) {
        free_repository_data_json(&metadata);
        free_repository_data_json(&information);
        ESP_LOGE(TAG, "Failed to get repository data URL");
        return ESP_ERR_INVALID_RESPONSE;
    }
    char* repository_data_url = repository_data_url_obj->valuestring;

    // Find name of the device
    char device_name[32] = {0};
    bsp_device_get_name(device_name, sizeof(device_name));
    size_t device_name_len = strlen(device_name);

    // Find application for current platform
    cJSON* applications = cJSON_GetObjectItem(metadata.json, "application");
    if (applications == NULL || !cJSON_IsArray(applications)) {
        free_repository_data_json(&metadata);
        free_repository_data_json(&information);
        ESP_LOGE(TAG, "No applications found in metadata");
        return ESP_ERR_INVALID_RESPONSE;
    }

    bool   application_found = false;
    cJSON* application       = NULL;
    cJSON_ArrayForEach(application, applications) {
        cJSON* targets = cJSON_GetObjectItem(application, "targets");
        if (targets == NULL || !cJSON_IsArray(targets)) {
            continue;
        }

        cJSON* target = NULL;
        cJSON_ArrayForEach(target, targets) {
            if (target == NULL || !cJSON_IsString(target)) {
                continue;
            }
            if (strlen(target->valuestring) == device_name_len &&
                strncasecmp(target->valuestring, device_name, device_name_len) == 0) {
                // Found application for current platform
                application_found = true;
                break;
            }
        }
        if (application_found) {
            break;
        }
    }

    if (!application_found) {
        free_repository_data_json(&metadata);
        free_repository_data_json(&information);
        ESP_LOGE(TAG, "No application found for device: %s", device_name);
        return ESP_ERR_NOT_FOUND;
    }

    // Get the target directory path
    char app_path[256] = {0};
    snprintf(app_path, sizeof(app_path), "%s/%s", base_path, slug);

    // Remove app folder if it already exists
    if (fs_utils_exists(app_path)) {
        esp_err_t uninstall_res = app_mgmt_uninstall(slug, location);
        if (uninstall_res != ESP_OK) {
            free_repository_data_json(&metadata);
            free_repository_data_json(&information);
            ESP_LOGE(TAG, "Failed to uninstall existing app: %s", esp_err_to_name(uninstall_res));
            return uninstall_res;
        }
    }

    // Create new app folder
    if (mkdir(app_path, 0777) != 0) {
        free_repository_data_json(&metadata);
        free_repository_data_json(&information);
        ESP_LOGE(TAG, "Failed to create app directory (%s)", app_path);
        return ESP_FAIL;
    }

    // Store metadata in the app directory
    char file_path[512] = {0};
    snprintf(file_path, sizeof(file_path), "%s/metadata.json", app_path);
    FILE* fd = fastopen(file_path, "wb");
    if (fd == NULL) {
        free_repository_data_json(&metadata);
        free_repository_data_json(&information);
        ESP_LOGE(TAG, "Failed to open metadata file for writing");
        return ESP_FAIL;
    }
    fwrite(metadata.data, 1, metadata.size, fd);
    fastclose(fd);

    // Create a download session for all file downloads (reuses TCP/TLS connection)
    char base_data_url[256] = {0};
    snprintf(base_data_url, sizeof(base_data_url), "%s/%s/%s", repository_url, repository_data_url, slug);
    http_session_t session = http_session_begin(base_data_url);
    if (session == NULL) {
        free_repository_data_json(&metadata);
        free_repository_data_json(&information);
        app_mgmt_uninstall(slug, location);
        ESP_LOGE(TAG, "Failed to create HTTP session");
        return ESP_FAIL;
    }
    http_session_set_callback(session, download_callback, NULL);

    // Install assets
    cJSON* assets = cJSON_GetObjectItem(application, "assets");
    if (assets != NULL && cJSON_IsArray(assets)) {
        cJSON* asset = NULL;
        cJSON_ArrayForEach(asset, assets) {
            if (asset == NULL || !cJSON_IsObject(asset)) {
                http_session_end(session);
                free_repository_data_json(&metadata);
                free_repository_data_json(&information);
                app_mgmt_uninstall(slug, location);
                ESP_LOGE(TAG, "Invalid asset object");
                return ESP_ERR_INVALID_RESPONSE;
            }
            cJSON* source_file = cJSON_GetObjectItem(asset, "source_file");
            cJSON* target_file = cJSON_GetObjectItem(asset, "target_file");
            if (source_file == NULL || target_file == NULL || !cJSON_IsString(source_file) ||
                !cJSON_IsString(target_file)) {
                http_session_end(session);
                free_repository_data_json(&metadata);
                free_repository_data_json(&information);
                app_mgmt_uninstall(slug, location);
                ESP_LOGE(TAG, "Invalid asset source or target file");
                return ESP_ERR_INVALID_RESPONSE;
            }

            char file_url[256] = {0};
            snprintf(file_url, sizeof(file_url), "%s/%s/%s/%s", repository_url, repository_data_url, slug,
                     source_file->valuestring);

            char target_path[512] = {0};
            snprintf(target_path, sizeof(target_path), "%s/%s", app_path, target_file->valuestring);

            char status_text[64] = {0};
            snprintf(status_text, sizeof(status_text), "Downloading asset '%s'...", target_file->valuestring);
            http_session_set_callback(session, download_callback, status_text);
            if (!http_session_download_file(session, file_url, target_path)) {
                ESP_LOGE(TAG, "Failed to download asset: %s", source_file->valuestring);
                http_session_end(session);
                free_repository_data_json(&metadata);
                free_repository_data_json(&information);
                app_mgmt_uninstall(slug, location);
                return ESP_FAIL;
            }
        }
    } else {
        http_session_end(session);
        free_repository_data_json(&metadata);
        free_repository_data_json(&information);
        app_mgmt_uninstall(slug, location);
        ESP_LOGE(TAG, "No assets found in application metadata");
        return ESP_ERR_INVALID_RESPONSE;
    }

    // Download and store executable binary.
    // - type=appfs installed to /int: binary streams directly into AppFS (skips /int/apps/{slug}/).
    // - type=appfs installed to /sd: binary is stored on /sd/apps/{slug}/ and cached to AppFS on first launch.
    // - type=plugin: binary stays on the install filesystem and is loaded directly from there.
    cJSON* type = cJSON_GetObjectItem(application, "type");
    if (type != NULL && cJSON_IsString(type) &&
        (strcmp(type->valuestring, "appfs") == 0 || strcmp(type->valuestring, "plugin") == 0)) {
        char*  executable     = NULL;
        cJSON* executable_obj = cJSON_GetObjectItem(application, "executable");
        if (executable_obj != NULL && cJSON_IsString(executable_obj)) {
            executable = executable_obj->valuestring;
        }

        bool install_to_appfs = (location == APP_MGMT_LOCATION_INTERNAL) && (strcmp(type->valuestring, "appfs") == 0);

        if (executable != NULL && strlen(executable) > 0) {
            char file_url[256] = {0};
            snprintf(file_url, sizeof(file_url), "%s/%s/%s/%s", repository_url, repository_data_url, slug, executable);

            char status_text[64] = {0};
            snprintf(status_text, sizeof(status_text), "Downloading executable '%s'...", executable);
            http_session_set_callback(session, download_callback, status_text);

            if (install_to_appfs) {
                // Clear any stale AppFS entry so a failed download doesn't leave a half-populated one behind.
                if (appfsExists(slug)) {
                    appfsDeleteFile(slug);
                }

                const char* app_name = (cJSON_GetObjectItem(metadata.json, "name") &&
                                        cJSON_IsString(cJSON_GetObjectItem(metadata.json, "name")))
                                           ? cJSON_GetObjectItem(metadata.json, "name")->valuestring
                                           : slug;
                uint32_t    app_revision = 0;
                cJSON*      revision_obj = cJSON_GetObjectItem(application, "revision");
                if (revision_obj != NULL && cJSON_IsNumber(revision_obj)) {
                    app_revision = (uint32_t)revision_obj->valueint;
                }

                uint8_t* buf       = NULL;
                size_t   buf_size  = 0;
                if (!http_session_download_ram(session, file_url, &buf, &buf_size)) {
                    ESP_LOGE(TAG, "Failed to download executable: %s", executable);
                    http_session_end(session);
                    free_repository_data_json(&metadata);
                    free_repository_data_json(&information);
                    app_mgmt_uninstall(slug, location);
                    return ESP_FAIL;
                }

                esp_err_t appfs_res = app_mgmt_install_from_buffer(slug, app_name, app_revision, buf, buf_size);
                if (appfs_res == ESP_ERR_NO_MEM) {
                    uint8_t auto_cleanup = 0;
                    appfs_settings_get_auto_cleanup(&auto_cleanup);
                    if (auto_cleanup) {
                        size_t rounded_size =
                            (buf_size + (SPI_FLASH_MMU_PAGE_SIZE - 1)) & (~(SPI_FLASH_MMU_PAGE_SIZE - 1));
                        app_mgmt_appfs_evict_lru(rounded_size);
                        appfs_res = app_mgmt_install_from_buffer(slug, app_name, app_revision, buf, buf_size);
                    }
                }
                free(buf);

                if (appfs_res != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to write executable to AppFS: %s (%s)", executable,
                             esp_err_to_name(appfs_res));
                    http_session_end(session);
                    free_repository_data_json(&metadata);
                    free_repository_data_json(&information);
                    app_mgmt_uninstall(slug, location);
                    return appfs_res;
                }
            } else {
                char target_path[512] = {0};
                snprintf(target_path, sizeof(target_path), "%s/%s", app_path, executable);

                if (!http_session_download_file(session, file_url, target_path)) {
                    ESP_LOGE(TAG, "Failed to download executable: %s", executable);
                    http_session_end(session);
                    free_repository_data_json(&metadata);
                    free_repository_data_json(&information);
                    app_mgmt_uninstall(slug, location);
                    return ESP_FAIL;
                }
            }
        }
    }

    cJSON* icon_obj = cJSON_GetObjectItem(metadata.json, "icon");
    if (icon_obj != NULL && cJSON_IsObject(icon_obj)) {
        const char* icon_keys[] = {"16x16", "32x32", "64x64"};
        for (int i = 0; i < 3; i++) {
            cJSON* icon_entry = cJSON_GetObjectItem(icon_obj, icon_keys[i]);
            if (icon_entry != NULL && cJSON_IsString(icon_entry)) {
                char icon_url[256];
                snprintf(icon_url, sizeof(icon_url), "%s/%s/%s/%s", repository_url, repository_data_url, slug,
                         icon_entry->valuestring);
                char icon_path[512];
                snprintf(icon_path, sizeof(icon_path), "%s/%s", app_path, icon_entry->valuestring);
                char status_text[64] = {0};
                snprintf(status_text, sizeof(status_text), "Downloading icon '%s'...", icon_entry->valuestring);
                http_session_set_callback(session, download_callback, status_text);
                if (!http_session_download_file(session, icon_url, icon_path)) {
                    ESP_LOGE(TAG, "Failed to download icon %s", icon_keys[i]);
                }
            }
        }
    }

    http_session_end(session);

    // Remove stale AppFS cache if present so the newly-installed /sd copy becomes the source of truth
    // on next launch. The int/appfs path writes directly to AppFS and is handled before the download;
    // plugins and int/elf/int/script installs don't use AppFS. If the cached revision still matches
    // the metadata revision, the cache is up-to-date and we keep it.
    if (location == APP_MGMT_LOCATION_SD && appfsExists(slug)) {
        uint32_t       new_revision    = 0;
        cJSON*         revision_obj    = cJSON_GetObjectItem(application, "revision");
        if (revision_obj != NULL && cJSON_IsNumber(revision_obj)) {
            new_revision = (uint32_t)revision_obj->valueint;
        }
        appfs_handle_t cached_fd       = appfsOpen(slug);
        uint16_t       cached_revision = 0;
        if (cached_fd != APPFS_INVALID_FD) {
            appfsEntryInfoExt(cached_fd, NULL, NULL, &cached_revision, NULL);
        }
        if ((uint16_t)new_revision != cached_revision) {
            ESP_LOGI(TAG, "Removing stale AppFS cache for %s (cached rev %u, new rev %u)", slug, cached_revision,
                     new_revision);
            appfsDeleteFile(slug);
        } else {
            ESP_LOGI(TAG, "Keeping AppFS cache for %s (revision %u unchanged)", slug, cached_revision);
        }
    }

    free_repository_data_json(&metadata);
    free_repository_data_json(&information);
    ESP_LOGI(TAG, "App %s installed successfully at %s", slug, base_path);
    return ESP_OK;
}

esp_err_t app_mgmt_uninstall(const char* slug, app_mgmt_location_t location) {
    const char* base_path = app_mgmt_location_to_path(location);
    if (base_path == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    bool is_plugin = app_mgmt_location_is_plugin(location);

#ifdef CONFIG_ENABLE_LAUNCHERPLUGINS
    // For plugins: stop and unload before deleting files
    if (is_plugin) {
        plugin_context_t* ctx = plugin_manager_get_by_slug(slug);
        if (ctx != NULL) {
            plugin_manager_stop_service(ctx);
            plugin_manager_unload(ctx);
        }
    }
#endif

    esp_err_t res = ESP_OK;

    char app_path[256] = {0};
    snprintf(app_path, sizeof(app_path), "%s/%s", base_path, slug);

    if (fs_utils_exists(app_path)) {
        res = fs_utils_remove(app_path);
    }

    if (!is_plugin) {
        // If app is installed on SD, check if the app is also installed to the internal storage
        // if it is, do not remove the binary from appfs
        if (location == APP_MGMT_LOCATION_SD) {
            snprintf(app_path, sizeof(app_path), "%s/%s", app_mgmt_location_to_path(APP_MGMT_LOCATION_INTERNAL), slug);
            if (fs_utils_exists(app_path)) {
                return res;
            }
        }

        if (appfsExists(slug)) {
            esp_err_t appfs_res = appfsDeleteFile(slug);
            if (appfs_res != ESP_OK) {
                res = appfs_res;
            }
        }
    }

    // Clean up last-used timestamp from NVS
    app_usage_remove_last_used(slug);

    return res;
}

esp_err_t app_mgmt_install_from_buffer(const char* slug, const char* name, uint32_t revision, uint8_t* data,
                                       size_t size) {
    if (slug == NULL || name == NULL || data == NULL || size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t rounded_size = (size + (SPI_FLASH_MMU_PAGE_SIZE - 1)) & (~(SPI_FLASH_MMU_PAGE_SIZE - 1));
    if (appfsGetFreeMem() < rounded_size) {
        ESP_LOGW(TAG, "Not enough space in AppFS for %s (need %u, have %u)", slug, rounded_size, appfsGetFreeMem());
        return ESP_ERR_NO_MEM;
    }

    appfs_handle_t appfs_file = APPFS_INVALID_FD;
    if (appfsCreateFileExt(slug, name, revision, size, &appfs_file) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create appfs file: %s", slug);
        return ESP_FAIL;
    }

    if (appfsErase(appfs_file, 0, rounded_size) != ESP_OK) {
        appfsDeleteFile(slug);
        ESP_LOGE(TAG, "Failed to erase appfs file: %s", slug);
        return ESP_FAIL;
    }

    if (appfsWrite(appfs_file, 0, data, size) != ESP_OK) {
        appfsDeleteFile(slug);
        ESP_LOGE(TAG, "Failed to install executable to AppFS: %s", slug);
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t app_mgmt_install_from_file(const char* slug, const char* name, uint32_t revision, char* firmware_path) {
    if (slug == NULL || name == NULL || firmware_path == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    FILE* fd = fastopen(firmware_path, "rb");
    if (fd == NULL) {
        ESP_LOGE(TAG, "Failed to open executable file for app %s", slug);
        return ESP_FAIL;
    }

    size_t   file_size = fs_utils_get_file_size(fd);
    uint8_t* file_data = fs_utils_load_file_to_ram(fd);
    fastclose(fd);

    if (file_data == NULL) {
        ESP_LOGE(TAG, "Failed to read executable file for app %s", slug);
        return ESP_FAIL;
    }

    esp_err_t res = app_mgmt_install_from_buffer(slug, name, revision, file_data, file_size);
    free(file_data);
    return res;
}

// --- AppFS cache management ---

bool app_mgmt_has_binary_in_install_dir(const char* slug) {
    const char* base_paths[] = {"/int/apps", "/sd/apps"};
    for (int i = 0; i < 2; i++) {
        uint32_t revision  = 0;
        char*    exec_path = NULL;
        if (get_executable_revision(base_paths[i], slug, &revision, &exec_path)) {
            if (exec_path != NULL) {
                bool exists = fs_utils_exists(exec_path);
                free(exec_path);
                if (exists) {
                    return true;
                }
            }
        }
    }
    return false;
}

esp_err_t app_mgmt_copy_appfs_to_install_dir(const char* slug) {
    // Find which base path has an install directory for this app
    const char* base_paths[] = {"/int/apps", "/sd/apps"};
    const char* found_base   = NULL;
    char        app_dir[256] = {0};
    for (int i = 0; i < 2; i++) {
        snprintf(app_dir, sizeof(app_dir), "%s/%s", base_paths[i], slug);
        if (fs_utils_exists(app_dir)) {
            found_base = base_paths[i];
            break;
        }
    }
    if (found_base == NULL) {
        ESP_LOGE(TAG, "No install directory found for app %s", slug);
        return ESP_ERR_NOT_FOUND;
    }

    // Get executable filename from metadata
    // get_executable_revision(base, slug, ...) returns path as "{base}/{slug}/{filename}"
    uint32_t revision  = 0;
    char*    exec_path = NULL;
    if (!get_executable_revision(found_base, slug, &revision, &exec_path)) {
        ESP_LOGE(TAG, "Failed to get executable info for app %s", slug);
        return ESP_FAIL;
    }

    // Read binary from appfs
    appfs_handle_t appfs_fd = appfsOpen(slug);
    if (appfs_fd == APPFS_INVALID_FD) {
        ESP_LOGE(TAG, "Failed to open appfs entry for %s", slug);
        free(exec_path);
        return ESP_FAIL;
    }

    int appfs_size = 0;
    appfsEntryInfoExt(appfs_fd, NULL, NULL, NULL, &appfs_size);
    if (appfs_size <= 0) {
        ESP_LOGE(TAG, "Invalid appfs file size for %s", slug);
        free(exec_path);
        return ESP_FAIL;
    }

    uint8_t* file_data = malloc(appfs_size);
    if (file_data == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for appfs read (%d bytes)", appfs_size);
        free(exec_path);
        return ESP_ERR_NO_MEM;
    }

    if (appfsRead(appfs_fd, 0, file_data, appfs_size) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read appfs entry for %s", slug);
        free(file_data);
        free(exec_path);
        return ESP_FAIL;
    }

    // Write to install directory
    FILE* fd = fastopen(exec_path, "wb");
    if (fd == NULL) {
        ESP_LOGE(TAG, "Failed to open %s for writing", exec_path);
        free(file_data);
        free(exec_path);
        return ESP_FAIL;
    }

    size_t written = fwrite(file_data, 1, appfs_size, fd);
    fastclose(fd);
    free(file_data);

    if (written != (size_t)appfs_size) {
        ESP_LOGE(TAG, "Failed to write complete binary to %s", exec_path);
        free(exec_path);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Copied appfs binary for %s to %s", slug, exec_path);
    free(exec_path);
    return ESP_OK;
}

bool app_mgmt_is_int_only(const char* slug) {
    // True for apps installed to /int under the new scheme: metadata present on /int,
    // no executable on any filesystem (the binary only lives in AppFS).
    char path[256];
    snprintf(path, sizeof(path), "/int/apps/%s/metadata.json", slug);
    if (!fs_utils_exists(path)) {
        return false;
    }
    return !app_mgmt_has_binary_in_install_dir(slug);
}

bool app_mgmt_can_uncache(const char* slug) {
    // An app can only be uncached if its executable exists on the install filesystem,
    // so the AppFS entry can be rebuilt later. Apps whose binary lives only in AppFS
    // (dev/legacy apps, and int-installed apps under the new scheme) cannot be uncached.
    return app_mgmt_has_binary_in_install_dir(slug);
}

esp_err_t app_mgmt_remove_from_appfs(const char* slug) {
    if (!appfsExists(slug)) {
        return ESP_OK;
    }

    // Ensure binary exists in install dir before removing from appfs (backward compat)
    if (!app_mgmt_has_binary_in_install_dir(slug)) {
        esp_err_t res = app_mgmt_copy_appfs_to_install_dir(slug);
        if (res != ESP_OK) {
            ESP_LOGE(TAG, "Failed to back up appfs binary for %s, not removing", slug);
            return res;
        }
    }

    return appfsDeleteFile(slug);
}

char* app_mgmt_find_firmware_path(const char* slug) {
    const char* base_paths[] = {"/int/apps", "/sd/apps"};
    for (int i = 0; i < 2; i++) {
        uint32_t revision  = 0;
        char*    exec_path = NULL;
        if (get_executable_revision(base_paths[i], slug, &revision, &exec_path)) {
            if (exec_path != NULL && fs_utils_exists(exec_path)) {
                return exec_path;
            }
            free(exec_path);
        }
    }
    return NULL;
}

esp_err_t app_mgmt_ensure_in_appfs(const char* slug, const char* name, uint32_t revision, const char* firmware_path) {
    if (appfsExists(slug)) {
        return ESP_OK;
    }

    FILE* fd = fastopen(firmware_path, "rb");
    if (fd == NULL) {
        ESP_LOGE(TAG, "Failed to open firmware file %s", firmware_path);
        return ESP_FAIL;
    }
    size_t file_size = fs_utils_get_file_size(fd);
    fastclose(fd);

    if (file_size == 0) {
        ESP_LOGE(TAG, "Firmware file %s is empty", firmware_path);
        return ESP_FAIL;
    }

    size_t rounded_size = (file_size + (SPI_FLASH_MMU_PAGE_SIZE - 1)) & (~(SPI_FLASH_MMU_PAGE_SIZE - 1));
    if (appfsGetFreeMem() < rounded_size) {
        ESP_LOGW(TAG, "Not enough space in AppFS for %s (need %u, have %u)", slug, rounded_size, appfsGetFreeMem());
        return ESP_ERR_NO_MEM;
    }

    return app_mgmt_install_from_file(slug, name, revision, (char*)firmware_path);
}

esp_err_t app_mgmt_cache_to_appfs(const char* slug, const char* name, uint32_t revision, const char* firmware_path) {
    esp_err_t res = app_mgmt_ensure_in_appfs(slug, name, revision, firmware_path);
    if (res == ESP_ERR_NO_MEM) {
        uint8_t auto_cleanup = 0;
        appfs_settings_get_auto_cleanup(&auto_cleanup);
        if (auto_cleanup) {
            FILE* fd = fastopen(firmware_path, "rb");
            if (fd != NULL) {
                size_t file_size = fs_utils_get_file_size(fd);
                fastclose(fd);
                size_t rounded_size = (file_size + (SPI_FLASH_MMU_PAGE_SIZE - 1)) & (~(SPI_FLASH_MMU_PAGE_SIZE - 1));
                app_mgmt_appfs_evict_lru(rounded_size);
                res = app_mgmt_ensure_in_appfs(slug, name, revision, firmware_path);
            }
        }
    }
    return res;
}

// Comparison function for qsort — sort by timestamp ascending (oldest first)
typedef struct {
    char     slug[48];
    uint32_t timestamp;
} appfs_usage_entry_t;

static int compare_usage_entries(const void* a, const void* b) {
    const appfs_usage_entry_t* ea = (const appfs_usage_entry_t*)a;
    const appfs_usage_entry_t* eb = (const appfs_usage_entry_t*)b;
    if (ea->timestamp < eb->timestamp) return -1;
    if (ea->timestamp > eb->timestamp) return 1;
    return 0;
}

esp_err_t app_mgmt_appfs_evict_lru(size_t needed_bytes) {
    // Collect appfs entries that have a backup in the install directory.
    // Apps without install dirs (dev/legacy apps only in appfs) are never auto-evicted
    // since removing them would permanently lose the binary.
    // Heap-allocated to avoid stack overflow in the deep call chain.
    appfs_usage_entry_t* entries = malloc(sizeof(appfs_usage_entry_t) * MAX_NUM_APPS);
    if (entries == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for LRU eviction");
        return ESP_ERR_NO_MEM;
    }

    // Collect evictable entries
    size_t         count    = 0;
    appfs_handle_t appfs_fd = appfsNextEntry(APPFS_INVALID_FD);
    while (appfs_fd != APPFS_INVALID_FD && count < MAX_NUM_APPS) {
        const char* slug = NULL;
        appfsEntryInfoExt(appfs_fd, &slug, NULL, NULL, NULL);
        if (slug != NULL && app_mgmt_can_uncache(slug)) {
            strncpy(entries[count].slug, slug, sizeof(entries[count].slug) - 1);
            entries[count].slug[sizeof(entries[count].slug) - 1] = '\0';

            uint32_t ts = 0;
            app_usage_get_last_used(slug, &ts);
            entries[count].timestamp = ts;
            count++;
        }
        appfs_fd = appfsNextEntry(appfs_fd);
    }

    if (count == 0) {
        ESP_LOGW(TAG, "No appfs entries to evict");
        free(entries);
        return ESP_ERR_NO_MEM;
    }

    // Sort by timestamp ascending (oldest first)
    qsort(entries, count, sizeof(appfs_usage_entry_t), compare_usage_entries);

    // Evict entries until we have enough space
    for (size_t i = 0; i < count; i++) {
        if (appfsGetFreeMem() >= needed_bytes) {
            free(entries);
            return ESP_OK;
        }
        ESP_LOGI(TAG, "Evicting %s from AppFS (last used: %u)", entries[i].slug, entries[i].timestamp);
        app_mgmt_remove_from_appfs(entries[i].slug);
    }

    free(entries);

    if (appfsGetFreeMem() >= needed_bytes) {
        return ESP_OK;
    }

    ESP_LOGW(TAG, "Could not free enough AppFS space (need %u, have %u)", needed_bytes, appfsGetFreeMem());
    return ESP_ERR_NO_MEM;
}

// --- App move between storage locations ---

esp_err_t app_mgmt_move(const char* slug, app_mgmt_location_t from, app_mgmt_location_t to) {
    const char* src_base = app_mgmt_location_to_path(from);
    const char* dst_base = app_mgmt_location_to_path(to);
    if (src_base == NULL || dst_base == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    char src_path[256] = {0};
    char dst_path[256] = {0};
    snprintf(src_path, sizeof(src_path), "%s/%s", src_base, slug);
    snprintf(dst_path, sizeof(dst_path), "%s/%s", dst_base, slug);

    if (!fs_utils_exists(src_path)) {
        ESP_LOGE(TAG, "Source app directory does not exist: %s", src_path);
        return ESP_ERR_NOT_FOUND;
    }

    // Check free space on target filesystem
    uint64_t dir_size = fs_utils_get_directory_size(src_path);
    if (dir_size == 0) {
        ESP_LOGE(TAG, "Failed to calculate directory size for %s", src_path);
        return ESP_FAIL;
    }

    // Determine target mount point for free space check
    const char* mount_point = (to == APP_MGMT_LOCATION_SD) ? "/sd" : "/int";
    uint64_t    total_bytes = 0;
    uint64_t    free_bytes  = 0;
    esp_err_t   res         = esp_vfs_fat_info(mount_point, &total_bytes, &free_bytes);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get filesystem info for %s", mount_point);
        return res;
    }

    if (free_bytes < dir_size) {
        ESP_LOGW(TAG, "Not enough space on %s (need %llu, have %llu)", mount_point, dir_size, free_bytes);
        return ESP_ERR_NO_MEM;
    }

    // Create base apps directory on target if needed
    if (!fs_utils_exists(dst_base)) {
        if (mkdir(dst_base, 0777) != 0) {
            ESP_LOGE(TAG, "Failed to create apps directory: %s", dst_base);
            return ESP_FAIL;
        }
    }

    // Remove destination if it already exists
    if (fs_utils_exists(dst_path)) {
        fs_utils_remove(dst_path);
    }

    // Copy directory recursively
    res = fs_utils_copy_recursive(src_path, dst_path);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to copy app from %s to %s", src_path, dst_path);
        // Clean up partial copy
        if (fs_utils_exists(dst_path)) {
            fs_utils_remove(dst_path);
        }
        return res;
    }

    // Remove source directory
    res = fs_utils_remove(src_path);
    if (res != ESP_OK) {
        ESP_LOGW(TAG, "App copied but failed to remove source at %s", src_path);
    }

    ESP_LOGI(TAG, "Moved app %s from %s to %s", slug, src_base, dst_base);
    return ESP_OK;
}

bool app_mgmt_can_move(const char* slug, app_mgmt_location_t from, app_mgmt_location_t to) {
    const char* src_base = app_mgmt_location_to_path(from);
    const char* dst_base = app_mgmt_location_to_path(to);
    if (src_base == NULL || dst_base == NULL) {
        return false;
    }

    char src_path[256] = {0};
    snprintf(src_path, sizeof(src_path), "%s/%s", src_base, slug);
    if (!fs_utils_exists(src_path)) {
        return false;
    }

    uint64_t dir_size = fs_utils_get_directory_size(src_path);
    if (dir_size == 0) {
        return false;
    }

    const char* mount_point = (to == APP_MGMT_LOCATION_SD) ? "/sd" : "/int";
    uint64_t    total_bytes = 0;
    uint64_t    free_bytes  = 0;
    if (esp_vfs_fat_info(mount_point, &total_bytes, &free_bytes) != ESP_OK) {
        return false;
    }

    return free_bytes >= dir_size;
}
