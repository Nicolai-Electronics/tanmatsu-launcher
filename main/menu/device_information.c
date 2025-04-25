#include "device_information.h"
#include <string.h>
#include "bsp/device.h"
#include "bsp/input.h"
#include "common/display.h"
#include "esp_efuse.h"
#include "esp_efuse_chip.h"
#include "esp_efuse_custom_table.h"
#include "esp_efuse_table.h"
#include "esp_ota_ops.h"
#include "freertos/idf_additions.h"
#include "gui_footer.h"
#include "gui_style.h"
#include "icons.h"
#include "menu/message_dialog.h"
#include "pax_gfx.h"
#include "pax_matrix.h"
#include "pax_text.h"
#include "pax_types.h"

// #include "shapes/pax_misc.h"

typedef enum {
    DEVICE_VARIANT_RADIO_NONE                = 0,
    DEVICE_VARIANT_RADIO_AI_THINKER_RA01S    = 1,   // SX1268 410-525MHz
    DEVICE_VARIANT_RADIO_AI_THINKER_RA01SH   = 2,   // SX1262 803-930MHz
    DEVICE_VARIANT_RADIO_EBYTE_E22_400M_22S  = 3,   // SX1268 410-493MHz TCXO
    DEVICE_VARIANT_RADIO_EBYTE_E22_900M_22S  = 4,   // SX1262 850-930MHz TCXO
    DEVICE_VARIANT_RADIO_EBYTE_E22_400M_30S  = 5,   // SX1268 410-493MHz TCXO PA
    DEVICE_VARIANT_RADIO_EBYTE_E22_900M_30S  = 6,   // SX1262 850-930MHz TCXO PA
    DEVICE_VARIANT_RADIO_EBYTE_E22_400M_33S  = 7,   // SX1268 410-493MHz TCXO PA
    DEVICE_VARIANT_RADIO_EBYTE_E22_900M_33S  = 8,   // SX1262 850-930MHz TCXO PA
    DEVICE_VARIANT_RADIO_EBYTE_E220_400M_22S = 9,   // LLCC68 410-493 MHz
    DEVICE_VARIANT_RADIO_EBYTE_E220_900M_22S = 10,  // LLCC68 850-930 MHz
    DEVICE_VARIANT_RADIO_EBYTE_E220_400M_33S = 11,  // LLCC68 410-493 MHz PA
    DEVICE_VARIANT_RADIO_EBYTE_E220_900M_33S = 12,  // LLCC68 850-930 MHz PA
} device_variant_radio_t;

typedef enum {
    DEVICE_VARIANT_COLOR_NONE      = 0,
    DEVICE_VARIANT_COLOR_BLACK     = 1,
    DEVICE_VARIANT_COLOR_CYBERDECK = 2,
    DEVICE_VARIANT_COLOR_BLUE      = 3,
    DEVICE_VARIANT_COLOR_RED       = 4,
    DEVICE_VARIANT_COLOR_GREEN     = 5,
    DEVICE_VARIANT_COLOR_PURPLE    = 6,
    DEVICE_VARIANT_COLOR_YELLOW    = 7,
    DEVICE_VARIANT_COLOR_WHITE     = 8,
} device_variant_color_t;

typedef struct {
    // User data
    char                   name[16 + sizeof('\0')];
    char                   vendor[10 + sizeof('\0')];
    uint8_t                revision;
    device_variant_radio_t radio;
    device_variant_color_t color;
    char                   region[2 + sizeof('\0')];
    // MAC address
    uint8_t                mac[6];
    // Unique identifier
    uint8_t                uid[16];
    // Waver revision
    uint8_t                waver_rev_major;
    uint8_t                waver_rev_minor;
} device_identity_t;

esp_err_t read_device_identity(device_identity_t* out_identity) {
    esp_err_t res;

    memset(out_identity, 0, sizeof(device_identity_t));

    res = esp_efuse_read_field_blob(ESP_EFUSE_USER_DATA_HARDWARE_NAME, out_identity->name, 16 * 8);
    if (res != ESP_OK) {
        return res;
    }

    res = esp_efuse_read_field_blob(ESP_EFUSE_USER_DATA_HARDWARE_VENDOR, out_identity->vendor, 10 * 8);
    if (res != ESP_OK) {
        return res;
    }

    res =
        esp_efuse_read_field_blob(ESP_EFUSE_USER_DATA_HARDWARE_REVISION, &out_identity->revision, sizeof(uint8_t) * 8);
    if (res != ESP_OK) {
        return res;
    }

    uint8_t variant_radio = 0;
    res = esp_efuse_read_field_blob(ESP_EFUSE_USER_DATA_HARDWARE_VARIANT_RADIO, &variant_radio, sizeof(uint8_t) * 8);
    if (res != ESP_OK) {
        return res;
    }
    out_identity->radio = (device_variant_radio_t)variant_radio;

    uint8_t variant_color = 0;
    res = esp_efuse_read_field_blob(ESP_EFUSE_USER_DATA_HARDWARE_VARIANT_COLOR, &variant_color, sizeof(uint8_t) * 8);
    if (res != ESP_OK) {
        return res;
    }
    out_identity->color = (device_variant_radio_t)variant_color;

    res = esp_efuse_read_field_blob(ESP_EFUSE_USER_DATA_HARDWARE_REGION, out_identity->region, 2 * 8);
    if (res != ESP_OK) {
        return res;
    }

    res = esp_efuse_read_field_blob(ESP_EFUSE_MAC, &out_identity->mac, 6 * 8);
    if (res != ESP_OK) {
        return res;
    }
#ifdef CONFIG_IDF_TARGET_ESP32P4
    res = esp_efuse_read_field_blob(ESP_EFUSE_OPTIONAL_UNIQUE_ID, &out_identity->uid, 16 * 8);
    if (res != ESP_OK) {
        return res;
    }

    res = esp_efuse_read_field_blob(ESP_EFUSE_WAFER_VERSION_MAJOR, &out_identity->waver_rev_major, 2);
    if (res != ESP_OK) {
        return res;
    }

    res = esp_efuse_read_field_blob(ESP_EFUSE_WAFER_VERSION_MINOR, &out_identity->waver_rev_minor, 4);
    if (res != ESP_OK) {
        return res;
    }
#endif

    return res;
}

const char* get_radio_name(uint8_t radio_id) {
    switch (radio_id) {
        case DEVICE_VARIANT_RADIO_NONE:
            return "None";
        case DEVICE_VARIANT_RADIO_AI_THINKER_RA01S:
            return "Ai thinker RA-01S";
        case DEVICE_VARIANT_RADIO_AI_THINKER_RA01SH:
            return "Ai thinker RA-01SH";
        case DEVICE_VARIANT_RADIO_EBYTE_E22_400M_22S:
            return "EBYTE E22-400M22S";
        case DEVICE_VARIANT_RADIO_EBYTE_E22_900M_22S:
            return "EBYTE E22-900M22S";
        case DEVICE_VARIANT_RADIO_EBYTE_E22_400M_30S:
            return "EBYTE E22-400M30S";
        case DEVICE_VARIANT_RADIO_EBYTE_E22_900M_30S:
            return "EBYTE E22-900M30S";
        case DEVICE_VARIANT_RADIO_EBYTE_E22_400M_33S:
            return "EBYTE E22-400M33S";
        case DEVICE_VARIANT_RADIO_EBYTE_E22_900M_33S:
            return "EBYTE E22-900M33S";
        case DEVICE_VARIANT_RADIO_EBYTE_E220_400M_22S:
            return "EBYTE E220-400M22S";
        case DEVICE_VARIANT_RADIO_EBYTE_E220_900M_22S:
            return "EBYTE E220-900M22S";
        case DEVICE_VARIANT_RADIO_EBYTE_E220_400M_33S:
            return "EBYTE E220-400M33S";
        case DEVICE_VARIANT_RADIO_EBYTE_E220_900M_33S:
            return "EBYTE E220-900M33S";
        default:
            return "Unknown";
    }
}

const char* get_color_name(uint8_t color_id) {
    switch (color_id) {
        case DEVICE_VARIANT_COLOR_BLACK:
            return "Black";
        case DEVICE_VARIANT_COLOR_CYBERDECK:
            return "Cyberdeck";
        case DEVICE_VARIANT_COLOR_BLUE:
            return "Blue";
        case DEVICE_VARIANT_COLOR_RED:
            return "Red";
        case DEVICE_VARIANT_COLOR_GREEN:
            return "Green";
        case DEVICE_VARIANT_COLOR_PURPLE:
            return "Purple";
        case DEVICE_VARIANT_COLOR_YELLOW:
            return "Yellow";
        case DEVICE_VARIANT_COLOR_WHITE:
            return "White";
        default:
            return "Unknown";
    }
}

#if defined(CONFIG_BSP_TARGET_TANMATSU) || defined(CONFIG_BSP_TARGET_KONSOOL) || \
    defined(CONFIG_BSP_TARGET_HACKERHOTEL_2026)
#define FOOTER_LEFT  ((gui_header_field_t[]){{get_icon(ICON_ESC), "/"}, {get_icon(ICON_F1), "Back"}}), 2
#define FOOTER_RIGHT NULL, 0
#define TEXT_FONT    pax_font_sky_mono
#define TEXT_SIZE    18
#elif defined(CONFIG_BSP_TARGET_MCH2022)
#define FOOTER_LEFT  ((gui_header_field_t[]){{NULL, "🅱 Back"}}), 1
#define FOOTER_RIGHT NULL, 0
#define TEXT_FONT    pax_font_sky_mono
#define TEXT_SIZE    9
#else
#define FOOTER_LEFT  NULL, 0
#define FOOTER_RIGHT NULL, 0
#define TEXT_FONT    pax_font_sky_mono
#define TEXT_SIZE    9
#endif

static void render(pax_buf_t* buffer, gui_theme_t* theme, pax_vec2_t position, bool partial, bool icons) {
    if (!partial || icons) {
        render_base_screen_statusbar(buffer, theme, !partial, !partial || icons, !partial,
                                     ((gui_header_field_t[]){{get_icon(ICON_DEVICE_INFO), "Device information"}}), 1,
                                     FOOTER_LEFT, FOOTER_RIGHT);
    }
    char text_buffer[256];
    int  line = 0;
    if (!partial) {

        const esp_app_desc_t* app_description = esp_app_get_description();
        pax_draw_text(buffer, theme->palette.color_foreground, TEXT_FONT, TEXT_SIZE, position.x0,
                      position.y0 + (TEXT_SIZE + 2) * (line++), "Firmware identity:");
        snprintf(text_buffer, sizeof(text_buffer), "Project name:        %s", app_description->project_name);
        pax_draw_text(buffer, theme->palette.color_foreground, TEXT_FONT, TEXT_SIZE, position.x0,
                      position.y0 + (TEXT_SIZE + 2) * (line++), text_buffer);
        snprintf(text_buffer, sizeof(text_buffer), "Project version:     %s", app_description->version);
        pax_draw_text(buffer, theme->palette.color_foreground, TEXT_FONT, TEXT_SIZE, position.x0,
                      position.y0 + (TEXT_SIZE + 2) * (line++), text_buffer);
        char bsp_buffer[64];
        bsp_device_get_name(bsp_buffer, sizeof(bsp_buffer));
        snprintf(text_buffer, sizeof(text_buffer), "BSP device name:     %s", bsp_buffer);
        pax_draw_text(buffer, theme->palette.color_foreground, TEXT_FONT, TEXT_SIZE, position.x0,
                      position.y0 + (TEXT_SIZE + 2) * (line++), text_buffer);
        bsp_device_get_manufacturer(bsp_buffer, sizeof(bsp_buffer));
        snprintf(text_buffer, sizeof(text_buffer), "BSP device vendor:   %s", bsp_buffer);
        pax_draw_text(buffer, theme->palette.color_foreground, TEXT_FONT, TEXT_SIZE, position.x0,
                      position.y0 + (TEXT_SIZE + 2) * (line++), text_buffer);
        line++;
        device_identity_t identity = {0};
        if (read_device_identity(&identity) != ESP_OK) {
            snprintf(identity.name, sizeof(identity.name), "Unknown");
            snprintf(identity.vendor, sizeof(identity.vendor), "Unknown");
            identity.revision = 0;
            identity.radio    = DEVICE_VARIANT_RADIO_NONE;
            snprintf(identity.region, sizeof(identity.region), "??");
            memset(identity.mac, 0, sizeof(identity.mac));
            memset(identity.uid, 0, sizeof(identity.uid));
        }
        pax_draw_text(buffer, theme->palette.color_foreground, TEXT_FONT, TEXT_SIZE, position.x0,
                      position.y0 + (TEXT_SIZE + 2) * (line++), "Hardware identity:");
        snprintf(text_buffer, sizeof(text_buffer), "Name:                %s", identity.name);
        pax_draw_text(buffer, theme->palette.color_foreground, TEXT_FONT, TEXT_SIZE, position.x0,
                      position.y0 + (TEXT_SIZE + 2) * (line++), text_buffer);
        snprintf(text_buffer, sizeof(text_buffer), "Vendor:              %s",
                 strcmp(identity.vendor, "Nicolai") != 0 ? identity.vendor : "Nicolai Electronics");
        pax_draw_text(buffer, theme->palette.color_foreground, TEXT_FONT, TEXT_SIZE, position.x0,
                      position.y0 + (TEXT_SIZE + 2) * (line++), text_buffer);
        snprintf(text_buffer, sizeof(text_buffer), "Board revision:      %" PRIu8, identity.revision);
        pax_draw_text(buffer, theme->palette.color_foreground, TEXT_FONT, TEXT_SIZE, position.x0,
                      position.y0 + (TEXT_SIZE + 2) * (line++), text_buffer);
        snprintf(text_buffer, sizeof(text_buffer), "Radio:               %s", get_radio_name(identity.radio));
        pax_draw_text(buffer, theme->palette.color_foreground, TEXT_FONT, TEXT_SIZE, position.x0,
                      position.y0 + (TEXT_SIZE + 2) * (line++), text_buffer);
        snprintf(text_buffer, sizeof(text_buffer), "Color:               %s", get_color_name(identity.color));
        pax_draw_text(buffer, theme->palette.color_foreground, TEXT_FONT, TEXT_SIZE, position.x0,
                      position.y0 + (TEXT_SIZE + 2) * (line++), text_buffer);
        snprintf(text_buffer, sizeof(text_buffer), "Region:              %s", identity.region);
        pax_draw_text(buffer, theme->palette.color_foreground, TEXT_FONT, TEXT_SIZE, position.x0,
                      position.y0 + (TEXT_SIZE + 2) * (line++), text_buffer);
        snprintf(text_buffer, sizeof(text_buffer), "APP SOC UID:         ");
        for (size_t i = 0; i < 16; i++) {
            snprintf(text_buffer + strlen(text_buffer), sizeof(text_buffer) - strlen(text_buffer), "%02x",
                     identity.uid[i]);
        }
        pax_draw_text(buffer, theme->palette.color_foreground, TEXT_FONT, TEXT_SIZE, position.x0,
                      position.y0 + (TEXT_SIZE + 2) * (line++), text_buffer);
        snprintf(text_buffer, sizeof(text_buffer), "APP SOC MAC:         ");
        for (size_t i = 0; i < 6; i++) {
            snprintf(text_buffer + strlen(text_buffer), sizeof(text_buffer) - strlen(text_buffer), "%02x%s",
                     identity.mac[i], i < 5 ? ":" : "");
        }
        pax_draw_text(buffer, theme->palette.color_foreground, TEXT_FONT, TEXT_SIZE, position.x0,
                      position.y0 + (TEXT_SIZE + 2) * (line++), text_buffer);
        snprintf(text_buffer, sizeof(text_buffer), "APP SOC waver rev:   %u.%u", identity.waver_rev_major,
                 identity.waver_rev_minor);
        pax_draw_text(buffer, theme->palette.color_foreground, TEXT_FONT, TEXT_SIZE, position.x0,
                      position.y0 + (TEXT_SIZE + 2) * (line++), text_buffer);
        line++;
    }
    display_blit_buffer(buffer);
}

void menu_device_information(pax_buf_t* buffer, gui_theme_t* theme) {
    QueueHandle_t input_event_queue = NULL;
    ESP_ERROR_CHECK(bsp_input_get_queue(&input_event_queue));

    int header_height = theme->header.height + (theme->header.vertical_margin * 2);
    int footer_height = theme->footer.height + (theme->footer.vertical_margin * 2);

    pax_vec2_t position = {
        .x0 = theme->menu.horizontal_margin + theme->menu.horizontal_padding,
        .y0 = header_height + theme->menu.vertical_margin + theme->menu.vertical_padding,
        .x1 = pax_buf_get_width(buffer) - theme->menu.horizontal_margin - theme->menu.horizontal_padding,
        .y1 = pax_buf_get_height(buffer) - footer_height - theme->menu.vertical_margin - theme->menu.vertical_padding,
    };

    render(buffer, theme, position, false, true);
    while (1) {
        bsp_input_event_t event;
        if (xQueueReceive(input_event_queue, &event, pdMS_TO_TICKS(1000)) == pdTRUE) {
            switch (event.type) {
                case INPUT_EVENT_TYPE_NAVIGATION: {
                    if (event.args_navigation.state) {
                        switch (event.args_navigation.key) {
                            case BSP_INPUT_NAVIGATION_KEY_ESC:
                            case BSP_INPUT_NAVIGATION_KEY_F1:
                            case BSP_INPUT_NAVIGATION_KEY_GAMEPAD_B:
                                return;
                            default:
                                break;
                        }
                    }
                    break;
                }
                default:
                    break;
            }
        } else {
            render(buffer, theme, position, true, true);
        }
    }
}
