# Tanmatsu Plugin SDK

This SDK allows you to build plugins for the Tanmatsu launcher.

## Prerequisites

- ESP-IDF v5.x installed and configured
- CMake 3.16 or newer
- RISC-V toolchain (installed with ESP-IDF)

## Quick Start

1. Create a new directory for your plugin:
   ```bash
   mkdir my-plugin
   cd my-plugin
   ```

2. Create `CMakeLists.txt`:
   ```cmake
   cmake_minimum_required(VERSION 3.16)

   set(PLUGIN_NAME "my-plugin")
   set(TANMATSU_PLUGIN_SDK "/path/to/tanmatsu-launcher/tools/plugin-sdk")

   project(${PLUGIN_NAME} C)
   include(${TANMATSU_PLUGIN_SDK}/plugin-build.cmake)

   set(PLUGIN_SOURCES src/main.c)
   build_tanmatsu_plugin(${PLUGIN_NAME} "${PLUGIN_SOURCES}")
   ```

3. Create `plugin.json`:
   ```json
   {
       "name": "My Plugin",
       "slug": "my-plugin",
       "version": "1.0.0",
       "author": "Your Name",
       "description": "Plugin description",
       "type": "menu",
       "api_version": 1,
       "permissions": ["display", "input"],
       "autostart": false
   }
   ```

4. Create `src/main.c`:
   ```c
   #include "tanmatsu_plugin.h"

   static const plugin_info_t plugin_info = {
       .name = "My Plugin",
       .slug = "my-plugin",
       .version = "1.0.0",
       .author = "Your Name",
       .description = "Plugin description",
       .api_version = TANMATSU_PLUGIN_API_VERSION,
       .type = PLUGIN_TYPE_MENU,
       .flags = 0,
   };

   static const plugin_info_t* get_info(void) {
       return &plugin_info;
   }

   static int plugin_init(plugin_context_t* ctx) {
       plugin_log_info("my-plugin", "Plugin initialized");
       return 0;
   }

   static void plugin_cleanup(plugin_context_t* ctx) {
       plugin_log_info("my-plugin", "Plugin cleanup");
   }

   static const plugin_entry_t entry = {
       .get_info = get_info,
       .init = plugin_init,
       .cleanup = plugin_cleanup,
   };

   TANMATSU_PLUGIN_REGISTER(entry);
   ```

5. Build:
   ```bash
   # Source ESP-IDF environment first
   source /path/to/esp-idf/export.sh

   mkdir build && cd build
   cmake ..
   make
   ```

6. Install to device:
   ```bash
   # Copy to SD card
   mkdir -p /path/to/sd/plugins/my-plugin
   cp my-plugin.plugin /path/to/sd/plugins/my-plugin/
   cp ../plugin.json /path/to/sd/plugins/my-plugin/
   ```

## Plugin Types

- **PLUGIN_TYPE_MENU**: Adds items to launcher menu
- **PLUGIN_TYPE_SERVICE**: Background service running in its own task
- **PLUGIN_TYPE_HOOK**: Receives system events (app launch, WiFi, etc.)

## API Reference

See `PLUGINAPIS.md` in the tanmatsu-launcher repository for complete API documentation.

## Plugin Directory Structure

```
/sd/plugins/my-plugin/
├── my-plugin.plugin    # Compiled plugin binary
├── plugin.json         # Metadata
└── data/               # Plugin-specific data (optional)
```

Plugins can also be installed to `/int/plugins/` (internal flash).

## Permissions

Available permissions in `plugin.json`:
- `display`: Access to display buffer
- `input`: Access to input events
- `storage`: File system access (sandboxed to plugin directory)
- `network`: Network access
- `settings`: NVS settings access

## Debugging

The plugin build generates a `.map` file showing memory layout. Use this to debug size issues.

To see plugin logs, connect via serial and look for messages tagged with your plugin's slug.
