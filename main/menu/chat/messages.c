#include "messages.h"
#include "bsp/input.h"
#include "chat.h"
#include "chatdb_contacts.h"
#include "chatdb_messages.h"
#include "common/display.h"
#include "common/theme.h"
#include "freertos/idf_additions.h"
#include "gui_chat.h"
#include "gui_menu.h"
#include "gui_style.h"
#include "icons.h"
#include "menu/message_dialog.h"
#include "pax_gfx.h"
#include "pax_matrix.h"
#include "pax_types.h"
#include "rajdhani.h"

#if defined(CONFIG_BSP_TARGET_TANMATSU) || defined(CONFIG_BSP_TARGET_KONSOOL)
#define FOOTER_LEFT  ((gui_element_icontext_t[]){{get_icon(ICON_ESC), "/"}, {get_icon(ICON_F1), "Back"}}), 2
#define FOOTER_RIGHT ((gui_element_icontext_t[]){{NULL, "↑ / ↓ | ⏎ Select"}}), 1
#elif defined(CONFIG_BSP_TARGET_MCH2022) || defined(CONFIG_BSP_TARGET_KAMI)
#define FOOTER_LEFT  NULL, 0
#define FOOTER_RIGHT ((gui_element_icontext_t[]){{NULL, "↑ / ↓ | 🅱 Back 🅰 Select"}}), 1
#else
#define FOOTER_LEFT  NULL, 0
#define FOOTER_RIGHT NULL, 0
#endif

/*typedef struct {
    char*               title;
    pax_buf_t*          icon;
    chat_network_type_t network;
    chat_message_t      messages[64];
    size_t              message_count;
} chat_data_t;*/

pax_buf_t icon_alice;
pax_buf_t icon_bob;
pax_buf_t icon_user;
/*
static chat_contact_t chat_contacts[16] = {{
                                               .name     = "Alice",
                                               .network  = CHAT_CONTACT_TYPE_MESHCORE,
                                               .avatar   = &icon_alice,
                                               .location = {.latitude = 0, .longitude = 0},
                                           },
                                           {
                                               .name     = "Bob",
                                               .network  = CHAT_CONTACT_TYPE_MESHCORE,
                                               .avatar   = &icon_bob,
                                               .location = {.latitude = 0, .longitude = 0},
                                           }};

static chat_data_t chat_data = {0};*/

static void render(menu_t* menu, pax_vec2_t position, bool partial, bool icons) {
    pax_buf_t*   buffer = display_get_buffer();
    gui_theme_t* theme  = get_theme();

    if (!partial || icons) {
        render_base_screen_statusbar(buffer, theme, !partial, !partial || icons, !partial,
                                     ((gui_element_icontext_t[]){{get_icon(ICON_SETTINGS), "Chat messages test"}}), 1,
                                     FOOTER_LEFT, FOOTER_RIGHT);
    }
    chat_render(buffer, menu, position, theme, partial);
    display_blit_buffer(buffer);
}

void menu_messages(void) {
    QueueHandle_t input_event_queue = NULL;
    ESP_ERROR_CHECK(bsp_input_get_queue(&input_event_queue));

    pax_buf_t*   buffer = display_get_buffer();
    gui_theme_t* theme  = get_theme();

    pax_buf_init(&icon_alice, NULL, 32, 32, PAX_BUF_32_8888ARGB);
    pax_background(&icon_alice, 0x00000000);
    pax_draw_circle(&icon_alice, theme->chat.palette.color_highlight_tertiary, 16, 16, 16);
    pax_center_text(&icon_alice, 0xFFFFFFFF, &rajdhani, 16, 16, (32 - 16) / 2, "AL");

    pax_buf_init(&icon_user, NULL, 32, 32, PAX_BUF_32_8888ARGB);
    pax_background(&icon_user, 0x00000000);
    pax_draw_circle(&icon_user, theme->chat.palette.color_highlight_primary, 16, 16, 16);
    pax_center_text(&icon_user, 0xFFFFFFFF, &rajdhani, 16, 16, (32 - 16) / 2, "TM");

    pax_buf_init(&icon_bob, NULL, 32, 32, PAX_BUF_32_8888ARGB);
    pax_background(&icon_bob, 0x00000000);
    pax_draw_circle(&icon_bob, theme->chat.palette.color_highlight_secondary, 16, 16, 16);
    pax_center_text(&icon_bob, 0xFFFFFFFF, &rajdhani, 16, 16, (32 - 16) / 2, "BO");

    menu_t menu = {0};
    menu_initialize(&menu);

    /*chat_data.title         = "Public";
    chat_data.network       = CHAT_CONTACT_TYPE_MESHCORE;
    chat_data.message_count = 6;
    chat_data.messages[0]   = (chat_message_t){.message_id   = 1,
                                               .timestamp    = 1625247600,
                                               .message      = "Hello, how are you?",
                                               .sent_by_user = false,
                                               .sender       = &chat_contacts[0]};
    chat_data.messages[1]   = (chat_message_t){.message_id   = 2,
                                               .timestamp    = 1625247660,
                                               .message      = "I'm good, thanks! How about you?",
                                               .sent_by_user = true,
                                               .sender       = NULL};
    chat_data.messages[2]   = (chat_message_t){.message_id   = 3,
                                               .timestamp    = 1625247720,
                                               .message      = "Doing well, just working on a project.",
                                               .sent_by_user = false,
                                               .sender       = &chat_contacts[0]};
    chat_data.messages[3]   = (chat_message_t){.message_id   = 4,
                                               .timestamp    = 1625247780,
                                               .message      = "That's great to hear! What kind of project?",
                                               .sent_by_user = true,
                                               .sender       = NULL};
    chat_data.messages[4]   = (chat_message_t){.message_id   = 5,
                                               .timestamp    = 1625247840,
                                               .message      = "Just a small app for chatting.",
                                               .sent_by_user = false,
                                               .sender       = &chat_contacts[0]};
    chat_data.messages[5]   = (chat_message_t){.message_id   = 6,
                                               .timestamp    = 1625247840,
                                               .message      = "Hi there! This is Bob.",
                                               .sent_by_user = false,
                                               .sender       = &chat_contacts[1]};*/

    /*for (size_t i = 0; i < chat_data.message_count; i++) {
        gui_chat_message_metadata_t* metadata = malloc(sizeof(gui_chat_message_metadata_t));
        if (!metadata) {
            break;
        }
        metadata->timestamp    = chat_data.messages[i].timestamp;
        metadata->sent_by_user = chat_data.messages[i].sent_by_user;
        metadata->message      = chat_data.messages[i].message;
        char* sender_name =
            chat_data.messages[i].sent_by_user
                ? strdup("You")
                : (chat_data.messages->sender ? strdup(chat_data.messages[i].sender->name) : strdup("Unknown"));
        pax_buf_t* sender_avatar =
            chat_data.messages[i].sent_by_user
                ? &icon_user
                : (chat_data.messages[i].sender != NULL ? chat_data.messages[i].sender->avatar : get_icon(ICON_ERROR));
        menu_insert_item_value_icon(&menu, chat_data.messages[i].message, sender_name, (void*)metadata,
                                    (void*)chat_data.messages[i].message_id, -1, sender_avatar);
    }*/

    size_t message_count = 0;
    chat_message_get_amount("/sd/messages.dat", &message_count);
    for (size_t i = 0; i < message_count; i++) {
        chat_message_t message;
        if (chat_message_load("/sd/messages.dat", i, &message) != 0) {
            continue;
        }
        gui_chat_message_metadata_t* metadata = malloc(sizeof(gui_chat_message_metadata_t));
        if (!metadata) {
            break;
        }
        metadata->timestamp    = message.timestamp;
        metadata->sent_by_user = false;
        metadata->message      = NULL;

        menu_insert_item_value_icon(&menu, message.text, message.name, (void*)metadata, (void*)0, -1, NULL);
    }

    int header_height = theme->header.height + (theme->header.vertical_margin * 2);
    int footer_height = theme->footer.height + (theme->footer.vertical_margin * 2);

    pax_vec2_t position = {
        .x0 = theme->menu.horizontal_margin + theme->menu.horizontal_padding,
        .y0 = header_height + theme->menu.vertical_margin + theme->menu.vertical_padding,
        .x1 = pax_buf_get_width(buffer) - theme->menu.horizontal_margin - theme->menu.horizontal_padding,
        .y1 = pax_buf_get_height(buffer) - footer_height - theme->menu.vertical_margin - theme->menu.vertical_padding,
    };

    render(&menu, position, false, true);
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
                            case BSP_INPUT_NAVIGATION_KEY_HOME:
                                for (size_t i = 0; i < menu_get_length(&menu); i++) {
                                    gui_chat_message_metadata_t* metadata =
                                        (gui_chat_message_metadata_t*)menu_get_callback_args(&menu, i);
                                    free(metadata);
                                }
                                menu_free(&menu);
                                return;
                            case BSP_INPUT_NAVIGATION_KEY_UP:
                                menu_navigate_previous(&menu);
                                render(&menu, position, true, false);
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_DOWN:
                                menu_navigate_next(&menu);
                                render(&menu, position, true, false);
                                break;
                            case BSP_INPUT_NAVIGATION_KEY_RETURN:
                            case BSP_INPUT_NAVIGATION_KEY_GAMEPAD_A:
                            case BSP_INPUT_NAVIGATION_KEY_JOYSTICK_PRESS: {
                                void* arg = menu_get_callback_args(&menu, menu_get_position(&menu));
                                // execute_action(buffer, (menu_home_action_t)arg, theme);
                                render(&menu, position, false, true);
                                break;
                            }
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
            render(&menu, position, true, true);
        }
    }
}
