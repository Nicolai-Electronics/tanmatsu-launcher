#include "repository_client.h"
#include <stdio.h>
#include <string.h>
#include "bsp/device.h"
#include "cJSON.h"
#include "device_settings.h"
#include "http_download.h"
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

// Helper functions for API

bool load_information(const char* base_url, repository_json_data_t* out_data) {
    char base_uri[64] = {0};
    device_settings_get_repo_base_uri(base_uri, sizeof(base_uri));
    char url[256];
    sprintf(url, "%s%s/information", base_url, base_uri);
    bool success = download_ram(url, (uint8_t**)&out_data->data, &out_data->size, NULL, NULL);
    if (!success) return false;
    out_data->json = cJSON_ParseWithLength(out_data->data, out_data->size);
    if (out_data->json == NULL) {
        free(out_data->data);
        return false;
    }
    return true;
}

bool load_categories(const char* base_url, repository_json_data_t* out_data) {
    char base_uri[64] = {0};
    device_settings_get_repo_base_uri(base_uri, sizeof(base_uri));
    char url[256];
    char device_name[64] = {0};
    bsp_device_get_name(device_name, sizeof(device_name));
    sprintf(url, "%s%s/categories?device=%s", base_url, base_uri, device_name);
    bool success = download_ram(url, (uint8_t**)&out_data->data, &out_data->size, NULL, NULL);
    if (!success) return false;
    out_data->json = cJSON_ParseWithLength(out_data->data, out_data->size);
    if (out_data->json == NULL) {
        free(out_data->data);
        return false;
    }
    return true;
}

bool load_projects(const char* base_url, repository_json_data_t* out_data, const char* category) {
    char base_uri[64] = {0};
    device_settings_get_repo_base_uri(base_uri, sizeof(base_uri));
    char url[256];
    if (category != NULL) {
        sprintf(url, "%s%s/projects?device=%s&category=%s", base_url, base_uri, "tanmatsu", category);
    } else {
        sprintf(url, "%s%s/projects?device=%s", base_url, base_uri, "tanmatsu");
    }
    bool success = download_ram(url, (uint8_t**)&out_data->data, &out_data->size, NULL, NULL);
    if (!success) return false;
    out_data->json = cJSON_ParseWithLength(out_data->data, out_data->size);
    if (out_data->json == NULL) {
        free(out_data->data);
        return false;
    }
    return true;
}

bool load_projects_paginated(const char* base_url, repository_json_data_t* out_data, const char* category,
                             uint32_t offset, uint32_t amount) {
    char base_uri[64] = {0};
    device_settings_get_repo_base_uri(base_uri, sizeof(base_uri));
    char url[256];
    if (category != NULL) {
        sprintf(url, "%s%s/projects?device=%s&category=%s&offset=%" PRIu32 "&amount=%" PRIu32, base_url, base_uri,
                "tanmatsu", category, offset, amount);
    } else {
        sprintf(url, "%s%s/projects?device=%s&offset=%" PRIu32 "&amount=%" PRIu32, base_url, base_uri, "tanmatsu",
                offset, amount);
    }
    bool success = download_ram(url, (uint8_t**)&out_data->data, &out_data->size, NULL, NULL);
    if (!success) return false;
    out_data->json = cJSON_ParseWithLength(out_data->data, out_data->size);
    if (out_data->json == NULL) {
        free(out_data->data);
        return false;
    }
    return true;
}

bool load_project(const char* base_url, repository_json_data_t* out_data, const char* project_slug) {
    char base_uri[64] = {0};
    device_settings_get_repo_base_uri(base_uri, sizeof(base_uri));
    char url[256];
    sprintf(url, "%s%s/projects/%s", base_url, base_uri, project_slug);
    bool success = download_ram(url, (uint8_t**)&out_data->data, &out_data->size, NULL, NULL);
    if (!success) return false;
    out_data->json = cJSON_ParseWithLength(out_data->data, out_data->size);
    if (out_data->json == NULL) {
        free(out_data->data);
        return false;
    }
    return true;
}
