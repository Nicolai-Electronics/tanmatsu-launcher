#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "cJSON.h"
#include "http_download.h"

typedef struct {
    char*  data;
    size_t size;
    cJSON* json;
} repository_json_data_t;

void repository_client_prepare_device_query(void);

void free_repository_data_json(repository_json_data_t* data);
bool load_information(const char* base_url, repository_json_data_t* out_data);
// out_result, if non-NULL, is set to classify the failure reason (timeout, 4xx, 5xx, ...).
bool load_categories(const char* base_url, repository_json_data_t* out_data, http_result_t* out_result);
bool load_projects(const char* base_url, repository_json_data_t* out_data, const char* category,
                   http_result_t* out_result);
bool load_projects_paginated(const char* base_url, repository_json_data_t* out_data, const char* category,
                             uint32_t offset, uint32_t amount);
bool load_project(const char* base_url, repository_json_data_t* out_data, const char* project_slug);

// Builds a user-facing message explaining why loading `what` (e.g. "projects", "categories")
// from the server failed, with guidance specific to the failure category (timeout, 4xx, 5xx).
void repository_describe_load_failure(char* out, size_t out_len, const char* what, http_result_t result);

// Request a challenge (64 bytes) to be signed by the device identified by the given MAC address (6 bytes)
bool repository_signature_request(const char* base_url, const uint8_t* mac_address, uint8_t* out_challenge);
// Send the signature (64 bytes, r || s) of a previously requested challenge to the server for verification
bool repository_signature_verify(const char* base_url, const uint8_t* mac_address, const uint8_t* signature,
                                 bool* out_verified);
