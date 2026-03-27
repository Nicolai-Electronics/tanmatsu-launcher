#include "filesystem_utils.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/unistd.h>
#include "esp_err.h"
#include "fastopen.h"
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

static esp_err_t copy_single_file(const char* src, const char* dst) {
    FILE* f_src = fastopen(src, "rb");
    if (f_src == NULL) {
        return ESP_FAIL;
    }

    FILE* f_dst = fastopen(dst, "wb");
    if (f_dst == NULL) {
        fastclose(f_src);
        return ESP_FAIL;
    }

    uint8_t* buf = malloc(4096);
    if (buf == NULL) {
        fastclose(f_src);
        fastclose(f_dst);
        return ESP_ERR_NO_MEM;
    }

    esp_err_t result = ESP_OK;
    size_t    bytes_read;
    while ((bytes_read = fread(buf, 1, 4096, f_src)) > 0) {
        if (fwrite(buf, 1, bytes_read, f_dst) != bytes_read) {
            result = ESP_FAIL;
            break;
        }
    }

    free(buf);
    fastclose(f_src);
    fastclose(f_dst);
    return result;
}

esp_err_t fs_utils_copy_recursive(const char* src, const char* dst) {
    struct stat stat_src;
    if (stat(src, &stat_src) != 0) {
        return ESP_ERR_NOT_FOUND;
    }

    // If source is a file, copy it directly
    if (!S_ISDIR(stat_src.st_mode)) {
        return copy_single_file(src, dst);
    }

    // Create destination directory
    if (mkdir(dst, 0777) != 0) {
        // Check if it already exists
        if (!fs_utils_is_directory(dst)) {
            return ESP_FAIL;
        }
    }

    DIR* dir = opendir(src);
    if (dir == NULL) {
        return ESP_FAIL;
    }

    struct dirent* entry;
    esp_err_t      result = ESP_OK;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        size_t src_len = strlen(src) + strlen(entry->d_name) + 2;
        size_t dst_len = strlen(dst) + strlen(entry->d_name) + 2;
        char*  src_path = malloc(src_len);
        char*  dst_path = malloc(dst_len);
        if (src_path == NULL || dst_path == NULL) {
            free(src_path);
            free(dst_path);
            result = ESP_ERR_NO_MEM;
            break;
        }

        snprintf(src_path, src_len, "%s/%s", src, entry->d_name);
        snprintf(dst_path, dst_len, "%s/%s", dst, entry->d_name);

        result = fs_utils_copy_recursive(src_path, dst_path);
        free(src_path);
        free(dst_path);

        if (result != ESP_OK) {
            break;
        }
    }

    closedir(dir);
    return result;
}

uint64_t fs_utils_get_directory_size(const char* path) {
    struct stat st;
    if (stat(path, &st) != 0) {
        return 0;
    }

    if (!S_ISDIR(st.st_mode)) {
        return st.st_size;
    }

    DIR* dir = opendir(path);
    if (dir == NULL) {
        return 0;
    }

    uint64_t       total = 0;
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        size_t full_len  = strlen(path) + strlen(entry->d_name) + 2;
        char*  full_path = malloc(full_len);
        if (full_path == NULL) {
            break;
        }
        snprintf(full_path, full_len, "%s/%s", path, entry->d_name);
        total += fs_utils_get_directory_size(full_path);
        free(full_path);
    }

    closedir(dir);
    return total;
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
