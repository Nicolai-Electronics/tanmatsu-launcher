#include "addon.h"
#include "bsp/i2c.h"
#include "driver/i2c_master.h"
#include "driver/i2c_types.h"
#include "eeprom.h"
#include "esp_err.h"
#include "esp_log.h"
#include "sdkconfig.h"

// Descriptor structures

typedef struct __attribute__((__packed__)) {
    uint8_t magic[4];
    uint8_t name_length;
    uint8_t driver_name_length;
    uint8_t driver_data_length;
    uint8_t number_of_extra_drivers;
} sao_binary_header_t;

typedef struct __attribute__((__packed__)) {
    uint8_t driver_name_length;
    uint8_t driver_data_length;
} sao_binary_extra_driver_t;

typedef struct __attribute__((__packed__)) {
    uint8_t magic[4];
    char    manifest_version[4];
    struct {
        uint8_t offset[2];
        uint8_t page_size[2];
        uint8_t total_size[4];
    } filesystem_info;
    uint8_t vendor_id[2];
    uint8_t product_id[2];
    uint8_t unique_id[2];
    char    name[9];
    uint8_t checksum;
} catt_header_t;

// Constants & variables

static const char TAG[] = "Add-on";

static i2c_master_bus_handle_t internal_i2c_bus_handle    = NULL;
static SemaphoreHandle_t       internal_i2c_bus_semaphore = NULL;
static eeprom_handle_t         internal_eeprom_handle     = {0};
static addon_descriptor_t*     internal_addon_descriptor  = NULL;

static i2c_master_bus_handle_t external_i2c_bus_handle    = NULL;
static SemaphoreHandle_t       external_i2c_bus_semaphore = NULL;
static eeprom_handle_t         external_eeprom_handle     = {0};
static addon_descriptor_t*     external_addon_descriptor  = NULL;

static addon_descriptor_t* addon_internal = NULL;
static addon_descriptor_t* addon_catt     = NULL;

// Helper functions

static esp_err_t addon_parse_binary_sao_descriptor(eeprom_handle_t* eeprom, addon_descriptor_t* descriptor) {
    if (descriptor == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    sao_binary_header_t header = {0};
    esp_err_t           res    = eeprom_read(eeprom, 0x00, (uint8_t*)&header, sizeof(sao_binary_header_t));
    if (res != ESP_OK) {
        return res;
    }

    uint8_t address = sizeof(sao_binary_header_t);

    descriptor->binary_sao.amount_of_drivers = 1 + header.number_of_extra_drivers;

    // Allocate memory
    descriptor->binary_sao.name = calloc(header.name_length + 1, sizeof(char));
    if (descriptor->binary_sao.name == NULL) {
        return ESP_ERR_NO_MEM;
    }
    descriptor->binary_sao.drivers =
        calloc(descriptor->binary_sao.amount_of_drivers, sizeof(addon_binary_sao_driver_t));
    if (descriptor->binary_sao.drivers == NULL) {
        free(descriptor->binary_sao.name);
        return ESP_ERR_NO_MEM;
    }

    // Read name
    res = eeprom_read(eeprom, address, (uint8_t*)descriptor->binary_sao.name, header.name_length);
    if (res != ESP_OK) {
        free(descriptor->binary_sao.name);
        free(descriptor->binary_sao.drivers);
        return res;
    }

    // Read primary driver data
    address                                += header.name_length;
    descriptor->binary_sao.drivers[0].name  = calloc(header.driver_name_length + 1, sizeof(char));
    if (descriptor->binary_sao.drivers[0].name == NULL) {
        free(descriptor->binary_sao.name);
        free(descriptor->binary_sao.drivers);
        return ESP_ERR_NO_MEM;
    }

    res = eeprom_read(eeprom, address, (uint8_t*)descriptor->binary_sao.drivers[0].name, header.driver_name_length);
    if (res != ESP_OK) {
        free(descriptor->binary_sao.name);
        free(descriptor->binary_sao.drivers[0].name);
        free(descriptor->binary_sao.drivers);
        return res;
    }

    address                                       += header.driver_name_length;
    descriptor->binary_sao.drivers[0].data_length  = header.driver_data_length;
    descriptor->binary_sao.drivers[0].data         = malloc(header.driver_data_length);
    if (descriptor->binary_sao.drivers[0].data == NULL) {
        free(descriptor->binary_sao.name);
        free(descriptor->binary_sao.drivers[0].name);
        free(descriptor->binary_sao.drivers);
        return ESP_ERR_NO_MEM;
    }

    res = eeprom_read(eeprom, address, descriptor->binary_sao.drivers[0].data, header.driver_data_length);
    if (res != ESP_OK) {
        free(descriptor->binary_sao.name);
        free(descriptor->binary_sao.drivers[0].name);
        free(descriptor->binary_sao.drivers[0].data);
        free(descriptor->binary_sao.drivers);
        return res;
    }

    address += header.driver_data_length;

    // Read extra drivers
    for (uint8_t i = 1; i < descriptor->binary_sao.amount_of_drivers; i++) {
        sao_binary_extra_driver_t extra_driver_header = {0};
        res = eeprom_read(eeprom, address, (uint8_t*)&extra_driver_header, sizeof(extra_driver_header));
        if (res != ESP_OK) {
            // Free previously allocated memory
            for (uint8_t j = 0; j <= i - 1; j++) {
                free(descriptor->binary_sao.drivers[j].name);
                free(descriptor->binary_sao.drivers[j].data);
            }
            free(descriptor->binary_sao.name);
            free(descriptor->binary_sao.drivers);
            return res;
        }
        address                                += sizeof(extra_driver_header);
        descriptor->binary_sao.drivers[i].name  = calloc(extra_driver_header.driver_name_length + 1, sizeof(char));
        if (descriptor->binary_sao.drivers[i].name == NULL) {
            // Free previously allocated memory
            for (uint8_t j = 0; j < i - 1; j++) {
                free(descriptor->binary_sao.drivers[j].name);
                free(descriptor->binary_sao.drivers[j].data);
            }
            free(descriptor->binary_sao.name);
            free(descriptor->binary_sao.drivers);
            return ESP_ERR_NO_MEM;
        }
        descriptor->binary_sao.drivers[i].data = malloc(extra_driver_header.driver_data_length);
        if (descriptor->binary_sao.drivers[i].data == NULL) {
            // Free previously allocated memory
            for (uint8_t j = 0; j < i - 1; j++) {
                free(descriptor->binary_sao.drivers[j].name);
                free(descriptor->binary_sao.drivers[j].data);
            }
            free(descriptor->binary_sao.drivers[i].name);
            free(descriptor->binary_sao.name);
            free(descriptor->binary_sao.drivers);
            return ESP_ERR_NO_MEM;
        }
        // Read extra driver name
        res      = eeprom_read(eeprom, address, (uint8_t*)descriptor->binary_sao.drivers[i].name,
                               extra_driver_header.driver_name_length);
        address += extra_driver_header.driver_name_length;
        if (res == ESP_OK) {
            // Read extra driver data
            descriptor->binary_sao.drivers[i].data_length = extra_driver_header.driver_data_length;
            res      = eeprom_read(eeprom, address, descriptor->binary_sao.drivers[i].data,
                                   extra_driver_header.driver_data_length);
            address += extra_driver_header.driver_data_length;
        }
        if (res != ESP_OK) {
            // Free previously allocated memory
            for (uint8_t j = 0; j <= i; j++) {
                free(descriptor->binary_sao.drivers[j].name);
                free(descriptor->binary_sao.drivers[j].data);
            }
            free(descriptor->binary_sao.name);
            free(descriptor->binary_sao.drivers);
            return res;
        }
    }

    descriptor->descriptor_type = ADDON_TYPE_BINARY_SAO;

    return ESP_OK;
}

static esp_err_t addon_parse_json_descriptor(eeprom_handle_t* eeprom, addon_descriptor_t* descriptor) {
    if (descriptor == NULL || descriptor->descriptor_type != ADDON_TYPE_JSON) {
        return ESP_ERR_INVALID_ARG;
    }

    // Read the size of the JSON data
    uint8_t   json_size = 0;
    esp_err_t res       = eeprom_read(eeprom, 0x04, &json_size, sizeof(uint8_t));
    if (res != ESP_OK) {
        return res;
    }

    // Allocate memory for JSON data
    descriptor->json.json_text = calloc(json_size + 1, sizeof(char));  // +1 for null terminator
    if (descriptor->json.json_text == NULL) {
        return ESP_ERR_NO_MEM;
    }

    // Read JSON data
    res = eeprom_read(eeprom, 0x05, (uint8_t*)descriptor->json.json_text, json_size);
    if (res != ESP_OK) {
        free(descriptor->json.json_text);
        return res;
    }

    descriptor->descriptor_type = ADDON_TYPE_JSON;

    return ESP_OK;
}

static esp_err_t addon_parse_hexpansion_catt_descriptor(eeprom_handle_t* eeprom, addon_descriptor_t* descriptor,
                                                        addon_descriptor_type_t type) {
    if (descriptor == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (type != ADDON_TYPE_HEXPANSION && type != ADDON_TYPE_CATT) {
        return ESP_ERR_NOT_SUPPORTED;
    }

    // Read manifest version
    catt_header_t header = {0};
    esp_err_t     res    = eeprom_read(eeprom, 0x00, (uint8_t*)&header, sizeof(catt_header_t));
    if (res != ESP_OK) return res;

    if (type == ADDON_TYPE_HEXPANSION && memcmp(descriptor->catt.manifest_version, "2024", 4) != 0) {
        return ESP_ERR_NOT_SUPPORTED;
    } else if (type == ADDON_TYPE_CATT && memcmp(descriptor->catt.manifest_version, "0001", 4) != 0) {
        return ESP_ERR_NOT_SUPPORTED;
    }

    uint8_t calculated_checksum = 0x55;
    for (size_t i = 1; i < sizeof(catt_header_t); i++) {
        calculated_checksum ^= ((uint8_t*)&header)[i];
    }
    if (calculated_checksum != header.checksum) {
        return ESP_ERR_INVALID_CRC;
    }

    memcpy(descriptor->catt.manifest_version, header.manifest_version, 4);
    descriptor->catt.filesystem_info.offset =
        (header.filesystem_info.offset[0] << 8) | header.filesystem_info.offset[1];
    descriptor->catt.filesystem_info.page_size =
        (header.filesystem_info.page_size[0] << 8) | header.filesystem_info.page_size[1];
    descriptor->catt.filesystem_info.total_size =
        (header.filesystem_info.total_size[0] << 24) | (header.filesystem_info.total_size[1] << 16) |
        (header.filesystem_info.total_size[2] << 8) | header.filesystem_info.total_size[3];
    descriptor->catt.vendor_id  = (header.vendor_id[0] << 8) | header.vendor_id[1];
    descriptor->catt.product_id = (header.product_id[0] << 8) | header.product_id[1];
    descriptor->catt.unique_id  = (header.unique_id[0] << 8) | header.unique_id[1];
    memcpy(descriptor->catt.name, header.name, 9);

    return ESP_OK;
}

static esp_err_t addon_detect(i2c_master_bus_handle_t bus, SemaphoreHandle_t semaphore, eeprom_handle_t* eeprom,
                              addon_location_t location, addon_descriptor_t** out_descriptor) {
    if (*out_descriptor != NULL) {
        // Already detected
        return ESP_OK;
    }

    if (eeprom->i2c_device == NULL) {
        // Initialize EEPROM driver
        esp_err_t res = eeprom_init(eeprom, bus, semaphore, 0x50, 16, false);
        if (res != ESP_OK) {
            return ESP_ERR_NOT_FOUND;
        }
    }

    char      magic[4] = "";
    esp_err_t res      = eeprom_read(eeprom, 0x00, (uint8_t*)magic, sizeof(magic));
    if (res != ESP_OK) {
        return res;
    }

    *out_descriptor = calloc(1, sizeof(addon_descriptor_t));
    if (*out_descriptor == NULL) {
        return ESP_ERR_NO_MEM;
    }

    (*out_descriptor)->location        = location;
    (*out_descriptor)->descriptor_type = ADDON_TYPE_UNKNOWN;

    if (memcmp(magic, "\0\0\0\0", sizeof(magic)) == 0 || memcmp(magic, "\xFF\xFF\xFF\xFF", sizeof(magic)) == 0) {
        (*out_descriptor)->descriptor_type = ADDON_TYPE_UNINITIALIZED;
    } else if (memcmp(magic, "LIFE", sizeof(magic)) == 0) {
        addon_parse_binary_sao_descriptor(eeprom, *out_descriptor);
    } else if (memcmp(magic, "JSON", sizeof(magic)) == 0) {
        addon_parse_json_descriptor(eeprom, *out_descriptor);
    } else if (memcmp(magic, "THEX", sizeof(magic)) == 0) {
        addon_parse_hexpansion_catt_descriptor(eeprom, *out_descriptor, ADDON_TYPE_HEXPANSION);
    } else if (memcmp(magic, "CATT", sizeof(magic)) == 0) {
        addon_parse_hexpansion_catt_descriptor(eeprom, *out_descriptor, ADDON_TYPE_CATT);
    }

    return ESP_OK;
}

// Public functions

esp_err_t addon_print_descriptor(addon_descriptor_t* descriptor) {
    if (descriptor == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    const char* location_str = descriptor->location == ADDON_LOCATION_INTERNAL ? "Internal" : "External";
    const char* type_str     = "Unknown";

    switch (descriptor->descriptor_type) {
        case ADDON_TYPE_UNINITIALIZED:
            type_str = "uninitialized";
            break;
        case ADDON_TYPE_BINARY_SAO:
            type_str = "binary SAO";
            break;
        case ADDON_TYPE_JSON:
            type_str = "JSON";
            break;
        case ADDON_TYPE_HEXPANSION:
            type_str = "Hexpansion";
            break;
        case ADDON_TYPE_CATT:
            type_str = "CATT";
            break;
        default:
            break;
    }

    ESP_LOGI(TAG, "%s %s add-on", location_str, type_str);

    if (descriptor->descriptor_type == ADDON_TYPE_BINARY_SAO) {
        ESP_LOGI(TAG, "  Name: %s", descriptor->binary_sao.name);
        ESP_LOGI(TAG, "  Number of drivers: %d", descriptor->binary_sao.amount_of_drivers);
        for (uint8_t i = 0; i < descriptor->binary_sao.amount_of_drivers; i++) {
            ESP_LOGI(TAG, "    Driver %d:", i + 1);
            ESP_LOGI(TAG, "      Name: %s", descriptor->binary_sao.drivers[i].name);
            ESP_LOGI(TAG, "      Data length: %d bytes", descriptor->binary_sao.drivers[i].data_length);
            ESP_LOGI(TAG, "      Data (hex):");
            printf("                ");
            for (uint8_t j = 0; j < descriptor->binary_sao.drivers[i].data_length; j++) {
                printf("%02X ", descriptor->binary_sao.drivers[i].data[j]);
            }
            printf("\r\n");
        }
    }

    if (descriptor->descriptor_type == ADDON_TYPE_JSON) {
        ESP_LOGI(TAG, "  JSON data: %s", descriptor->json.json_text);
    }

    if (descriptor->descriptor_type == ADDON_TYPE_HEXPANSION || descriptor->descriptor_type == ADDON_TYPE_CATT) {
        ESP_LOGI(TAG, "  Manifest version:      %.4s", descriptor->catt.manifest_version);
        ESP_LOGI(TAG, "  Name:                  %.9s", descriptor->catt.name);
        ESP_LOGI(TAG, "  Vendor ID:             0x%04X", descriptor->catt.vendor_id);
        ESP_LOGI(TAG, "  Product ID:            0x%04X", descriptor->catt.product_id);
        ESP_LOGI(TAG, "  Unique ID:             0x%04X", descriptor->catt.unique_id);
        ESP_LOGI(TAG, "  Filesystem offset:     0x%04X", descriptor->catt.filesystem_info.offset);
        ESP_LOGI(TAG, "  Filesystem page size:  %d bytes", descriptor->catt.filesystem_info.page_size);
        ESP_LOGI(TAG, "  Filesystem total size: %d bytes", descriptor->catt.filesystem_info.total_size);
    }

    return ESP_OK;
}

esp_err_t addon_initialize(void) {
    esp_err_t res = bsp_i2c_primary_bus_get_handle(&internal_i2c_bus_handle);
    if (res == ESP_OK) {
        // Primary I2C bus available
        bsp_i2c_primary_bus_get_semaphore(&internal_i2c_bus_semaphore);
        res = addon_detect(internal_i2c_bus_handle, internal_i2c_bus_semaphore, &internal_eeprom_handle,
                           ADDON_LOCATION_INTERNAL, &internal_addon_descriptor);
        if (res == ESP_OK) {
            ESP_LOGI(TAG, "Internal add-on detected");
            addon_print_descriptor(internal_addon_descriptor);
        } else {
            ESP_LOGI(TAG, "No internal add-on detected");
        }
    }

#ifdef CONFIG_BSP_TARGET_TANMATSU
    i2c_master_bus_config_t sao_i2c_master_config = {
        .clk_source                   = I2C_CLK_SRC_DEFAULT,
        .i2c_port                     = 1,
        .scl_io_num                   = 13,
        .sda_io_num                   = 12,
        .glitch_ignore_cnt            = 7,
        .flags.enable_internal_pullup = true,
    };

    external_i2c_bus_semaphore = xSemaphoreCreateBinary();
    if (external_i2c_bus_semaphore != NULL) {
        xSemaphoreGive(external_i2c_bus_semaphore);
        res = i2c_new_master_bus(&sao_i2c_master_config, &external_i2c_bus_handle);
        if (res == ESP_OK) {
            res = addon_detect(external_i2c_bus_handle, external_i2c_bus_semaphore, &external_eeprom_handle,
                               ADDON_LOCATION_EXTERNAL, &external_addon_descriptor);
            if (res == ESP_OK) {
                ESP_LOGI(TAG, "External add-on detected");
                addon_print_descriptor(external_addon_descriptor);
            } else {
                ESP_LOGI(TAG, "No external add-on detected");
                i2c_del_master_bus(external_i2c_bus_handle);
                external_i2c_bus_handle = NULL;
                vSemaphoreDelete(external_i2c_bus_semaphore);
                external_i2c_bus_semaphore = NULL;
            }
        }
    }
#endif

    return ESP_OK;
}

esp_err_t addon_get_descriptor(addon_location_t location, addon_descriptor_t** out_descriptor) {
    if (location == ADDON_LOCATION_INTERNAL) {
        if (addon_internal != NULL) {
            *out_descriptor = addon_internal;
            return ESP_OK;
        } else {
        }
        return ESP_ERR_NOT_FOUND;
    } else if (location == ADDON_LOCATION_EXTERNAL) {
        if (addon_catt != NULL) {
            *out_descriptor = addon_catt;
            return ESP_OK;
        } else {
        }
        return ESP_ERR_NOT_FOUND;
    }
    return ESP_ERR_NOT_SUPPORTED;  // Location not available
}