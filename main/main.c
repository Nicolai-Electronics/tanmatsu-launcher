#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include "addon.h"
#include "appfs.h"
#include "badgelink.h"
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
#include "gui_element_footer.h"
#include "gui_element_header.h"
#include "gui_menu.h"
#include "gui_style.h"
#include "hal/lcd_types.h"
#include "icons.h"
#include "menu/home.h"
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

#if defined(CONFIG_BSP_TARGET_TANMATSU) || defined(CONFIG_BSP_TARGET_KONSOOL)
#include "bsp/tanmatsu.h"
#include "tanmatsu_coprocessor.h"
#endif

// Constants
static char const TAG[] = "main";

// Global variables
static QueueHandle_t input_event_queue      = NULL;
static wl_handle_t   wl_handle              = WL_INVALID_HANDLE;
static bool          wifi_stack_initialized = false;
static bool          wifi_stack_task_done   = false;

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

void startup_screen(const char* text) {
    gui_theme_t* theme = get_theme();
    pax_buf_t*   fb    = display_get_buffer();
    pax_background(fb, theme->palette.color_background);
    gui_header_draw(fb, theme, ((gui_element_icontext_t[]){{NULL, (char*)text}}), 1, NULL, 0);
    gui_footer_draw(fb, theme, NULL, 0, NULL, 0);
    display_blit_buffer(fb);
}

bool wifi_stack_get_initialized(void) {
    return wifi_stack_initialized;
}

bool wifi_stack_get_task_done(void) {
    return wifi_stack_task_done;
}

static void wifi_task(void* pvParameters) {
    if (wifi_remote_initialize() == ESP_OK) {
        wifi_connection_init_stack();
        wifi_stack_initialized = true;
        // wifi_connect_try_all();
    } else {
        // bsp_power_set_radio_state(BSP_POWER_RADIO_STATE_OFF);
        ESP_LOGE(TAG, "WiFi radio not responding, did you flash ESP-HOSTED firmware?");
    }
    wifi_stack_task_done = true;

    if (ntp_get_enabled() && wifi_stack_initialized) {
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

    addon_detect_internal();
    addon_detect_catt();

#if 0
    while (1) {
        printf("free:%lu min-free:%lu lfb-dma:%u lfb-def:%u lfb-8bit:%u\n", esp_get_free_heap_size(),
               esp_get_minimum_free_heap_size(), heap_caps_get_largest_free_block(MALLOC_CAP_DMA),
               heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT), heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
#endif

    vTaskDelete(NULL);
}

esp_err_t check_i2c_bus(void) {
    i2c_master_bus_handle_t i2c_bus_handle_internal;
    ESP_ERROR_CHECK(bsp_i2c_primary_bus_get_handle(&i2c_bus_handle_internal));
#if defined(CONFIG_BSP_TARGET_TANMATSU) || defined(CONFIG_BSP_TARGET_KONSOOL)
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

            startup_screen("Initializing coprocessor...");
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
        display_init();
    } else if (bsp_device_get_initialized_without_coprocessor()) {
        display_init();
        pax_buf_t* buffer = display_get_buffer();
        pax_background(buffer, 0xFFFFFF00);
        pax_draw_text(buffer, 0xFF000000, pax_font_sky_mono, 16, 0, 0, "Device started without coprocessor!");
        display_blit_buffer(buffer);
        vTaskDelay(pdMS_TO_TICKS(2000));
    } else {
        ESP_LOGE(TAG, "Failed to initialize BSP, bailing out.");
        return;
    }

    // Apply settings
    startup_screen("Applying settings...");
    device_settings_apply();

    // Configure LEDs
    bsp_led_clear();
    bsp_led_set_mode(true);

    // Initialize filesystems
    startup_screen("Mounting FAT filesystem...");

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
        startup_screen("Error: Failed to mount FAT filesystem");
        pax_buf_t* buffer = display_get_buffer();
        pax_background(buffer, 0xFFFF0000);
        pax_draw_text(buffer, 0xFFFFFFFF, pax_font_sky_mono, 16, 0, 0, "Failed to initialize FAT filesystem");
        display_blit_buffer(buffer);
        return;
    }

    startup_screen("Checking I2C bus...");
    if (check_i2c_bus() != ESP_OK) {
        return;
    }

    startup_screen("Initializing coprocessor...");
    coprocessor_flash(false);

    if (bsp_init_result != ESP_OK || bsp_device_get_initialized_without_coprocessor()) {
        pax_buf_t* buffer = display_get_buffer();
        pax_background(buffer, 0xFFFF0000);
        pax_draw_text(buffer, 0xFFFFFFFF, pax_font_sky_mono, 16, 0, 0, "Failed to initialize coprocessor");
        display_blit_buffer(buffer);
        return;
    }

    startup_screen("Mounting AppFS filesystem...");
    res = appfsInit(APPFS_PART_TYPE, APPFS_PART_SUBTYPE);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize AppFS: %s", esp_err_to_name(res));
        pax_buf_t* buffer = display_get_buffer();
        pax_background(buffer, 0xFFFF0000);
        pax_draw_text(buffer, 0xFFFFFFFF, pax_font_sky_mono, 16, 0, 0, "Failed to initialize app filesystem");
        display_blit_buffer(buffer);
        return;
    }

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

    bool sdcard_inserted = false;
    bsp_input_read_action(BSP_INPUT_ACTION_TYPE_SD_CARD, &sdcard_inserted);

    if (sdcard_inserted) {
        printf("SD card detected\r\n");
#if defined(CONFIG_BSP_TARGET_TANMATSU) || defined(CONFIG_BSP_TARGET_KONSOOL)
        sd_pwr_ctrl_handle_t sd_pwr_handle = initialize_sd_ldo();
        sd_mount(sd_pwr_handle);
        // sd_speedtest();  // Uncomment to run SD card benchmark
        test_sd();
#endif
    }

    ESP_ERROR_CHECK(initialize_custom_ca_store());

#if CONFIG_IDF_TARGET_ESP32P4
// Only integrate Python into the launcher on ESP32-P4 targets
#if 0
    python_initialize();
#endif
#endif

    xTaskCreatePinnedToCore(wifi_task, TAG, 4096, NULL, 10, NULL, CONFIG_SOC_CPU_CORES_NUM - 1);

    badgelink_init();
    usb_initialize();
    badgelink_start(usb_send_data);

    load_icons();

    bsp_power_set_usb_host_boost_enabled(true);

    menu_home();
}
