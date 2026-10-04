#include "repository_client.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
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

// Signature API

#define SIGNATURE_CHALLENGE_LENGTH 64
#define SIGNATURE_LENGTH           64
#define MAC_ADDRESS_LENGTH         6

static void bytes_to_hex(const uint8_t* data, size_t length, char* out_hex) {
    for (size_t i = 0; i < length; i++) {
        sprintf(&out_hex[i * 2], "%02x", data[i]);
    }
    out_hex[length * 2] = '\0';
}

static bool hex_to_bytes(const char* hex, uint8_t* out_data, size_t length) {
    if (strlen(hex) != length * 2) {
        return false;
    }
    for (size_t i = 0; i < length; i++) {
        if (!isxdigit((unsigned char)hex[i * 2]) || !isxdigit((unsigned char)hex[i * 2 + 1])) {
            return false;
        }
        char byte_hex[3] = {hex[i * 2], hex[i * 2 + 1], '\0'};
        out_data[i]      = (uint8_t)strtoul(byte_hex, NULL, 16);
    }
    return true;
}

static bool post_and_parse(const char* url, cJSON* request, repository_json_data_t* out_data) {
    free_repository_data_json(out_data);
    char* body = cJSON_PrintUnformatted(request);
    if (body == NULL) return false;
    http_session_t session = http_session_begin(url);
    if (session == NULL) {
        cJSON_free(body);
        return false;
    }
    bool success = http_session_post_ram(session, url, "application/json", body, strlen(body),
                                         (uint8_t**)&out_data->data, &out_data->size);
    http_session_end(session);
    cJSON_free(body);
    if (!success) return false;
    out_data->json = cJSON_ParseWithLength(out_data->data, out_data->size);
    if (out_data->json == NULL) {
        free(out_data->data);
        out_data->data = NULL;
        return false;
    }
    return true;
}

static bool build_signature_url(const char* base_url, const char* endpoint, char* url, size_t url_length) {
    char base_uri[64] = {0};
    nvs_settings_get_repo_base_uri(base_uri, sizeof(base_uri), DEFAULT_REPO_BASE_URI);
    int res = snprintf(url, url_length, "%s%s/signature/%s", base_url, base_uri, endpoint);
    if (res < 0 || res >= url_length) {
        ESP_LOGE(TAG, "URL is too long");
        return false;
    }
    return true;
}

bool repository_signature_request(const char* base_url, const uint8_t* mac_address, uint8_t* out_challenge) {
    char url[256];
    if (!build_signature_url(base_url, "request", url, sizeof(url))) {
        return false;
    }

    char mac_hex[MAC_ADDRESS_LENGTH * 2 + 1];
    bytes_to_hex(mac_address, MAC_ADDRESS_LENGTH, mac_hex);

    cJSON* request = cJSON_CreateObject();
    if (request == NULL) return false;
    cJSON_AddStringToObject(request, "mac_address", mac_hex);

    repository_json_data_t response = {0};
    bool                   success  = post_and_parse(url, request, &response);
    cJSON_Delete(request);
    if (!success) {
        ESP_LOGE(TAG, "Failed to request signature challenge");
        return false;
    }

    cJSON* challenge = cJSON_GetObjectItem(response.json, "challenge");
    success = cJSON_IsString(challenge) && hex_to_bytes(challenge->valuestring, out_challenge, SIGNATURE_CHALLENGE_LENGTH);
    if (!success) {
        ESP_LOGE(TAG, "Invalid signature challenge received");
    }
    free_repository_data_json(&response);
    return success;
}

bool repository_signature_verify(const char* base_url, const uint8_t* mac_address, const uint8_t* signature,
                                 bool* out_verified) {
    char url[256];
    if (!build_signature_url(base_url, "verify", url, sizeof(url))) {
        return false;
    }

    char mac_hex[MAC_ADDRESS_LENGTH * 2 + 1];
    bytes_to_hex(mac_address, MAC_ADDRESS_LENGTH, mac_hex);
    char signature_hex[SIGNATURE_LENGTH * 2 + 1];
    bytes_to_hex(signature, SIGNATURE_LENGTH, signature_hex);

    cJSON* request = cJSON_CreateObject();
    if (request == NULL) return false;
    cJSON_AddStringToObject(request, "mac_address", mac_hex);
    cJSON_AddStringToObject(request, "signature", signature_hex);

    repository_json_data_t response = {0};
    bool                   success  = post_and_parse(url, request, &response);
    cJSON_Delete(request);
    if (!success) {
        ESP_LOGE(TAG, "Failed to verify signature");
        return false;
    }

    cJSON* verified = cJSON_GetObjectItem(response.json, "verified");
    success         = cJSON_IsBool(verified);
    if (success) {
        *out_verified = cJSON_IsTrue(verified);
    } else {
        ESP_LOGE(TAG, "Invalid signature verification response received");
    }
    free_repository_data_json(&response);
    return success;
}
