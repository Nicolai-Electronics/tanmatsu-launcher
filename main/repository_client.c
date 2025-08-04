#include "repository_client.h"
#include <stdio.h>
#include <string.h>
#include "cJSON.h"
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
    char url[128];
    sprintf(url, "%s/v1/information", base_url);
    bool success = download_ram(url, (uint8_t**)&out_data->data, &out_data->size);
    if (!success) return false;
    out_data->json = cJSON_ParseWithLength(out_data->data, out_data->size);
    if (out_data->json == NULL) {
        free(out_data->data);
        return false;
    }
    return true;
}

bool load_categories(const char* base_url, repository_json_data_t* out_data) {
    char url[128];
    sprintf(url, "%s/v1/categories?device=%s", base_url, "tanmatsu");
    bool success = download_ram(url, (uint8_t**)&out_data->data, &out_data->size);
    if (!success) return false;
    out_data->json = cJSON_ParseWithLength(out_data->data, out_data->size);
    if (out_data->json == NULL) {
        free(out_data->data);
        return false;
    }
    return true;
}

bool load_projects(const char* base_url, repository_json_data_t* out_data, const char* category) {
    char url[128];
    if (category != NULL) {
        sprintf(url, "%s/v1/projects?device=%s&category=%s", base_url, "tanmatsu", category);
    } else {
        sprintf(url, "%s/v1/projects?device=%s", base_url, "tanmatsu");
    }
    bool success = download_ram(url, (uint8_t**)&out_data->data, &out_data->size);
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
    char url[128];
    if (category != NULL) {
        sprintf(url, "%s/v1/projects?device=%s&category=%s&offset=%" PRIu32 "&amount=%" PRIu32, base_url, "tanmatsu",
                category, offset, amount);
    } else {
        sprintf(url, "%s/v1/projects?device=%s&offset=%" PRIu32 "&amount=%" PRIu32, base_url, "tanmatsu", offset,
                amount);
    }
    bool success = download_ram(url, (uint8_t**)&out_data->data, &out_data->size);
    if (!success) return false;
    out_data->json = cJSON_ParseWithLength(out_data->data, out_data->size);
    if (out_data->json == NULL) {
        free(out_data->data);
        return false;
    }
    return true;
}

bool load_project(const char* base_url, repository_json_data_t* out_data, const char* project_slug) {
    char url[128];
    sprintf(url, "%s/v1/projects/%s", base_url, project_slug);
    bool success = download_ram(url, (uint8_t**)&out_data->data, &out_data->size);
    if (!success) return false;
    out_data->json = cJSON_ParseWithLength(out_data->data, out_data->size);
    if (out_data->json == NULL) {
        free(out_data->data);
        return false;
    }
    return true;
}
