#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include "addon.h"
#include "appfs.h"
#include "badgelink.h"
#include "bootloader_update.h"
#include "bsp/device.h"
#include "bsp/display.h"
#include "bsp/i2c.h"
#include "bsp/input.h"
#include "bsp/led.h"
#include "bsp/power.h"
#include "bsp/rtc.h"
#include "chakrapetchmedium.h"
#include "common/display.h"
#include "common/theme.h"
#include "coprocessor_management.h"
#include "custom_certificates.h"
#include "device_settings.h"
#include "driver/gpio.h"
#include "eeprom.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_types.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "global_event_handler.h"
#include "gui_element_footer.h"
#include "gui_element_header.h"
#include "gui_menu.h"
#include "gui_style.h"
#include "hal/lcd_types.h"
#include "icons.h"
#include "lora.h"
#include "menu/home.h"
#include "menu/message_dialog.h"
#include "ntp.h"
#include "nvs_flash.h"
#include "pax_fonts.h"
#include "pax_gfx.h"
#include "pax_text.h"
#include "portmacro.h"
#include "python.h"
#include "sdcard.h"
#include "sdkconfig.h"
#include "timezone.h"
#include "usb_device.h"
#include "wifi_connection.h"
#include "wifi_remote.h"

#if defined(CONFIG_IDF_TARGET_ESP32P4)
#include "esp_hosted_custom.h"
#endif

#if defined(CONFIG_BSP_TARGET_TANMATSU) || defined(CONFIG_BSP_TARGET_KONSOOL)
#include "bsp/tanmatsu.h"
#include "tanmatsu_coprocessor.h"
#endif

// Constants
static char const TAG[] = "main";

// Global variables
static QueueHandle_t input_event_queue              = NULL;
static wl_handle_t   wl_handle                      = WL_INVALID_HANDLE;
static bool          wifi_stack_initialized         = false;
static bool          wifi_stack_task_done           = false;
static bool          wifi_firmware_version_match    = false;
static bool          wifi_firmware_version_mismatch = false;
static bool          display_available              = false;

static void fix_rtc_out_of_bounds(void) {
    time_t rtc_time = time(NULL);

    bool adjust = false;

    if (rtc_time < 1735689600) {  // 2025-01-01 00:00:00 UTC
        rtc_time = 1735689600;
        adjust   = true;
    }

    if (rtc_time > 4102441200) {  // 2100-01-01 00:00:00 UTC
        rtc_time = 4102441200;
        adjust   = true;
    }

    if (adjust) {
        struct timeval rtc_timeval = {
            .tv_sec  = rtc_time,
            .tv_usec = 0,
        };

        settimeofday(&rtc_timeval, NULL);
        bsp_rtc_set_time(rtc_time);
    }
}

bool wifi_stack_get_initialized(void) {
#if defined(CONFIG_BSP_TARGET_TANMATSU) || defined(CONFIG_BSP_TARGET_KONSOOL)
    bsp_radio_state_t state;
    bsp_power_get_radio_state(&state);
    return wifi_stack_initialized && state == BSP_POWER_RADIO_STATE_APPLICATION;
#else
    return wifi_stack_initialized;
#endif
}

bool wifi_stack_get_version_mismatch(void) {
    return wifi_firmware_version_mismatch;
}

bool wifi_stack_get_task_done(void) {
    return wifi_stack_task_done;
}

static void radio_firmware_callback(uint32_t version) {
    ESP_LOGI(TAG, "Radio firmware version: 0x%" PRIX32, version);
    if (version == 0x00020703) {
        wifi_firmware_version_match = true;
    } else {
        ESP_LOGE(TAG, "Incompatible radio firmware version detected, expected 0x00020703");
        wifi_firmware_version_mismatch = true;
    }
}

static void radio_callback(uint8_t type, uint8_t* payload, uint16_t payload_length) {
    if (type == 1) {
        lora_transaction_receive(payload, payload_length);
    } else {
        ESP_LOGI(TAG, "Received message from radio: type: %d, payload length: %d", type, payload_length);
        for (int i = 0; i < payload_length; i++) {
            printf("%02X ", payload[i]);
        }
        printf("\r\n");
    }
}

static void wifi_task(void* pvParameters) {
    while (!wifi_stack_get_initialized()) {
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    if (ntp_get_enabled()) {
        if (wifi_connect_try_all() == ESP_OK) {
            esp_err_t res = ntp_start_service("pool.ntp.org");
            if (res == ESP_OK) {
                res = ntp_sync_wait();
                if (res != ESP_OK) {
                    ESP_LOGW(TAG, "NTP time sync failed: %s", esp_err_to_name(res));
                } else {
                    time_t rtc_time = time(NULL);
                    bsp_rtc_set_time(rtc_time);
                    ESP_LOGI(TAG, "NTP time sync succesful, RTC updated");
                }
            } else {
                ESP_LOGE(TAG, "Failed to initialize NTP service: %s", esp_err_to_name(res));
            }
        } else {
            ESP_LOGW(TAG, "Could not connect to network for NTP");
        }
    }

#if 0
    while (1) {
        printf("free:%lu min-free:%lu lfb-dma:%u lfb-def:%u lfb-8bit:%u\n", esp_get_free_heap_size(),
               esp_get_minimum_free_heap_size(), heap_caps_get_largest_free_block(MALLOC_CAP_DMA),
               heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT), heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
#endif

    /*while (1) {
        esp_hosted_send_custom(3, (uint8_t*)"Hello from Tanmatsu!", 20);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }*/

    lora_protocol_status_params_t status;
    esp_err_t                     res = lora_get_status(&status);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read LoRa radio status: %s", esp_err_to_name(res));
        vTaskDelete(NULL);
    }

    ESP_LOGI(TAG, "LoRa radio version string: %s", status.version_string);

    lora_protocol_mode_t mode;
    res = lora_get_mode(&mode);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get LoRa mode: %s", esp_err_to_name(res));
        vTaskDelete(NULL);
    }
    ESP_LOGI(TAG, "LoRa mode: %d", (int)mode);

    lora_protocol_config_params_t config = {
        .frequency                  = 869618000,  // Hz
        .spreading_factor           = 8,          // SF8
        .bandwidth                  = 62,         // 62.5 kHz
        .coding_rate                = 8,          // 4/8
        .sync_word                  = 0x12,       // private
        .preamble_length            = 16,         // symbols
        .power                      = 22,         // +22 dBm
        .ramp_time                  = 40,         // us
        .crc_enabled                = true,       // CRC enabled
        .invert_iq                  = false,      // normal IQ
        .low_data_rate_optimization = false,      // disabled
    };

    if (status.chip_type == LORA_PROTOCOL_CHIP_SX1268) {
        // 433MHz LoRa radio
        ESP_LOGW(TAG, "433MHz LoRa radio detected");
        config.frequency = 433.875;  // MHz
    } else {
        ESP_LOGW(TAG, "868/915MHz LoRa radio detected");
    }

    res = lora_set_config(&config);
    if (res == ESP_OK) {
        ESP_LOGI(TAG, "LoRa configuration set");
    } else {
        ESP_LOGE(TAG, "Failed to set LoRa configuration: %s", esp_err_to_name(res));
    }

    res = lora_set_mode(LORA_PROTOCOL_MODE_RX);
    if (res == ESP_OK) {
        ESP_LOGI(TAG, "LoRa set to RX mode");
    } else {
        ESP_LOGE(TAG, "Failed to set LoRa mode: %s", esp_err_to_name(res));
    }

    vTaskDelay(pdMS_TO_TICKS(1000));

    res = lora_get_mode(&mode);
    if (res == ESP_OK) {
        ESP_LOGI(TAG, "LoRa mode (after setting to RX): %d", (int)mode);
    } else {
        ESP_LOGE(TAG, "Failed to get LoRa mode: %s", esp_err_to_name(res));
    }

    vTaskDelete(NULL);
}

esp_err_t check_i2c_bus(void) {
    i2c_master_bus_handle_t i2c_bus_handle_internal;
#if defined(CONFIG_BSP_TARGET_TANMATSU) || defined(CONFIG_BSP_TARGET_KONSOOL)
    ESP_ERROR_CHECK(bsp_i2c_primary_bus_get_handle(&i2c_bus_handle_internal));
    esp_err_t ret_codec  = i2c_master_probe(i2c_bus_handle_internal, 0x08, 50);
    esp_err_t ret_bmi270 = i2c_master_probe(i2c_bus_handle_internal, 0x68, 50);

    if (ret_codec) {
        ESP_LOGE(TAG, "Audio codec not found on I2C bus");
    }

    if (ret_bmi270) {
        ESP_LOGE(TAG, "BMI270 not found on I2C bus");
    }

    if (ret_codec != ESP_OK && ret_bmi270 != ESP_OK) {
        // Neither the audio codec nor the BMI270 sensor were found on the I2C bus.
        // This probably means something is wrong with the I2C bus, we check if the coprocessor is present
        // to determine if the I2C bus is working at all
        esp_err_t ret_coprocessor = i2c_master_probe(i2c_bus_handle_internal, 0x5F, 50);
        if (ret_coprocessor != ESP_OK) {
            ESP_LOGE(TAG, "Coprocessor not found on I2C bus");
            pax_buf_t* buffer = display_get_buffer();
            pax_background(buffer, 0xFFFF0000);
            pax_draw_text(buffer, 0xFFFFFFFF, pax_font_sky_mono, 16, 0, 18 * 0, "The internal I2C bus is not working!");
            pax_draw_text(buffer, 0xFFFFFFFF, pax_font_sky_mono, 16, 0, 18 * 1,
                          "Please remove add-on board, modifications and other plugged in");
            pax_draw_text(buffer, 0xFFFFFFFF, pax_font_sky_mono, 16, 0, 18 * 2,
                          "devices and wires and power cycle the device.");
            display_blit_buffer(buffer);

            vTaskDelay(pdMS_TO_TICKS(10000));

            startup_dialog("Initializing coprocessor...");
            coprocessor_flash(true);
            return ESP_FAIL;
        } else {
            pax_buf_t* buffer = display_get_buffer();
            pax_background(buffer, 0xFFFF0000);
            pax_draw_text(buffer, 0xFFFFFFFF, pax_font_sky_mono, 16, 0, 18 * 0,
                          "Audio codec and orientation sensor do not respond");
            pax_draw_text(buffer, 0xFFFFFFFF, pax_font_sky_mono, 16, 0, 18 * 1,
                          "This could indicate a hardware issue.");
            pax_draw_text(buffer, 0xFFFFFFFF, pax_font_sky_mono, 16, 0, 18 * 2,
                          "Please power cycle the device, if that does not");
            pax_draw_text(buffer, 0xFFFFFFFF, pax_font_sky_mono, 16, 0, 18 * 3, "help please contact support.");
            display_blit_buffer(buffer);
            vTaskDelay(pdMS_TO_TICKS(3000));
        }
    } else if (ret_codec != ESP_OK) {
        pax_buf_t* buffer = display_get_buffer();
        pax_background(buffer, 0xFFFF0000);
        pax_draw_text(buffer, 0xFFFFFFFF, pax_font_sky_mono, 16, 0, 18 * 0, "Audio codec does not respond");
        pax_draw_text(buffer, 0xFFFFFFFF, pax_font_sky_mono, 16, 0, 18 * 1, "This could indicate a hardware issue.");
        pax_draw_text(buffer, 0xFFFFFFFF, pax_font_sky_mono, 16, 0, 18 * 2,
                      "Please power cycle the device, if that does not");
        pax_draw_text(buffer, 0xFFFFFFFF, pax_font_sky_mono, 16, 0, 18 * 3, "help please contact support.");
        display_blit_buffer(buffer);
        vTaskDelay(pdMS_TO_TICKS(3000));
    } else if (ret_bmi270 != ESP_OK) {
        pax_buf_t* buffer = display_get_buffer();
        pax_background(buffer, 0xFFFF0000);
        pax_draw_text(buffer, 0xFFFFFFFF, pax_font_sky_mono, 16, 0, 18 * 0, "Orientation sensor does not respond");
        pax_draw_text(buffer, 0xFFFFFFFF, pax_font_sky_mono, 16, 0, 18 * 1, "This could indicate a hardware issue.");
        pax_draw_text(buffer, 0xFFFFFFFF, pax_font_sky_mono, 16, 0, 18 * 2,
                      "Please power cycle the device, if that does not");
        pax_draw_text(buffer, 0xFFFFFFFF, pax_font_sky_mono, 16, 0, 18 * 3, "help please contact support.");
        display_blit_buffer(buffer);
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
#endif
    return ESP_OK;
}

void app_main(void) {
    // Initialize the Non Volatile Storage service
    esp_err_t res = nvs_flash_init();
    if (res == ESP_ERR_NVS_NO_FREE_PAGES || res == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        res = nvs_flash_init();
    }
    ESP_ERROR_CHECK(res);

    // Initialize the Board Support Package
    const bsp_configuration_t bsp_configuration = {
        .display =
            {
                .requested_color_format = LCD_COLOR_PIXEL_FORMAT_RGB888,
                .num_fbs                = 1,
            },
    };
    esp_err_t bsp_init_result = bsp_device_initialize(&bsp_configuration);

    if (bsp_init_result == ESP_OK) {
        ESP_ERROR_CHECK(bsp_input_get_queue(&input_event_queue));
        bsp_display_set_backlight_brightness(100);
    }

    display_available = display_init() == ESP_OK;

    if (bsp_device_get_initialized_without_coprocessor()) {
        startup_dialog("Device started without coprocessor!");
        ESP_LOGW(TAG, "Device started without coprocessor!");
        vTaskDelay(pdMS_TO_TICKS(2000));
    } else if (bsp_init_result != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize BSP, bailing out.");
        return;
    }

    // Apply settings
    startup_dialog("Applying settings...");
    device_settings_apply();

    // Configure LEDs
    bsp_led_clear();
    bsp_led_set_mode(true);

    // Initialize filesystems
    startup_dialog("Mounting FAT filesystem...");

    esp_vfs_fat_mount_config_t fat_mount_config = {
        .format_if_mount_failed   = false,
        .max_files                = 10,
        .allocation_unit_size     = CONFIG_WL_SECTOR_SIZE,
        .disk_status_check_enable = false,
        .use_one_fat              = false,
    };

    res = esp_vfs_fat_spiflash_mount_rw_wl("/int", "locfd", &fat_mount_config, &wl_handle);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount FAT filesystem: %s", esp_err_to_name(res));
        if (display_available) {
            startup_dialog("Error: Failed to mount FAT filesystem");
            vTaskDelay(pdMS_TO_TICKS(2000));
        }
    } else {
        startup_dialog("Loading icons...");
        load_icons();
    }

    startup_dialog("Checking I2C bus...");
    if (check_i2c_bus() != ESP_OK) {
        startup_dialog("Error: Internal I2C bus failure");
        return;
    }

    startup_dialog("Initializing coprocessor...");
    coprocessor_flash(false);

    if (bsp_init_result != ESP_OK || bsp_device_get_initialized_without_coprocessor()) {
        startup_dialog("Error: Failed to initialize coprocessor");
        return;
    }

    bool radio_recovery_requested = false;
    res = bsp_input_read_navigation_key(BSP_INPUT_NAVIGATION_KEY_VOLUME_UP, &radio_recovery_requested);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read volume up key state: %s", esp_err_to_name(res));
    }
    if (radio_recovery_requested) {
        bsp_power_set_radio_state(BSP_POWER_RADIO_STATE_BOOTLOADER);
        ESP_LOGW(TAG, "Radio firmware update mode requested, starting radio in bootloader mode");
        printf(
            "Volume up was held down while starting the device. The radio is\nhas been started in bootloader \r\n"
            "mode to allow firmware recovery.\n\nGo to https://recovery.tanmatsu.cloud for instructions.");
        if (display_available) {
            gui_theme_t* theme = get_theme();
            pax_buf_t*   fb    = display_get_buffer();
            pax_background(fb, theme->palette.color_background);
            gui_header_draw(
                fb, theme,
                ((gui_element_icontext_t[]){{get_icon(ICON_SYSTEM_UPDATE), (char*)"Radio firmware update mode"}}), 1,
                NULL, 0);
            gui_footer_draw(fb, theme, NULL, 0, NULL, 0);
            int x = theme->menu.horizontal_margin + theme->menu.horizontal_padding;
            int y = theme->header.height + (theme->header.vertical_margin * 2) + theme->menu.vertical_margin +
                    theme->menu.vertical_padding;
            pax_draw_text(
                fb, theme->palette.color_foreground, theme->menu.text_font, 16, x, y + 18 * 0,
                "Volume up was held down while starting the device. The radio is\nhas been started in bootloader "
                "mode to allow firmware recovery.\n\nGo to https://recovery.tanmatsu.cloud for instructions.");
            display_blit_buffer(fb);
        }
        while (1) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    startup_dialog("Mounting AppFS filesystem...");
    res = appfsInit(APPFS_PART_TYPE, APPFS_PART_SUBTYPE);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize AppFS: %s", esp_err_to_name(res));
        pax_buf_t* buffer = display_get_buffer();
        pax_background(buffer, 0xFFFF0000);
        pax_draw_text(buffer, 0xFFFFFFFF, pax_font_sky_mono, 16, 0, 0, "Failed to initialize app filesystem");
        display_blit_buffer(buffer);
        return;
    }

    startup_dialog("Initializing clock...");
    bsp_rtc_update_time();
    if (timezone_nvs_apply("system", "timezone") != ESP_OK) {
        ESP_LOGE(TAG, "Failed to apply timezone, setting timezone to Etc/UTC");
        const timezone_t* zone = NULL;
        res                    = timezone_get_name("Etc/UTC", &zone);
        if (res != ESP_OK) {
            ESP_LOGE(TAG, "Timezone Etc/UTC not found");  // Should never happen
        } else {
            if (timezone_nvs_set("system", "timezone", zone->name) != ESP_OK) {
                ESP_LOGE(TAG, "Failed to save timezone to NVS");
            }
            if (timezone_nvs_set_tzstring("system", "tz", zone->tz) != ESP_OK) {
                ESP_LOGE(TAG, "Failed to save TZ string to NVS");
            }
        }
        timezone_apply_timezone(zone);
    }
    fix_rtc_out_of_bounds();

    startup_dialog("Initializing event handler...");
    global_event_handler_initialize();

    startup_dialog("Initializing certificate store...");
    ESP_ERROR_CHECK(initialize_custom_ca_store());

    startup_dialog("Checking bootloader...");
    bootloader_update();

    startup_dialog("Initializing radio...");
    ESP_ERROR_CHECK(lora_init(16));

#if defined(CONFIG_IDF_TARGET_ESP32P4)
    esp_hosted_set_custom_callback(radio_callback);
    esp_hosted_set_firmware_version_callback(radio_firmware_callback);
#endif

    if (wifi_remote_initialize() == ESP_OK) {
        res = wifi_connection_init_stack();
        if (res != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize WiFi stack %d (%s)", res, esp_err_to_name(res));
        } else {
            wifi_stack_initialized = true;
        }
    } else {
        ESP_LOGE(TAG, "WiFi radio not responding, did you flash ESP-HOSTED firmware?");
    }
    wifi_stack_task_done = true;

    xTaskCreatePinnedToCore(wifi_task, TAG, 4096, NULL, 10, NULL, CONFIG_SOC_CPU_CORES_NUM - 1);

    startup_dialog("Initializing BadgeLink...");
    badgelink_init();
    usb_initialize();
    badgelink_start(usb_send_data);

    startup_dialog("Detecting Add-On boards...");
    addon_initialize();

    // App start

    bsp_power_set_usb_host_boost_enabled(true);

#if CONFIG_IDF_TARGET_ESP32P4
// Only integrate Python into the launcher on ESP32-P4 targets
#if 0
    python_initialize();
#endif
#endif

    menu_home();
}
