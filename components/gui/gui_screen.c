#include "gui_screen.h"
#include "gui_element_footer.h"
#include "gui_element_header.h"
#include "pax_gfx.h"

void gui_screen_draw(pax_buf_t* buffer, gui_theme_t* theme, bool background, bool header, bool footer,
                     gui_element_icontext_t* header_left, size_t header_left_count,
                     gui_element_icontext_t* header_right, size_t header_right_count,
                     gui_element_icontext_t* footer_left, size_t footer_left_count,
                     gui_element_icontext_t* footer_right, size_t footer_right_count) {
    if (background) {
        pax_background(buffer, theme->palette.color_background);
    }
    if (header) {
        if (!background) {
            pax_simple_rect(buffer, theme->palette.color_background, 0, 0, pax_buf_get_width(buffer),
                            theme->header.height + (theme->header.vertical_margin * 2));
        }
        gui_header_draw(buffer, theme, header_left, header_left_count, header_right, header_right_count);
    }
    if (footer) {
        if (!background) {
            pax_simple_rect(buffer, theme->palette.color_background, 0,
                            pax_buf_get_height(buffer) - theme->footer.height - (theme->footer.vertical_margin * 2),
                            pax_buf_get_width(buffer), theme->footer.height + (theme->footer.vertical_margin * 2));
        }
        gui_footer_draw(buffer, theme, footer_left, footer_left_count, footer_right, footer_right_count);
    }
}
