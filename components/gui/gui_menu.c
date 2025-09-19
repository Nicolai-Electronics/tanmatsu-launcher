#include "gui_menu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gui_style.h"

void menu_initialize(menu_t* menu) {
    menu->firstItem         = NULL;
    menu->length            = 0;
    menu->position          = 0;
    menu->previous_position = 0;
}

void _menu_free_item(menu_item_t* menu_item) {
    free(menu_item->label);
    if (menu_item->value) {
        free(menu_item->value);
    }
    free(menu_item);
}

void menu_free(menu_t* menu) {
    menu_item_t* currentItem = menu->firstItem;
    while (currentItem != NULL) {
        menu_item_t* nextItem = currentItem->nextItem;
        _menu_free_item(currentItem);
        currentItem = nextItem;
    }
    menu->firstItem         = NULL;
    menu->length            = 0;
    menu->position          = 0;
    menu->previous_position = 0;
}

menu_item_t* menu_find_item(menu_t* menu, size_t position) {
    menu_item_t* currentItem = menu->firstItem;
    if (currentItem == NULL) return NULL;
    size_t index = 0;
    while (index < position) {
        if (currentItem->nextItem == NULL) break;
        currentItem = currentItem->nextItem;
        index++;
    }
    return currentItem;
}

menu_item_t* menu_find_last_item(menu_t* menu) {
    menu_item_t* lastItem = menu->firstItem;
    if (lastItem == NULL) return NULL;
    while (lastItem->nextItem != NULL) {
        lastItem = lastItem->nextItem;
    }
    return lastItem;
}

bool menu_insert_item_value(menu_t* menu, const char* label, const char* value, menu_callback_t callback,
                            void* callback_arguments, size_t position) {
    if (menu == NULL) return false;
    menu_item_t* newItem = calloc(1, sizeof(menu_item_t));
    if (newItem == NULL) return false;
    size_t label_size = strlen(label) + 1;
    newItem->label    = malloc(label_size);
    if (newItem->label == NULL) {
        free(newItem);
        return false;
    }
    memcpy(newItem->label, label, label_size);
    if (value != NULL) {
        size_t value_size = strlen(value) + 1;
        newItem->value    = calloc(1, value_size);
        if (newItem->value == NULL) {
            free(newItem->label);
            free(newItem);
            return false;
        }
        memcpy(newItem->value, value, value_size);
    }
    newItem->callback           = callback;
    newItem->callback_arguments = callback_arguments;
    newItem->icon               = NULL;
    if (menu->firstItem == NULL) {
        newItem->nextItem     = NULL;
        newItem->previousItem = NULL;
        menu->firstItem       = newItem;
    } else {
        if (position >= menu->length) {
            newItem->previousItem           = menu_find_last_item(menu);
            newItem->nextItem               = NULL;
            newItem->previousItem->nextItem = newItem;
        } else {
            newItem->nextItem     = menu_find_item(menu, position);
            newItem->previousItem = newItem->nextItem->previousItem;  // Copy pointer to previous item to new item
            if (newItem->nextItem != NULL)
                newItem->nextItem->previousItem = newItem;  // Replace pointer to previous item with new item
            if (newItem->previousItem != NULL)
                newItem->previousItem->nextItem = newItem;  // Replace pointer to next item in previous item
        }
    }
    menu->length++;
    return true;
}

bool menu_insert_item(menu_t* menu, const char* label, menu_callback_t callback, void* callback_arguments,
                      size_t position) {
    return menu_insert_item_value(menu, label, NULL, callback, callback_arguments, position);
}

bool menu_insert_item_icon(menu_t* menu, const char* label, menu_callback_t callback, void* callback_arguments,
                           size_t position, pax_buf_t* icon) {
    if (!menu_insert_item(menu, label, callback, callback_arguments, position)) {
        return false;
    }
    menu_item_t* item;
    if (position >= menu->length - 1) {
        item = menu_find_last_item(menu);
    } else {
        item = menu_find_item(menu, position);
    }

    item->icon = icon;
    return true;
}

bool menu_remove_item(menu_t* menu, size_t position) {
    if (menu == NULL) return false;              // Can't delete an item from a menu that doesn't exist
    if (menu->length <= position) return false;  // Can't delete an item that doesn't exist
    menu_item_t* item;

    if (position == 0) {
        item = menu->firstItem;
        if (item == NULL) return false;  // Can't delete if no linked list is allocated
        if (item->nextItem != NULL) {
            menu->firstItem               = item->nextItem;
            menu->firstItem->previousItem = NULL;
        } else {
            menu->firstItem = NULL;
        }
    } else {
        item = menu_find_item(menu, position);
        if (item == NULL) return false;
        if (item->previousItem != NULL) item->previousItem->nextItem = item->nextItem;
        if (item->nextItem != NULL) item->nextItem->previousItem = item->previousItem;
    }
    free(item->label);
    if (item->value) {
        free(item->value);
    }
    free(item);
    menu->length--;
    if (menu->length < 1) {
        menu->position = 0;
    }
    if (menu->position >= menu->length) {
        menu->position = menu->length - 1;
    }
    return true;
}

bool menu_navigate_to(menu_t* menu, size_t position) {
    if (menu == NULL) return false;
    if (menu->length < 1) return false;
    menu->previous_position = menu->position;
    menu->position          = position;
    if (menu->position >= menu->length) menu->position = menu->length - 1;
    return true;
}

void menu_navigate_previous(menu_t* menu) {
    if (menu == NULL) return;
    if (menu->length < 1) return;
    menu->previous_position = menu->position;
    menu->position--;
    if (menu->position > menu->length) {
        menu->position = menu->length - 1;
    }
}

void menu_navigate_next(menu_t* menu) {
    if (menu == NULL) return;
    if (menu->length < 1) return;
    menu->previous_position = menu->position;
    menu->position          = (menu->position + 1) % menu->length;
}

void menu_navigate_previous_row(menu_t* menu, gui_theme_t* theme) {
    int previous_position = menu->position;
    for (size_t index = 0; index < theme->menu.grid_horizontal_count; index++) {
        menu_navigate_previous(menu);
    }
    menu->previous_position = previous_position;
}

void menu_navigate_next_row(menu_t* menu, gui_theme_t* theme) {
    int previous_position = menu->position;
    for (size_t index = 0; index < theme->menu.grid_horizontal_count; index++) {
        menu_navigate_next(menu);
    }
    menu->previous_position = previous_position;
}

size_t menu_get_position(menu_t* menu) {
    return menu->position;
}

void menu_set_position(menu_t* menu, size_t position) {
    menu->previous_position = menu->position;
    menu->position          = position;
    if (menu->length < 1) {
        menu->position = 0;
    } else if (menu->position >= menu->length) {
        menu->position = menu->length - 1;
    }
}

size_t menu_get_length(menu_t* menu) {
    return menu->length;
}

void* menu_get_callback_args(menu_t* menu, size_t position) {
    menu_item_t* item = menu_find_item(menu, position);
    if (item == NULL) return NULL;
    return item->callback_arguments;
}

pax_buf_t* menu_get_icon(menu_t* menu, size_t position) {
    menu_item_t* item = menu_find_item(menu, position);
    if (item == NULL) return NULL;
    return item->icon;
}

const char* menu_get_value(menu_t* menu, size_t position) {
    menu_item_t* item = menu_find_item(menu, position);
    if (item == NULL) return NULL;
    return item->value;
}

const char* menu_get_label(menu_t* menu, size_t position) {
    menu_item_t* item = menu_find_item(menu, position);
    if (item == NULL) return NULL;
    return item->label;
}

void menu_set_value(menu_t* menu, size_t position, const char* value) {
    menu_item_t* item = menu_find_item(menu, position);
    if (item == NULL) {
        return;
    }
    if (item->value) {
        free(item->value);
        item->value = NULL;
    }
    if (value) {
        size_t value_size = strlen(value) + 1;
        item->value       = calloc(1, value_size);
        if (item->value != NULL) {
            memcpy(item->value, value, value_size);
        }
    }
}
