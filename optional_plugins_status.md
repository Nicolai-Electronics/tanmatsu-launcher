# Optional plugin support — status / progress report

Branch: **fix_plugins** (off `app_management_int_optimization`)
Working tree: **uncommitted changes present** for the plugin-gating work below.

This report covers two pieces of work on this branch:
  1. **Audio mixer for plugins** (already committed and pushed as commit `65d16b1`).
  2. **`CONFIG_ENABLE_LAUNCHERPLUGINS` build-time gate** (uncommitted).

---

## 1. Audio mixer — DONE, COMMITTED, PUSHED

### Problem
Two plugins playing audio via I2S simultaneously produced chopped sound because each plugin called `i2s_channel_write()` directly through `bsp_audio_get_i2s_handle()`, so the DMA buffers were interleaved chunk-by-chunk instead of mixed.

### Solution shipped
Software mixer that owns the I2S channel exclusively. Each plugin task gets its own FreeRTOS `StreamBuffer`. A dedicated mixer task drains every active stream, sums into int32, saturates to int16, and writes to I2S — `i2s_channel_write()` is the natural pacing primitive.

### Architecture
- Format: fixed 44.1 kHz, s16, stereo (matches the BSP default).
- Per-stream ring: 8 KB (~46 ms headroom).
- Mix chunk: 256 frames (~5.8 ms).
- Mixer task: priority 7 (above the priority-5 plugin tasks), 3 KB stack, ~4 KB static scratch buffers (in/accum/out).
- Per-stream `paused` flag so `start`/`stop` operate at stream level (not on the I2S channel, which would silence everyone).
- Concurrent additive writes are not used — the producer-consumer per-stream ring is single-producer / single-consumer, no locking.

### Files (committed)
- `main/audio_mixer.h` — public API: `audio_mixer_init`, `audio_mixer_register_stream`, `audio_mixer_unregister_stream`, `audio_mixer_start`, `audio_mixer_stop`, `audio_mixer_write`.
- `main/audio_mixer.c` — implementation.
- `main/main.c` — `audio_mixer_init()` runs after BSP comes up, before plugin manager.
- `main/plugin_manager.c` — registers a stream when the service task is created (`plugin_manager_start_service`); captures the task handle up front and unregisters at the end of `plugin_manager_stop_service` (the force-stop path nulls `ctx->task_handle`, so the original is captured separately).
- `components/badgeteam__badge-elf-api/src/audio.c` — **forked via `override_path`**; `asp_audio_write` calls `audio_mixer_write(xTaskGetCurrentTaskHandle(), ...)`; `asp_audio_start`/`stop` call `audio_mixer_start`/`audio_mixer_stop`. Volume / amplifier / set_rate still pass straight through to the BSP.
- `main/idf_component.yml` — `badge-elf-api` switched to `version: "*"` with `override_path: "../components/badgeteam__badge-elf-api"`.

### API compatibility
`asp/audio.h` signatures unchanged. Plugins resolve `asp_audio_*` symbols through the kbelf table at load time, so existing plugin ELFs do **not** need recompilation — they automatically get the mixed behavior.

### Behavior changes existing plugins will see
- Concurrent `asp_audio_write` from multiple plugins now mixes instead of chopping.
- `asp_audio_stop` is now per-plugin (used to disable I2S globally for everyone).
- `asp_audio_start` is a no-op on first call (stream is auto-registered as active when plugin task starts) and resumes after a `stop`.
- `asp_audio_set_rate` still passes through to the BSP, so it still affects all streams (known limitation — would need per-stream resampling to fix).

### Status
Committed as `65d16b1` ("Implement audio mixer so multiple plugins/mini-apps can play sound") and pushed to `origin/fix_plugins`.

---

## 2. `CONFIG_ENABLE_LAUNCHERPLUGINS` build-time gate — IMPLEMENTED, NOT YET COMMITTED

### Goal
Allow targets that can't afford plugin support (memory, etc.) to compile out:
  - The plugin manager and plugin API
  - The audio mixer (only used by plugins)
  - The `menu/menu_plugins.c` UI
  - All cross-cutting plugin lifecycle / install / uninstall calls
  - The `badge-elf` and `badge-elf-api` components (skip download AND link)

The user added `CONFIG_ENABLE_LAUNCHERPLUGINS=y` to `sdkconfigs/tanmatsu` (and `sdkconfigs/konsool`).

### Kconfig
`main/Kconfig` declares the option:
```kconfig
config ENABLE_LAUNCHERPLUGINS
    bool "Enable launcher plugin support"
    depends on IDF_TARGET_ESP32P4
    default n
```
Depends on ESP32-P4 because the plugin runtime (badge-elf) is P4-only.

### Component manager (`main/idf_component.yml`)
The trick: ESP-IDF component manager's `if:` rules support kconfig values, but with non-obvious syntax — `$CONFIG{NAME}`, the operators are `&&` / `||` (not `and`/`or`), and a kconfig boolean compares against `True` / `False` (not `1`):
```yaml
badgeteam/badge-elf:
    version: "^0.4.0"
    rules:
      - if: "target == esp32p4 && $CONFIG{ENABLE_LAUNCHERPLUGINS} == True"
badgeteam/badge-elf-api:
    version: "*"
    override_path: "../components/badgeteam__badge-elf-api"
    rules:
      - if: "target == esp32p4 && $CONFIG{ENABLE_LAUNCHERPLUGINS} == True"
```
This is verified working — when the config is off, neither component is even **downloaded** (not present in `managed_components/`). When on, both are pulled normally.

(Discovered by reading `idf_component_tools/manifest/if_parser.py` in `idf-component-manager` 2.4.10. Documentation does not advertise this syntax clearly.)

### CMake (`main/CMakeLists.txt`)
ESP-IDF's early component scan (where `REQUIRES` is determined) does **not** see `CONFIG_*` values. The workaround is `idf_component_optional_requires(PRIVATE ...)`, which runs after Kconfig is processed and is gated by a normal `if(CONFIG_*)`.

```cmake
if(CONFIG_ENABLE_LAUNCHERPLUGINS)
list(APPEND extra_sources
    "plugin_api.c"
    "plugin_manager.c"
    "menu/menu_plugins.c"
    "audio_mixer.c"
)
endif()
...
if(CONFIG_ENABLE_LAUNCHERPLUGINS)
idf_component_optional_requires(PRIVATE badge-elf badge-elf-api plugin-api)
endif()
```

`esp-hosted-tanmatsu` stays unconditional on P4 (it's the WiFi driver, not plugin-related).

### `components/plugin-api/CMakeLists.txt`
Added `REQUIRES badge-elf-api` because `tanmatsu_plugin.h` includes `asp/err.h`. (The original CMakeLists only had `INCLUDE_DIRS "include"`.)

### Cross-cutting source guards
All references to plugin code that survive in non-plugin files are wrapped in `#ifdef CONFIG_ENABLE_LAUNCHERPLUGINS`.

Files modified:
- **`main/main.c`** — `#include "audio_mixer.h"` / `"plugin_manager.h"` and the `audio_mixer_init` / `plugin_manager_init` / `plugin_manager_load_autostart` calls all moved from `#ifdef CONFIG_IDF_TARGET_ESP32P4` → `#ifdef CONFIG_ENABLE_LAUNCHERPLUGINS`.
- **`main/menu/home.c`** — `#include "menu/menu_plugins.h"`, `#include "plugin_manager.h"`, the `ACTION_PLUGINS` enum value, the `case ACTION_PLUGINS`, the two `plugin_manager_shutdown()` calls in OTA paths, and the "Plugins" menu item — all moved from `CONFIG_IDF_TARGET_ESP32P4` → `CONFIG_ENABLE_LAUNCHERPLUGINS`.
- **`main/menu/message_dialog.c`** — `#include "plugin_manager.h"` and `plugin_api_render_status_widgets()` moved to the new guard.
- **`main/menu/apps.c`** — `#include "badge_elf.h"` and the `EXECUTABLE_TYPE_ELF` branch that calls `badge_elf_start()` moved to the new guard. The fall-through "Applets are not supported on this platform" message stays.
- **`main/app_management.c`** — `#include "plugin_manager.h"` and the `is_plugin` cleanup branch (which calls `plugin_manager_get_by_slug`, `_stop_service`, `_unload`) wrapped in the new guard. The `APP_MGMT_LOCATION_*_PLUGINS` enum values and the `app_mgmt_location_is_plugin` helper stay (just enum/function definitions, no plugin symbols referenced).
- **`main/menu/menu_repository_client.c`** — extensive:
  - `VIEW_MODE_PLUGINS` enum value (gated)
  - `want_plugins` initialization in `populate_project_list` (becomes hardcoded `false` when disabled)
  - Header title falls back to `"Repository"` when disabled
  - F2 footer label hidden when disabled
  - F2 toggle handler body removed when disabled
  - Delete title/message simplified to "App" wording when disabled
  - Plugin uninstall branch (`APP_MGMT_LOCATION_*_PLUGINS`) gated
  - Call to `menu_repository_client_project(..., is_plugin)` always passes `false` when disabled

### Files NOT modified intentionally
- `menu/menu_repository_client_project.c` takes `is_plugin` as a runtime parameter. When called with `is_plugin=false` it never enters the plugin branches; the `APP_MGMT_LOCATION_*_PLUGINS` references it contains compile fine because those enum values always exist. No `#ifdef` needed.
- The plugin source files themselves (`plugin_manager.c`, `plugin_api.c`, `menu_plugins.c`, `audio_mixer.c`, `audio_mixer.h`, `plugin_manager.h`, `menu/menu_plugins.h`) are simply not added to the build when the config is off; their internals are not guarded.

### Build verification
Both configurations build cleanly:

| Config             | Binary size | Partition free | badge-elf in `managed_components/` |
|--------------------|-------------|----------------|--------------------------------------|
| `=y` (plugins on)  | 1,875,232 B | 11 %           | yes                                  |
| `=n` (plugins off) | 1,567,904 B | 25 %           | **no — not even downloaded**         |

Savings when disabled: **~307 KB** of flash, ~14 % of the app partition.

Important build hygiene: `sdkconfig_<target>` and the `build/` tree are cached, so after toggling the kconfig you must:
```
make clean && rm sdkconfig_tanmatsu && make build
```
(Saved as memory `feedback_clean_before_build.md`.)

### Last unresolved item
After the user's last note ("there's no need to compile in badge-elf-api when we disable all dynamic library loading"), I changed the optional_requires line to:
```cmake
idf_component_optional_requires(PRIVATE badge-elf badge-elf-api plugin-api)
```
…but the user interrupted before I re-ran a verification build. The change is **applied to the file but not yet built / committed**. Functionally it should be a no-op — badge-elf-api was already coming in transitively via plugin-api's `REQUIRES badge-elf-api` — but the explicit listing is what the user asked for.

### Outstanding decisions for the user
1. Run a final `make clean && rm sdkconfig_tanmatsu && make build` to reconfirm the explicit-listing change builds with the config on. Expected: identical 1,875,232-byte binary, no behavior change.
2. (Optional) Run the disabled build once more to reconfirm: identical 1,567,904 bytes.
3. Commit message — suggested:
   > Add CONFIG_ENABLE_LAUNCHERPLUGINS build-time gate
   >
   > Allow targets to fully exclude the plugin runtime (plugin manager, audio
   > mixer, plugin UI, badge-elf, badge-elf-api) from the build. Saves ~307 KB
   > of flash on tanmatsu when disabled.
4. Push when committed.

### Memory entries written this session (context for future Claude sessions)
- `feedback_no_linker_wrap.md` — never use `--wrap=` linker flags to override managed-component symbols; fork the component into `components/` with `override_path` instead.
- `feedback_clean_before_build.md` — after sdkconfig or Kconfig changes, run `make clean && rm sdkconfig_<target>` before the next `make build`.
