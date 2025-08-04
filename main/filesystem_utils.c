#include "filesystem_utils.h"
#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/unistd.h>
#include "esp_err.h"
#include "sys/dirent.h"

bool fs_utils_exists(const char* path) {
    struct stat stat_path;
    return stat(path, &stat_path) == 0;
}

bool fs_utils_is_directory(const char* path) {
    struct stat stat_path;
    if (stat(path, &stat_path) != 0) {
        return false;
    }
    return S_ISDIR(stat_path.st_mode);
}

bool fs_utils_is_file(const char* path) {
    struct stat stat_path;
    if (stat(path, &stat_path) != 0) {
        return false;
    }
    return S_ISREG(stat_path.st_mode);
}

esp_err_t fs_utils_remove(const char* path) {
    DIR*           dir;
    struct stat    stat_path, stat_entry;
    struct dirent* entry;

    stat(path, &stat_path);

    // Remove file if path is not a directory
    if (S_ISDIR(stat_path.st_mode) == 0) {
        // Remove file
        if (unlink(path) == 0) {
            return ESP_OK;
        } else {
            return ESP_FAIL;
        }
    }

    // Open directory
    if ((dir = opendir(path)) == NULL) {
        // Can't open directory
        return false;
    }

    // Remove all files from the directory and enter any subdirectories recursively
    bool   failed   = false;
    size_t path_len = strlen(path);
    while ((entry = readdir(dir)) != NULL) {
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) {
            continue;
        }

        char* full_path = calloc(path_len + strlen(entry->d_name) + 2, sizeof(char));
        strcpy(full_path, path);
        strcat(full_path, "/");
        strcat(full_path, entry->d_name);

        stat(full_path, &stat_entry);

        if (S_ISDIR(stat_entry.st_mode) != 0) {
            if (fs_utils_remove(full_path) != ESP_OK) {
                failed = true;
            }
            continue;
        }

        if (unlink(full_path) != 0) {
            failed = true;
        }
        free(full_path);
    }

    // Remove the directory itself
    if (rmdir(path) != 0) {
        failed = true;
    }

    closedir(dir);

    return failed ? ESP_FAIL : ESP_OK;
}

size_t fs_utils_get_file_size(FILE* fd) {
    fseek(fd, 0, SEEK_END);
    size_t fsize = ftell(fd);
    fseek(fd, 0, SEEK_SET);
    return fsize;
}

uint8_t* fs_utils_load_file_to_ram(FILE* fd) {
    fseek(fd, 0, SEEK_END);
    size_t fsize = ftell(fd);
    fseek(fd, 0, SEEK_SET);
    uint8_t* file = malloc(fsize);
    if (file == NULL) return NULL;
    fread(file, fsize, 1, fd);
    return file;
}
