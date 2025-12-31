// SPDX-License-Identifier: MIT
// Stub library for plugin linking
// These stubs are used at link time to create proper ELF dependencies.
// At runtime, kbelf resolves these symbols from the host.
// Function names use the asp_* convention for BadgeELF compatibility.

// Logging API
void asp_log_info(const char* tag, const char* format, ...) {}
void asp_log_warn(const char* tag, const char* format, ...) {}
void asp_log_error(const char* tag, const char* format, ...) {}

// Display API
// Note: asp_disp_get_pax_buf() is provided by the badge library
void asp_disp_flush(void) {}
void asp_disp_flush_region(int x, int y, int w, int h) {}

// Status Bar Widget API
int asp_plugin_status_widget_register(void* callback, void* user_data) { return 0; }
void asp_plugin_status_widget_unregister(int widget_id) {}

// Drawing Primitives - Use PAX library directly (pax_draw_circle, pax_draw_rect, etc.)
// These are already exported via kbelf_lib_pax_gfx

// Input API
int asp_plugin_input_poll(void* event, unsigned int timeout_ms) { return 0; }
int asp_plugin_input_get_key_state(unsigned int key) { return 0; }

// Input Hook API
int asp_plugin_input_hook_register(void* callback, void* user_data) { return 0; }
void asp_plugin_input_hook_unregister(int hook_id) {}
int asp_plugin_input_inject(void* event) { return 0; }

// LED API
int asp_led_set_brightness(unsigned char percentage) { return 0; }
int asp_led_get_brightness(unsigned char* out_percentage) { return 0; }
int asp_led_set_mode(int automatic) { return 0; }
int asp_led_get_mode(int* out_automatic) { return 0; }
int asp_led_set_pixel(unsigned int index, unsigned int color) { return 0; }
int asp_led_set_pixel_rgb(unsigned int index, unsigned char r, unsigned char g, unsigned char b) { return 0; }
int asp_led_set_pixel_hsv(unsigned int index, unsigned short hue, unsigned char sat, unsigned char val) { return 0; }
int asp_led_send(void) { return 0; }
int asp_led_clear(void) { return 0; }

// Storage API
void* asp_plugin_storage_open(void* ctx, const char* path, const char* mode) { return 0; }
unsigned int asp_plugin_storage_read(void* file, void* buffer, unsigned int size) { return 0; }
unsigned int asp_plugin_storage_write(void* file, const void* buffer, unsigned int size) { return 0; }
int asp_plugin_storage_seek(void* file, long offset, int whence) { return 0; }
long asp_plugin_storage_tell(void* file) { return 0; }
void asp_plugin_storage_close(void* file) {}
int asp_plugin_storage_exists(void* ctx, const char* path) { return 0; }
int asp_plugin_storage_mkdir(void* ctx, const char* path) { return 0; }
int asp_plugin_storage_remove(void* ctx, const char* path) { return 0; }

// Memory API - Use standard libc (malloc, calloc, realloc, free)
// These are already exported via kbelf_lib_c

// Timer API
void asp_plugin_delay_ms(unsigned int ms) {}
unsigned int asp_plugin_get_tick_ms(void) { return 0; }
int asp_plugin_should_stop(void* ctx) { return 0; }

// Menu API
int asp_plugin_menu_add_item(const char* label, void* icon, void* callback, void* user_data) { return 0; }
void asp_plugin_menu_remove_item(int item_id) {}

// Event API
int asp_plugin_event_register(unsigned int event_mask, void* callback, void* user_data) { return 0; }
void asp_plugin_event_unregister(int registration_id) {}

// Network API
int asp_net_is_connected(void) { return 0; }
int asp_http_get(const char* url, void* response, unsigned int max_len) { return 0; }
int asp_http_post(const char* url, const char* body, void* response, unsigned int max_len) { return 0; }

// Settings API
int asp_plugin_settings_get_string(void* ctx, const char* key, char* value, unsigned int max_len) { return 0; }
int asp_plugin_settings_set_string(void* ctx, const char* key, const char* value) { return 0; }
int asp_plugin_settings_get_int(void* ctx, const char* key, int* value) { return 0; }
int asp_plugin_settings_set_int(void* ctx, const char* key, int value) { return 0; }

// Power Information API
int asp_power_get_battery_info(void* out_info) { return 0; }
int asp_power_get_system_voltage(unsigned short* out_millivolt) { return 0; }
int asp_power_get_battery_voltage(unsigned short* out_millivolt) { return 0; }
int asp_power_get_input_voltage(unsigned short* out_millivolt) { return 0; }
int asp_power_get_charging_config(int* out_disabled, unsigned short* out_current_ma) { return 0; }
int asp_power_set_charging(int disable, unsigned short current_ma) { return 0; }
int asp_power_get_usb_boost(int* out_enabled) { return 0; }
int asp_power_set_usb_boost(int enable) { return 0; }
