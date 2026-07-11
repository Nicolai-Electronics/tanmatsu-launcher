#include "repository_client.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "bsp/device.h"
#include "cJSON.h"
#include "device_settings.h"
#include "esp_log.h"
#include "http_download.h"
#include "nvs_settings.h"
#include "wifi_connection.h"

extern bool wifi_stack_get_initialized(void);

static const char* TAG = "Repository";

// Helper functions for data management

void free_repository_data_json(repository_json_data_t* data) {
    if (data->json != NULL) {
        cJSON_Delete(data->json);
        data->json = NULL;
    }
    if (data->data != NULL) {
        free(data->data);
        data->data = NULL;
        data->size = 0;
    }
}

static bool download_and_parse(const char* url, repository_json_data_t* out_data) {
    // Free any prior contents so callers can safely re-use the same struct
    // (e.g. the static `projects` global in menu_repository_client) without leaking.
    free_repository_data_json(out_data);
    http_session_t session = http_session_begin(url);
    if (session == NULL) return false;
    bool success = http_session_download_ram(session, url, (uint8_t**)&out_data->data, &out_data->size);
    http_session_end(session);
    if (!success) return false;
    out_data->json = cJSON_ParseWithLength(out_data->data, out_data->size);
    if (out_data->json == NULL) {
        free(out_data->data);
        out_data->data = NULL;
        return false;
    }
    return true;
}

// Helper functions for API

static void url_append(char* destination, char* source, size_t buffer_length) {
    if (buffer_length == 0) {
        return;
    }
    size_t dest_len = strlen(destination);
    for (size_t i = 0; source[i] != '\0'; i++) {
        if (source[i] == ' ') {
            if (dest_len + 3 >= buffer_length) break;
            memcpy(&destination[dest_len], "%20", 3);
            dest_len += 3;
        } else {
            if (dest_len + 1 >= buffer_length) break;
            destination[dest_len++] = source[i];
        }
    }
    destination[dest_len] = '\0';
}

bool load_information(const char* base_url, repository_json_data_t* out_data) {
    char base_uri[64] = {0};
    nvs_settings_get_repo_base_uri(base_uri, sizeof(base_uri), DEFAULT_REPO_BASE_URI);
    char url[256];
    sprintf(url, "%s%s/information", base_url, base_uri);
    return download_and_parse(url, out_data);
}

bool load_categories(const char* base_url, repository_json_data_t* out_data) {
    char base_uri[64] = {0};
    nvs_settings_get_repo_base_uri(base_uri, sizeof(base_uri), DEFAULT_REPO_BASE_URI);

    char device_name[64] = {0};
    bsp_device_get_name(device_name, sizeof(device_name));
    for (size_t i = 0; i < strlen(device_name); i++) {
        device_name[i] = tolower(device_name[i]);
    }

    char url[256];
    sprintf(url, "%s%s/categories?device=", base_url, base_uri);
    url_append(url, device_name, sizeof(url));
    return download_and_parse(url, out_data);
}

bool load_projects(const char* base_url, repository_json_data_t* out_data, const char* category) {
    char base_uri[64] = {0};
    nvs_settings_get_repo_base_uri(base_uri, sizeof(base_uri), DEFAULT_REPO_BASE_URI);
    char url[256];

    char device_name[32] = {0};
    bsp_device_get_name(device_name, sizeof(device_name) - 1);
    for (size_t i = 0; i < strlen(device_name); i++) {
        device_name[i] = tolower(device_name[i]);
    }

    if (category != NULL) {
        sprintf(url, "%s%s/projects?category=%s&device=", base_url, base_uri, category);
    } else {
        sprintf(url, "%s%s/projects?device=", base_url, base_uri);
    }

    url_append(url, device_name, sizeof(url));
    return download_and_parse(url, out_data);
}

bool load_projects_paginated(const char* base_url, repository_json_data_t* out_data, const char* category,
                             uint32_t offset, uint32_t amount) {
    char base_uri[64] = {0};
    nvs_settings_get_repo_base_uri(base_uri, sizeof(base_uri), DEFAULT_REPO_BASE_URI);

    char device_name[32] = {0};
    bsp_device_get_name(device_name, sizeof(device_name) - 1);
    for (size_t i = 0; i < strlen(device_name); i++) {
        device_name[i] = tolower(device_name[i]);
    }

    char url[256];
    if (category != NULL) {
        sprintf(url, "%s%s/projects?category=%s&offset=%" PRIu32 "&amount=%" PRIu32 "&device=", base_url, base_uri,
                category, offset, amount);
    } else {
        sprintf(url, "%s%s/projects?offset=%" PRIu32 "&amount=%" PRIu32 "&device=", base_url, base_uri, offset, amount);
    }
    url_append(url, device_name, sizeof(url));
    return download_and_parse(url, out_data);
}

bool load_project(const char* base_url, repository_json_data_t* out_data, const char* project_slug) {
    char base_uri[64] = {0};
    nvs_settings_get_repo_base_uri(base_uri, sizeof(base_uri), DEFAULT_REPO_BASE_URI);
    char url[256];
    int  res = snprintf(url, sizeof(url), "%s%s/projects/%s", base_url, base_uri, project_slug);
    if (res < 0 || res >= sizeof(url)) {
        ESP_LOGE(TAG, "URL is too long");
        return false;
    }
    return download_and_parse(url, out_data);
}
