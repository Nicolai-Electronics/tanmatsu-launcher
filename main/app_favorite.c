#include "app_favorite.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_err.h"
#include "esp_log.h"
#include "nvs.h"

#define MAX_SLUG_SIZE 48

static const char* TAG       = "app_favorite";
static const char* NAMESPACE = "app_fav";
static const char* PREFIX    = "e_";

static void make_key(char* out, size_t out_size, uint16_t index) {
    snprintf(out, out_size, "%s%d", PREFIX, index);
}

static esp_err_t read_entry(nvs_handle_t handle, uint16_t index, char* out_slug) {
    if (out_slug == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    char key[16];
    make_key(key, sizeof(key), index);

    size_t    size = 0;
    esp_err_t res  = nvs_get_str(handle, key, NULL, &size);
    if (res != ESP_OK) {
        return res;
    }

    if (size > MAX_SLUG_SIZE) {
        return ESP_ERR_NO_MEM;
    }

    return nvs_get_str(handle, key, out_slug, &size);
}

static esp_err_t write_entry(nvs_handle_t handle, int index, const char* slug) {
    char key[16];
    make_key(key, sizeof(key), index);
    return nvs_set_str(handle, key, slug);
}

static esp_err_t erase_entry(nvs_handle_t handle, int index) {
    char key[16];
    make_key(key, sizeof(key), index);
    return nvs_erase_key(handle, key);
}

bool app_favorite_get(const char* slug) {
    if (slug == NULL || slug[0] == '\0') {
        return false;
    }

    nvs_handle_t handle;
    esp_err_t    res = nvs_open(NAMESPACE, NVS_READONLY, &handle);
    if (res != ESP_OK) {
        return false;
    }

    bool found                = false;
    int  index                = 0;
    char entry[MAX_SLUG_SIZE] = "";
    while (read_entry(handle, index, entry) == ESP_OK && (!found)) {
        if (strcmp(entry, slug) == 0) {
            found = true;
        }
        index++;
    }

    nvs_close(handle);
    return found;
}

void app_favorite_set(const char* slug, bool favorite) {
    if (slug == NULL || slug[0] == '\0') {
        return;
    }

    nvs_handle_t handle;
    esp_err_t    res = nvs_open(NAMESPACE, NVS_READWRITE, &handle);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(res));
        return;
    }

    if (favorite) {
        // Find the index one past the last existing entry.
        int  index                = 0;
        char entry[MAX_SLUG_SIZE] = "";
        while (read_entry(handle, index, entry) == ESP_OK) {
            index++;
        }

        res = write_entry(handle, index, slug);
        if (res == ESP_OK) {
            nvs_commit(handle);
        } else {
            ESP_LOGE(TAG, "Failed to store favorite '%s': %s", slug, esp_err_to_name(res));
        }
    } else {
        // Find the matching entry while counting the total number of entries.
        int  index       = 0;
        int  found_index = -1;
        char entry[48]   = "";
        while (read_entry(handle, index, entry) == ESP_OK) {
            if (found_index < 0 && strcmp(entry, slug) == 0) {
                found_index = index;
            }
            index++;
        }

        if (found_index >= 0) {
            int last_index = index - 1;
            // Shift every following entry down by one to close the gap.
            for (int i = found_index; i < last_index; i++) {
                if (read_entry(handle, i + 1, entry) == ESP_OK) {
                    write_entry(handle, i, entry);
                }
            }
            erase_entry(handle, last_index);
            nvs_commit(handle);
        }
    }

    nvs_close(handle);
}
