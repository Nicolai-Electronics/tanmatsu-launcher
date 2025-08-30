#pragma once

#include "appfs.h"
#include "pax_types.h"

#define APP_MAX_NUM_CATEGORIES 16

typedef enum {
    EXECUTABLE_TYPE_UNKNOWN = 0,
    EXECUTABLE_TYPE_APPFS,
    EXECUTABLE_TYPE_ELF,
    EXECUTABLE_TYPE_SCRIPT,
} executable_type_t;

typedef struct {
    // Filesystem
    char* path;
    char* slug;

    // Metadata
    char*      name;
    char*      description;
    char*      categories[APP_MAX_NUM_CATEGORIES];
    uint32_t   version;
    pax_buf_t* icon;
    char*      author;
    char*      license_type;
    char*      license_file;
    char*      repository;

    executable_type_t executable_type;
    uint32_t          executable_revision;
    char*             executable_filename;
    char*             executable_interpreter_slug;
    appfs_handle_t    executable_appfs_fd;
} app_t;

appfs_handle_t find_appfs_handle_for_slug(const char* search_slug);
app_t*         create_app(const char* path, const char* slug);
void           free_app(app_t* app);

size_t create_list_of_apps_from_directory(app_t** out_list, size_t list_size, const char* path, app_t** full_list,
                                          size_t full_list_size);
size_t create_list_of_apps_from_other_appfs_entries(app_t** out_list, size_t list_size, app_t** full_list,
                                                    size_t full_list_size);
size_t create_list_of_apps(app_t** out_list, size_t list_size);
void   free_list_of_apps(app_t** out_list, size_t list_size);
