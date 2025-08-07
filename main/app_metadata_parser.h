#pragma once

#include "appfs.h"
#include "pax_types.h"

typedef struct {
    // Filesystem
    char* path;

    // Metadata
    char*      slug;
    char*      name;
    char*      description;
    char*      author;
    char*      license;
    char*      main;
    char*      interpreter;
    uint32_t   version;
    pax_buf_t* icon;

    // AppFS
    appfs_handle_t appfs_fd;
} app_t;

app_t* create_app(const char* path, const char* slug);
void   free_app(app_t* app);

size_t create_list_of_apps_from_directory(app_t** out_list, size_t list_size, const char* path, app_t** full_list,
                                          size_t full_list_size);
size_t create_list_of_apps_from_other_appfs_entries(app_t** out_list, size_t list_size, app_t** full_list,
                                                    size_t full_list_size);
size_t create_list_of_apps(app_t** out_list, size_t list_size);
void   free_list_of_apps(app_t** out_list, size_t list_size);
