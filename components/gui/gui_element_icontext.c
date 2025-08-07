#include "gui_element_icontext.h"
#include <stddef.h>
#include "gui_style.h"
#include "pax_gfx.h"
#include "pax_types.h"

float gui_icontext_width(gui_theme_t* theme, int x, int y, gui_element_icontext_t* content, float padding) {
    float icon_width = 0;
    if (content->icon) {
        icon_width = pax_buf_get_width(content->icon);
    }
    pax_vec2f text_size = pax_text_size(theme->footer.text_font, theme->footer.text_height, content->text);
    return icon_width + padding + text_size.x;
}

float gui_icontext_draw(pax_buf_t* pax_buffer, gui_theme_t* theme, int x, int y, gui_element_icontext_t* content,
                        float padding, int box_height) {
    float icon_width = 0;
    if (content->icon) {
        icon_width        = pax_buf_get_width(content->icon);
        float icon_height = pax_buf_get_height(content->icon);
        pax_draw_image(pax_buffer, content->icon, x, y + (box_height - icon_height) / 2);
    }
    pax_draw_text(pax_buffer, theme->palette.color_foreground, theme->footer.text_font, theme->footer.text_height,
                  x + icon_width + padding, y + ((float)(box_height - theme->footer.text_height)) / 2.0f,
                  content->text);
    pax_vec2f text_size = pax_text_size(theme->footer.text_font, theme->footer.text_height, content->text);
    return icon_width + padding + text_size.x;
}
