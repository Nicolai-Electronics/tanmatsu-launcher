#include "app_metadata_parser.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "bsp/device.h"
#include "cJSON.h"
#include "esp_log.h"
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "icons.h"
#include "pax_codecs.h"
#include "pax_gfx.h"
#include "pax_types.h"

static const char* TAG = "App metadata";

static uint8_t* load_file_to_ram(FILE* fd) {
    fseek(fd, 0, SEEK_END);
    size_t fsize = ftell(fd);
    fseek(fd, 0, SEEK_SET);
    uint8_t* file = malloc(fsize);
    if (file == NULL) return NULL;
    fread(file, fsize, 1, fd);
    return file;
}

appfs_handle_t find_appfs_handle_for_slug(const char* search_slug) {
    appfs_handle_t appfs_fd = appfsNextEntry(APPFS_INVALID_FD);
    while (appfs_fd != APPFS_INVALID_FD) {
        const char* slug = NULL;
        appfsEntryInfoExt(appfs_fd, &slug, NULL, NULL, NULL);
        if ((strlen(search_slug) == strlen(slug)) && (strcmp(search_slug, slug) == 0)) {
            return appfs_fd;
        }
        appfs_fd = appfsNextEntry(appfs_fd);
    }

    return APPFS_INVALID_FD;
}

app_t* create_app(const char* path, const char* slug) {
    app_t* app = calloc(1, sizeof(app_t));
    app->path  = strdup(path);
    app->slug  = strdup(slug);

    char path_buffer[256] = {0};
    snprintf(path_buffer, sizeof(path_buffer), "%s/%s/metadata.json", path, slug);
    FILE* fd = fopen(path_buffer, "r");
    if (fd == NULL) {
        ESP_LOGE(TAG, "Failed to open metadata file %s", path_buffer);
        return app;
    }

    char* json_data = (char*)load_file_to_ram(fd);
    fclose(fd);

    if (json_data == NULL) {
        ESP_LOGE(TAG, "Failed to read from metadata file %s", path_buffer);
        return app;
    }

    cJSON* root = cJSON_Parse(json_data);
    if (root == NULL) {
        free(json_data);
        ESP_LOGE(TAG, "Failed to parse metadata file %s", path_buffer);
        return app;
    }

    cJSON* name_obj = cJSON_GetObjectItem(root, "name");
    if (name_obj && (name_obj->valuestring != NULL)) {
        app->name = strdup(name_obj->valuestring);
    }

    cJSON* description_obj = cJSON_GetObjectItem(root, "description");
    if (description_obj && (description_obj->valuestring != NULL)) {
        app->description = strdup(description_obj->valuestring);
    }

    cJSON* categories_obj = cJSON_GetObjectItem(root, "categories");
    if (categories_obj) {
        cJSON* entry_obj = NULL;
        int    index     = 0;
        cJSON_ArrayForEach(entry_obj, categories_obj) {
            if (index >= APP_MAX_NUM_CATEGORIES) {
                ESP_LOGW(TAG, "Maximum number of categories reached for app %s", slug);
                break;
            }
            if (!cJSON_IsString(entry_obj)) {
                ESP_LOGW(TAG, "Category %d is not a string for app %s", index, slug);
                continue;
            }
            app->categories[index] = strdup(entry_obj->valuestring);
        }
    }

    cJSON* version_obj = cJSON_GetObjectItem(root, "version");
    if (version_obj) {
        app->version = version_obj->valueint;
    }

    cJSON* icon_obj = cJSON_GetObjectItem(root, "icon");
    if (icon_obj && cJSON_IsObject(icon_obj)) {
        cJSON* icon32_obj = cJSON_GetObjectItem(icon_obj, "32x32");
        if (icon32_obj && cJSON_IsString(icon32_obj)) {
            snprintf(path_buffer, sizeof(path_buffer), "%s/%s/%s", path, slug, icon32_obj->valuestring);
            FILE* icon_fd = fopen(path_buffer, "rb");
            app->icon     = calloc(1, sizeof(pax_buf_t));
            if (app->icon != NULL) {
                if (!pax_decode_png_fd(app->icon, icon_fd, PAX_BUF_32_8888ARGB, 0)) {
                    ESP_LOGE(TAG, "Failed to decode icon for app %s", slug);
                }
            } else {
                ESP_LOGE(TAG, "Failed to open icon file for app %s", slug);
            }
            fclose(icon_fd);
        } else {
            ESP_LOGE(TAG, "No 32x32 icon object for app %s", slug);
        }
    } else {
        ESP_LOGE(TAG, "No icon object for app %s", slug);
    }

    cJSON* author_obj = cJSON_GetObjectItem(root, "author");
    if (author_obj && (author_obj->valuestring != NULL)) {
        app->author = strdup(author_obj->valuestring);
    }

    cJSON* license_type_obj = cJSON_GetObjectItem(root, "license_type");
    if (license_type_obj && (license_type_obj->valuestring != NULL)) {
        app->license_type = strdup(license_type_obj->valuestring);
    }

    cJSON* license_file_obj = cJSON_GetObjectItem(root, "license_file");
    if (license_file_obj && (license_file_obj->valuestring != NULL)) {
        app->license_file = strdup(license_file_obj->valuestring);
    }

    cJSON* repository_obj = cJSON_GetObjectItem(root, "repository");
    if (repository_obj && (repository_obj->valuestring != NULL)) {
        app->repository = strdup(repository_obj->valuestring);
    }

    char device_name[32] = {0};
    bsp_device_get_name(device_name, sizeof(device_name));
    for (int i = 0; device_name[i]; i++) {
        device_name[i] = tolower(device_name[i]);
    }

    cJSON* matched_executable = NULL;
    cJSON* executable_obj     = cJSON_GetObjectItem(root, "application");
    if (executable_obj) {
        cJSON* executable_entry_obj = NULL;
        cJSON_ArrayForEach(executable_entry_obj, executable_obj) {
            cJSON* targets_obj = cJSON_GetObjectItem(executable_entry_obj, "targets");
            if (targets_obj) {
                cJSON* target_obj = NULL;
                cJSON_ArrayForEach(target_obj, targets_obj) {
                    if (target_obj && (target_obj->valuestring != NULL) &&
                        (strcmp(target_obj->valuestring, device_name) == 0) &&
                        strlen(target_obj->valuestring) == strlen(device_name)) {
                        matched_executable = executable_entry_obj;
                        break;
                    }
                }
            }
            if (matched_executable != NULL) {
                break;
            }
        }
    }

    if (matched_executable == NULL) {
        ESP_LOGW(TAG, "No matching executable found for device %s in app %s", device_name, slug);
        app->executable_appfs_fd = find_appfs_handle_for_slug(app->slug);  // Try to find appfs entry anyway
    } else {
        // Parse application
        cJSON* type_obj = cJSON_GetObjectItem(matched_executable, "type");
        if (type_obj && (type_obj->valuestring != NULL)) {
            if (strcmp(type_obj->valuestring, "appfs") == 0) {
                app->executable_type     = EXECUTABLE_TYPE_APPFS;
                app->executable_appfs_fd = find_appfs_handle_for_slug(app->slug);
            } else if (strcmp(type_obj->valuestring, "elf") == 0) {
                app->executable_type = EXECUTABLE_TYPE_ELF;
            } else if (strcmp(type_obj->valuestring, "script") == 0) {
                app->executable_type = EXECUTABLE_TYPE_SCRIPT;
            } else {
                app->executable_type = EXECUTABLE_TYPE_UNKNOWN;
            }

            cJSON* revision_obj = cJSON_GetObjectItem(matched_executable, "revision");
            if (revision_obj) {
                app->executable_revision = revision_obj->valueint;
            } else {
                app->executable_revision = 0;
            }

            cJSON* filename_obj = cJSON_GetObjectItem(matched_executable, "executable");
            if (filename_obj && (filename_obj->valuestring != NULL)) {
                app->executable_filename = strdup(filename_obj->valuestring);
            }

            cJSON* interpreter_obj = cJSON_GetObjectItem(matched_executable, "interpreter");
            if (interpreter_obj && (interpreter_obj->valuestring != NULL)) {
                app->executable_interpreter_slug = strdup(interpreter_obj->valuestring);
            }
        } else {
            ESP_LOGW(TAG, "No type found for executable in app %s", slug);
        }
    }

    cJSON_Delete(root);
    free(json_data);

    if (app->icon == NULL) {
        ESP_LOGE(TAG, "No icon found for app %s, using default icon", slug);
        app->icon = calloc(1, sizeof(pax_buf_t));
        if (app->icon != NULL) {
            pax_buf_init(app->icon, NULL, 32, 32, PAX_BUF_32_8888ARGB);
            pax_draw_image(app->icon, get_icon(ICON_HELP), 0, 0);
        }
    }

    return app;
}

void free_app(app_t* app) {
    if (app == NULL) return;
    if (app->path != NULL) free(app->path);
    if (app->slug != NULL) free(app->slug);
    if (app->name != NULL) free(app->name);
    if (app->description != NULL) free(app->description);
    for (int i = 0; i < APP_MAX_NUM_CATEGORIES; i++) {
        if (app->categories[i] != NULL) {
            free(app->categories[i]);
        }
    }
    if (app->author != NULL) free(app->author);
    if (app->license_type != NULL) free(app->license_type);
    if (app->license_file != NULL) free(app->license_file);
    if (app->repository != NULL) free(app->repository);
    if (app->executable_filename != NULL) free(app->executable_filename);
    if (app->executable_interpreter_slug != NULL) free(app->executable_interpreter_slug);
    if (app->icon != NULL) {
        pax_buf_destroy(app->icon);
        free(app->icon);
    }
    free(app);
}

size_t create_list_of_apps_from_directory(app_t** out_list, size_t list_size, const char* path, app_t** full_list,
                                          size_t full_list_size) {
    DIR* dir = opendir(path);
    if (dir == NULL) {
        return 0;
    }
    struct dirent* entry;
    size_t         count = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (count >= list_size) {
            break;
        }
        if (entry->d_type == DT_DIR) {
            bool already_in_list = false;
            for (size_t i = 0; i < full_list_size; i++) {
                if (full_list[i] != NULL && full_list[i]->slug != NULL &&
                    strcmp(full_list[i]->slug, entry->d_name) == 0) {
                    already_in_list = true;
                    break;
                }
            }
            if (!already_in_list) {
                app_t* app = create_app(path, entry->d_name);
                if (app != NULL && count < list_size) {
                    out_list[count++] = app;
                }
            }
        }
    }
    closedir(dir);
    return count;
}

size_t create_list_of_apps_from_other_appfs_entries(app_t** out_list, size_t list_size, app_t** full_list,
                                                    size_t full_list_size) {
    size_t         count    = 0;
    appfs_handle_t appfs_fd = appfsNextEntry(APPFS_INVALID_FD);
    while (appfs_fd != APPFS_INVALID_FD) {
        if (count >= list_size) {
            break;
        }
        const char* slug    = NULL;
        const char* name    = NULL;
        uint16_t    version = 0xFFFF;
        appfsEntryInfoExt(appfs_fd, &slug, &name, &version, NULL);

        bool already_in_list = false;
        for (size_t i = 0; i < full_list_size; i++) {
            if (full_list[i] != NULL && full_list[i]->slug != NULL && strcmp(full_list[i]->slug, slug) == 0) {
                already_in_list = true;
                break;
            }
        }

        if (!already_in_list) {
            app_t* app           = calloc(1, sizeof(app_t));
            app->slug            = strdup(slug);
            app->name            = strdup(name);
            app->path            = strdup("");
            app->version         = version;
            app->icon            = calloc(1, sizeof(pax_buf_t));
            app->executable_type = EXECUTABLE_TYPE_APPFS;
            if (app->icon != NULL) {
                pax_buf_init(app->icon, NULL, 32, 32, PAX_BUF_32_8888ARGB);
                pax_draw_image(app->icon, get_icon(ICON_APP), 0, 0);
            }
            app->executable_appfs_fd = appfs_fd;
            out_list[count++]        = app;
        }

        appfs_fd = appfsNextEntry(appfs_fd);
    }
    return count;
}

size_t create_list_of_apps(app_t** out_list, size_t list_size) {
    size_t count = 0;

    count += create_list_of_apps_from_directory(&out_list[count], list_size - count, "/sd/apps", out_list, list_size);
    count += create_list_of_apps_from_directory(&out_list[count], list_size - count, "/int/apps", out_list, list_size);
    count += create_list_of_apps_from_other_appfs_entries(&out_list[count], list_size - count, out_list, list_size);
    return count;
}

void free_list_of_apps(app_t** out_list, size_t list_size) {
    for (size_t i = 0; i < list_size; i++) {
        free_app(out_list[i]);
    }
}
