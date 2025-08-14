#include "file_explorer.h"
#include <string.h>
#include "bsp/input.h"
#include "common/display.h"
#include "esp_vfs.h"
#include "gui_style.h"
#include "gui_menu.h"
#include "icons.h"
#include "menu/message_dialog.h"
#include "pax_gfx.h"
#include "pax_types.h"


static void populate_menu(menu_t* menu, char* path){
  DIR* dir = opendir(path);
  if(dir==NULL)return;
  struct dirent* entry;
  while ((entry = readdir(dir)) != NULL) {
    menu_insert_item(menu,entry->d_name, NULL, (void*)entry->d_type, -1);
  }
  closedir(dir);
}

#if defined(CONFIG_BSP_TARGET_TANMATSU) || defined(CONFIG_BSP_TARGET_KONSOOL) || \
    defined(CONFIG_BSP_TARGET_HACKERHOTEL_2026)
#define FOOTER_LEFT                                              \
    ((gui_element_icontext_t[]){{get_icon(ICON_ESC), "/"},       \
                                {get_icon(ICON_F1), "Quit"},     \
                                {get_icon(ICON_F2), "Back"}}),  \
        3
#define FOOTER_RIGHT ((gui_element_icontext_t[]){{NULL, "↑ / ↓ Select | ⏎ Open"}}), 1
#elif defined(CONFIG_BSP_TARGET_MCH2022)
#define FOOTER_LEFT  NULL, 0
#define FOOTER_RIGHT ((gui_element_icontext_t[]){{NULL, "🅰 Open"}, {NULL, "🅱"Back"}}), 2
#else
#define FOOTER_LEFT  NULL, 0
#define FOOTER_RIGHT NULL, 0
#endif

static void render(pax_buf_t* buffer, gui_theme_t* theme, menu_t*menu, char*path, pax_vec2_t position, bool partial, bool icons){
  if(!partial || icons) {
    render_base_screen_statusbar(buffer, theme, !partial, !partial||icons, !partial,
	((gui_element_icontext_t[]){{get_icon(ICON_SD), path}}), 1,
	FOOTER_LEFT,FOOTER_RIGHT);
  }
  if(menu_get_length(menu)!=0){
    menu_render(buffer, menu, position, theme, partial);
  }else{
    pax_draw_text(buffer, theme->palette.color_foreground, theme->footer.text_font, 16, position.x0, position.y0+18 * 0, "Directory empty");
  }
  display_blit_buffer(buffer);
}

// returns true upon quit, false upon back
static bool file_explorer(pax_buf_t* buffer, gui_theme_t* theme, char*path){
  QueueHandle_t input_event_queue = NULL;
  ESP_ERROR_CHECK(bsp_input_get_queue(&input_event_queue));

    int header_height = theme->header.height + (theme->header.vertical_margin * 2);
    int footer_height = theme->footer.height + (theme->footer.vertical_margin * 2);

    pax_vec2_t position = {
        .x0 = theme->menu.horizontal_margin + theme->menu.horizontal_padding,
        .y0 = header_height + theme->menu.vertical_margin + theme->menu.vertical_padding,
        .x1 = pax_buf_get_width(buffer) - theme->menu.horizontal_margin - theme->menu.horizontal_padding,
        .y1 = pax_buf_get_height(buffer) - footer_height - theme->menu.vertical_margin
	    - theme->menu.vertical_padding,
        };


  menu_t menu = {0};
  menu_initialize(&menu);

  populate_menu(&menu,path);
  render(buffer, theme, &menu, path, position, false, true);

  while(1){
    bsp_input_event_t event;
    if (xQueueReceive(input_event_queue, &event, pdMS_TO_TICKS(1000)) == pdTRUE){
      switch(event.type) {
	case INPUT_EVENT_TYPE_NAVIGATION:
	  if(event.args_navigation.state) {
	    switch(event.args_navigation.key){
	      case BSP_INPUT_NAVIGATION_KEY_ESC:
	      case BSP_INPUT_NAVIGATION_KEY_F1:
		menu_free(&menu);
		return true;
	      case BSP_INPUT_NAVIGATION_KEY_F2:
	      case BSP_INPUT_NAVIGATION_KEY_GAMEPAD_B:
		menu_free(&menu);
		return false;
	      case BSP_INPUT_NAVIGATION_KEY_UP:
                menu_navigate_previous(&menu);
                render(buffer, theme, &menu, path, position, true, false);
                break;
              case BSP_INPUT_NAVIGATION_KEY_DOWN:
                menu_navigate_next(&menu);
                render(buffer, theme, &menu, path, position, true, false);
                break;
              case BSP_INPUT_NAVIGATION_KEY_RETURN:
              case BSP_INPUT_NAVIGATION_KEY_GAMEPAD_A:
		if(menu_get_length(&menu) < 1)break;
		menu_item_t*menu_item = menu_find_item(&menu, menu_get_position(&menu));
		int newpathlen = strlen(path)+strlen(menu_item->label)+2;
		char*newpath=malloc(newpathlen);
		snprintf(newpath,newpathlen,"%s/%s",path,menu_item->label);
		unsigned char dtype = (unsigned char)menu_item->callback_arguments;
		switch(dtype){
		  case DT_DIR:
		    if(file_explorer(buffer,theme,newpath)){
		      free(newpath);
		      menu_free(&menu);
		      return true;
		    }
		}
		free(newpath);
                render(buffer, theme, &menu, path, position, false, true);
	      default:break;
	    }
	  }
	default:break;
      }
    }
  }
}

void menu_file_explorer(pax_buf_t* buffer, gui_theme_t* theme){
  file_explorer(buffer,theme,"/sd");
}


