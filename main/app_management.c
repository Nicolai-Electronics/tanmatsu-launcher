#include "app_management.h"
#include <ctype.h>
#include <string.h>
#include "appfs.h"
#include "bsp/device.h"
#include "cJSON.h"
#include "esp_err.h"
#include "esp_log.h"
#include "filesystem_utils.h"
#include "http_download.h"
#include "repository_client.h"

static const char* TAG = "App management";

static const char* app_mgmt_location_to_path(app_mgmt_location_t location) {
    switch (location) {
        case APP_MGMT_LOCATION_INTERNAL:
            return "/int/apps";
        case APP_MGMT_LOCATION_SD:
            return "/sd/apps";
        default:
            return NULL;
    }
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

    char*  name     = NULL;
    cJSON* name_obj = cJSON_GetObjectItem(metadata.json, "name");
    if (name_obj != NULL && cJSON_IsString(name_obj)) {
        name = name_obj->valuestring;
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
    for (int i = 0; device_name[i]; i++) {
        device_name[i] = tolower(device_name[i]);
    }

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
            if (strcmp(target->valuestring, device_name) == 0 && strlen(target->valuestring) == strlen(device_name)) {
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
    FILE* fd = fopen(file_path, "wb");
    if (fd == NULL) {
        free_repository_data_json(&metadata);
        free_repository_data_json(&information);
        ESP_LOGE(TAG, "Failed to open metadata file for writing");
        return ESP_FAIL;
    }
    fwrite(metadata.data, 1, metadata.size, fd);
    fclose(fd);

    // Install assets
    cJSON* assets = cJSON_GetObjectItem(application, "assets");
    if (assets != NULL && cJSON_IsArray(assets)) {
        cJSON* asset = NULL;
        cJSON_ArrayForEach(asset, assets) {
            if (asset == NULL || !cJSON_IsObject(asset)) {
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
            if (!download_file(file_url, target_path, download_callback, status_text)) {
                free_repository_data_json(&metadata);
                free_repository_data_json(&information);
                app_mgmt_uninstall(slug, location);
                ESP_LOGE(TAG, "Failed to download asset: %s", source_file->valuestring);
                return ESP_FAIL;
            }
        }
    } else {
        free_repository_data_json(&metadata);
        free_repository_data_json(&information);
        app_mgmt_uninstall(slug, location);
        ESP_LOGE(TAG, "No assets found in application metadata");
        return ESP_ERR_INVALID_RESPONSE;
    }

    // Install binary to AppFS
    cJSON* type = cJSON_GetObjectItem(application, "type");
    if (type != NULL && cJSON_IsString(type) && strcmp(type->valuestring, "appfs") == 0 &&
        strlen(type->valuestring) == strlen("appfs")) {
        uint32_t revision     = 0;
        cJSON*   revision_obj = cJSON_GetObjectItem(application, "revision");
        if (revision_obj != NULL && cJSON_IsNumber(revision_obj)) {
            revision = (uint32_t)revision_obj->valueint;
        }

        char*  executable     = NULL;
        cJSON* executable_obj = cJSON_GetObjectItem(application, "executable");
        if (executable_obj != NULL && cJSON_IsString(executable_obj)) {
            executable = executable_obj->valuestring;
        }

        if (executable != NULL && strlen(executable) > 0) {
            char file_url[256] = {0};
            snprintf(file_url, sizeof(file_url), "%s/%s/%s/%s", repository_url, repository_data_url, slug, executable);

            uint8_t* file_data = NULL;
            size_t   file_size = 0;

            char status_text[64] = {0};
            snprintf(status_text, sizeof(status_text), "Downloading executable '%s'...", executable);
            if (!download_ram(file_url, &file_data, &file_size, download_callback, status_text)) {
                free_repository_data_json(&metadata);
                free_repository_data_json(&information);
                app_mgmt_uninstall(slug, location);
                ESP_LOGE(TAG, "Failed to download executable: %s", executable);
                return ESP_FAIL;
            }

            appfs_handle_t appfs_file = APPFS_INVALID_FD;
            if (appfsCreateFileExt(slug, name, revision, file_size, &appfs_file) != ESP_OK) {
                free(file_data);
                free_repository_data_json(&metadata);
                free_repository_data_json(&information);
                app_mgmt_uninstall(slug, location);
                ESP_LOGE(TAG, "Failed to create appfs file: %s", slug);
                return ESP_FAIL;
            }

            int rounded_size = (file_size + (SPI_FLASH_MMU_PAGE_SIZE - 1)) & (~(SPI_FLASH_MMU_PAGE_SIZE - 1));
            if (appfsErase(appfs_file, 0, rounded_size) != ESP_OK) {
                free(file_data);
                free_repository_data_json(&metadata);
                free_repository_data_json(&information);
                app_mgmt_uninstall(slug, location);
                ESP_LOGE(TAG, "Failed to erase appfs file: %s", slug);
                return ESP_FAIL;
            }

            if (appfsWrite(appfs_file, 0, file_data, file_size) != ESP_OK) {
                free(file_data);
                free_repository_data_json(&metadata);
                free_repository_data_json(&information);
                app_mgmt_uninstall(slug, location);
                ESP_LOGE(TAG, "Failed to install executable to AppFS: %s", slug);
                return ESP_FAIL;
            }

            if (location == APP_MGMT_LOCATION_SD) {
                snprintf(file_path, sizeof(file_path), "%s/%s", app_path, executable);
                FILE* fd = fopen(file_path, "wb");
                if (fd == NULL) {
                    free(file_data);
                    free_repository_data_json(&metadata);
                    free_repository_data_json(&information);
                    app_mgmt_uninstall(slug, location);
                    ESP_LOGE(TAG, "Failed to write executable to SD card: %s", slug);
                    return ESP_FAIL;
                }
                fwrite(file_data, 1, file_size, fd);
                fclose(fd);
            }

            free(file_data);
        }
    }

    cJSON* icon_obj = cJSON_GetObjectItem(metadata.json, "icon");
    if (icon_obj != NULL && cJSON_IsObject(icon_obj)) {
        cJSON* icon32_obj = cJSON_GetObjectItem(icon_obj, "32x32");
        if (icon32_obj != NULL && cJSON_IsString(icon32_obj)) {
            // Download icon if it exists
            char icon_url[256];
            snprintf(icon_url, sizeof(icon_url), "%s/%s/%s/%s", repository_url, repository_data_url, slug,
                     icon32_obj->valuestring);
            char icon_path[512];
            snprintf(icon_path, sizeof(icon_path), "%s/%s", app_path, icon32_obj->valuestring);
            char status_text[64] = {0};
            snprintf(status_text, sizeof(status_text), "Downloading icon '%s'...", icon32_obj->valuestring);
            if (!download_file(icon_url, icon_path, download_callback, status_text)) {
                ESP_LOGE(TAG, "Failed to download icon");
            }
        }
    }

    free_repository_data_json(&metadata);
    free_repository_data_json(&information);
    ESP_LOGI(TAG, "App %s installed successfully at %s", slug, base_path);
    return ESP_OK;  // Placeholder return value
}

esp_err_t app_mgmt_uninstall(const char* slug, app_mgmt_location_t location) {
    const char* base_path = app_mgmt_location_to_path(location);
    if (base_path == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t res = ESP_OK;

    char app_path[256] = {0};
    snprintf(app_path, sizeof(app_path), "%s/%s", base_path, slug);

    if (fs_utils_exists(app_path)) {
        res = fs_utils_remove(app_path);
    }

    // If app is installed on SD, check if the app is also installed to the internal storage
    // if it is, do not remove the binary from appfs
    if (location == APP_MGMT_LOCATION_SD) {
        snprintf(app_path, sizeof(app_path), "%s/%s", app_mgmt_location_to_path(APP_MGMT_LOCATION_INTERNAL), slug);
        if (fs_utils_exists(app_path)) {
            return res;
        }
    }

    if (appfsExists(slug)) {
        esp_err_t appfs_res = ESP_OK;
        appfs_res           = appfsDeleteFile(slug);
        if (appfs_res != ESP_OK) {
            res = appfs_res;
        }
    }

    return res;
}

esp_err_t app_mgmt_install_from_file(const char* slug, const char* name, uint32_t revision, char* firmware_path) {
    if (slug == NULL || name == NULL || firmware_path == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    FILE* fd = fopen(firmware_path, "rb");
    if (fd == NULL) {
        ESP_LOGE(TAG, "Failed to open executable file for app %s", slug);
        return ESP_FAIL;
    }

    size_t   file_size = fs_utils_get_file_size(fd);
    uint8_t* file_data = fs_utils_load_file_to_ram(fd);
    fclose(fd);

    if (file_data == NULL) {
        ESP_LOGE(TAG, "Failed to read executable file for app %s", slug);
        return ESP_FAIL;
    }

    appfs_handle_t appfs_file = APPFS_INVALID_FD;
    if (appfsCreateFileExt(slug, name, revision, file_size, &appfs_file) != ESP_OK) {
        free(file_data);
        ESP_LOGE(TAG, "Failed to create appfs file: %s", slug);
        return ESP_FAIL;
    }

    int rounded_size = (file_size + (SPI_FLASH_MMU_PAGE_SIZE - 1)) & (~(SPI_FLASH_MMU_PAGE_SIZE - 1));
    if (appfsErase(appfs_file, 0, rounded_size) != ESP_OK) {
        free(file_data);
        ESP_LOGE(TAG, "Failed to erase appfs file: %s", slug);
        return ESP_FAIL;
    }

    if (appfsWrite(appfs_file, 0, file_data, file_size) != ESP_OK) {
        free(file_data);
        ESP_LOGE(TAG, "Failed to install executable to AppFS: %s", slug);
        return ESP_FAIL;
    }

    free(file_data);

    return ESP_OK;
}
