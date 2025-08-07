#include "app_metadata_parser.h"
#include <stdio.h>
#include <string.h>
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

static appfs_handle_t find_appfs_handle_for_slug(const char* search_slug) {
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

    cJSON* author_obj = cJSON_GetObjectItem(root, "author");
    if (author_obj && (author_obj->valuestring != NULL)) {
        app->author = strdup(author_obj->valuestring);
    }

    cJSON* license_obj = cJSON_GetObjectItem(root, "license");
    if (license_obj && (license_obj->valuestring != NULL)) {
        app->license = strdup(license_obj->valuestring);
    }

    cJSON* main_obj = cJSON_GetObjectItem(root, "main");
    if (main_obj && (main_obj->valuestring != NULL)) {
        app->main = strdup(main_obj->valuestring);
    }

    cJSON* interpreter_obj = cJSON_GetObjectItem(root, "interpreter");
    if (interpreter_obj && (interpreter_obj->valuestring != NULL)) {
        app->interpreter = strdup(interpreter_obj->valuestring);
    } else {
        app->interpreter = strdup(app->slug);
    }

    if (app->interpreter != NULL) {
        app->appfs_fd = find_appfs_handle_for_slug(app->interpreter);
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

    cJSON* version_obj = cJSON_GetObjectItem(root, "version");
    if (version_obj) {
        app->version = version_obj->valueint;
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
    if (app->author != NULL) free(app->author);
    if (app->license != NULL) free(app->license);
    if (app->main != NULL) free(app->main);
    if (app->interpreter != NULL) free(app->interpreter);
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
            app_t* app       = calloc(1, sizeof(app_t));
            app->slug        = strdup(slug);
            app->name        = strdup(name);
            app->version     = version;
            app->interpreter = strdup(slug);
            app->icon        = calloc(1, sizeof(pax_buf_t));
            if (app->icon != NULL) {
                pax_buf_init(app->icon, NULL, 32, 32, PAX_BUF_32_8888ARGB);
                pax_draw_image(app->icon, get_icon(ICON_APP), 0, 0);
            }
            app->appfs_fd     = appfs_fd;
            out_list[count++] = app;
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
