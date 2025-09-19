#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "cJSON.h"

typedef struct {
    char*  data;
    size_t size;
    cJSON* json;
} repository_json_data_t;

void free_repository_data_json(repository_json_data_t* data);
bool load_information(const char* base_url, repository_json_data_t* out_data);
bool load_categories(const char* base_url, repository_json_data_t* out_data);
bool load_projects(const char* base_url, repository_json_data_t* out_data, const char* category);
bool load_projects_paginated(const char* base_url, repository_json_data_t* out_data, const char* category,
                             uint32_t offset, uint32_t amount);
bool load_project(const char* base_url, repository_json_data_t* out_data, const char* project_slug);
