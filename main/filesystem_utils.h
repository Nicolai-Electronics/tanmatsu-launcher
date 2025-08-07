#include <stdbool.h>
#include <sys/stat.h>
#include <sys/unistd.h>
#include "esp_err.h"

bool      fs_utils_exists(const char* path);
bool      fs_utils_is_directory(const char* path);
bool      fs_utils_is_file(const char* path);
esp_err_t fs_utils_remove(const char* path);
size_t    fs_utils_get_file_size(FILE* fd);
uint8_t*  fs_utils_load_file_to_ram(FILE* fd);
