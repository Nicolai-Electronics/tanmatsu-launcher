#include <stdio.h>
#include "appfs.h"
#include "badgelink/badgelink.h"
#include "bsp/device.h"
#include "bsp/display.h"
#include "bsp/input.h"
#include "bsp/power.h"
#include "bsp/rtc.h"
#include "chakrapetchmedium.h"
#include "common/display.h"
#include "coprocessor_management.h"
#include "driver/gpio.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_types.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "freertos/idf_additions.h"
#include "gui_footer.h"
#include "gui_menu.h"
#include "gui_style.h"
#include "hal/lcd_types.h"
#include "icons.h"
#include "menu/home.h"
#include "nvs_flash.h"
#include "pax_fonts.h"
#include "pax_gfx.h"
#include "pax_text.h"
#include "portmacro.h"
#include "sdcard.h"
#include "timezone.h"
#include "usb_device.h"
#include "wifi_connection.h"
#include "wifi_remote.h"

#ifdef CONFIG_BSP_TARGET_TANMATSU
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

#ifdef CONFIG_BSP_TARGET_TANMATSU
gui_theme_t theme = {
    .palette =
        {
            .color_foreground          = 0xFF340132,  // #340132
            .color_background          = 0xFFEEEAEE,  // #EEEAEE
            .color_active_foreground   = 0xFF340132,  // #340132
            .color_active_background   = 0xFFFFFFFF,  // #FFFFFF
            .color_highlight_primary   = 0xFF01BC99,  // #01BC99
            .color_highlight_secondary = 0xFFFFCF53,  // #FFCF53
            .color_highlight_tertiary  = 0xFFFF017F,  // #FF017F
        },
    .footer =
        {
            .height             = 32,
            .vertical_margin    = 7,
            .horizontal_margin  = 20,
            .text_height        = 16,
            .vertical_padding   = 20,
            .horizontal_padding = 0,
            .text_font          = &chakrapetchmedium,
        },
    .header =
        {
            .height             = 32,
            .vertical_margin    = 7,
            .horizontal_margin  = 20,
            .text_height        = 16,
            .vertical_padding   = 20,
            .horizontal_padding = 0,
            .text_font          = &chakrapetchmedium,
        },
    .menu =
        {
            .height                = 480 - 64,
            .vertical_margin       = 20,
            .horizontal_margin     = 30,
            .text_height           = 16,
            .vertical_padding      = 6,
            .horizontal_padding    = 6,
            .text_font             = &chakrapetchmedium,
            .list_entry_height     = 32,
            .grid_horizontal_count = 4,
            .grid_vertical_count   = 3,
        },
};
#else
gui_theme_t theme = {
    .palette =
        {
            .color_foreground          = 0xFF340132,  // #340132
            .color_background          = 0xFFEEEAEE,  // #EEEAEE
            .color_active_foreground   = 0xFF340132,  // #340132
            .color_active_background   = 0xFFFFFFFF,  // #FFFFFF
            .color_highlight_primary   = 0xFF01BC99,  // #01BC99
            .color_highlight_secondary = 0xFFFFCF53,  // #FFCF53
            .color_highlight_tertiary  = 0xFFFF017F,  // #FF017F
        },
    .footer =
        {
            .height             = 16,
            .vertical_margin    = 0,
            .horizontal_margin  = 0,
            .text_height        = 16,
            .vertical_padding   = 20,
            .horizontal_padding = 0,
            .text_font          = &chakrapetchmedium,
        },
    .header =
        {
            .height             = 32,
            .vertical_margin    = 7,
            .horizontal_margin  = 20,
            .text_height        = 16,
            .vertical_padding   = 20,
            .horizontal_padding = 0,
            .text_font          = &chakrapetchmedium,
        },
    .menu =
        {
            .height                = 480 - 64,
            .vertical_margin       = 20,
            .horizontal_margin     = 30,
            .text_height           = 16,
            .vertical_padding      = 6,
            .horizontal_padding    = 6,
            .text_font             = &chakrapetchmedium,
            .list_entry_height     = 32,
            .grid_horizontal_count = 4,
            .grid_vertical_count   = 3,
        },
};
#endif

void startup_screen(const char* text) {
    pax_buf_t* fb = display_get_buffer();
    pax_background(fb, theme.palette.color_background);
    gui_render_header_adv(fb, &theme, ((gui_header_field_t[]){{NULL, (char*)text}}), 1, NULL, 0);
    gui_render_footer_adv(fb, &theme, NULL, 0, NULL, 0);
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
        wifi_connect_try_all();
    } else {
        bsp_power_set_radio_state(BSP_POWER_RADIO_STATE_OFF);
        ESP_LOGE(TAG, "WiFi radio not responding, did you flash ESP-HOSTED firmware?");
    }
    wifi_stack_task_done = true;
    vTaskDelete(NULL);
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
    ESP_ERROR_CHECK(bsp_device_initialize());

    ESP_ERROR_CHECK(bsp_input_get_queue(&input_event_queue));

    bsp_display_set_backlight_brightness(100);

    display_init();

    startup_screen("Mounting FAT filesystem...");

    esp_vfs_fat_mount_config_t fat_mount_config = {
        .format_if_mount_failed   = false,
        .max_files                = 10,
        .allocation_unit_size     = CONFIG_WL_SECTOR_SIZE,
        .disk_status_check_enable = false,
        .use_one_fat              = false,
    };

    res = esp_vfs_fat_spiflash_mount_rw_wl("/int", "fat", &fat_mount_config, &wl_handle);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount FAT filesystem: %s", esp_err_to_name(res));
        startup_screen("Error: Failed to mount FAT filesystem");
        return;
    }

    startup_screen("Initializing coprocessor...");
    coprocessor_flash();

    startup_screen("Mounting AppFS filesystem...");
    res = appfsInit(APPFS_PART_TYPE, APPFS_PART_SUBTYPE);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize AppFS: %s", esp_err_to_name(res));
        return;
    }

    bsp_rtc_update_time();
    if (timezone_nvs_apply("system", "timezone") != ESP_OK) {
        ESP_LOGE(TAG, "Failed to apply timezone, setting timezone to Etc/UTC");
        const timezone_t* zone = NULL;
        res                    = timezone_get_name("Etc/UTC", &zone);
        if (res != ESP_OK) {
            ESP_LOGE(TAG, "Timezone Etc/UTC not found");
            return;
        }
        if (timezone_nvs_set("system", "timezone", zone->name) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to save timezone to NVS");
        }
        if (timezone_nvs_set_tzstring("system", "tz", zone->tz) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to save TZ string to NVS");
        }
        timezone_apply_timezone(zone);
    }

#ifdef CONFIG_BSP_TARGET_TANMATSU
    tanmatsu_coprocessor_handle_t handle = NULL;
    if (bsp_tanmatsu_coprocessor_get_handle(&handle) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get coprocessor handle");
        return;
    }
    tanmatsu_coprocessor_inputs_t inputs = {0};
    if (tanmatsu_coprocessor_get_inputs(handle, &inputs) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get coprocessor inputs");
        return;
    }

    if (inputs.sd_card_detect) {
        printf("SD card detected, mounting\r\n");
        sd_pwr_ctrl_handle_t sd_pwr_handle = initialize_sd_ldo();
        sd_mount_spi(sd_pwr_handle);
        test_sd();
    }
#endif

    xTaskCreate(wifi_task, TAG, 4096, NULL, 10, NULL);

    badgelink_init();
    usb_initialize();
    badgelink_start();

    load_icons();

    pax_buf_t* buffer = display_get_buffer();
    menu_home(buffer, &theme);
}
