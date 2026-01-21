if (CONFIG_BSP_TARGET_TANMATSU OR CONFIG_BSP_TARGET_KONSOOL)
list(APPEND files "tanmatsu_coprocessor.bin")
endif()

idf_build_get_property(idf_target IDF_TARGET)
if("${idf_target}" STREQUAL "esp32p4")
list(APPEND extra_requires esp-hosted-tanmatsu badge-elf)
endif()

idf_component_register(
	SRCS
		# Application
		"main.c"
		"common/display.c"
		"common/theme.c"
		
		# Menus
		"menu/home.c"
		"menu/settings.c"
		"menu/information.c"
		"menu/tools.c"
		"menu/about.c"
		"menu/wifi.c"
		"menu/wifi_edit.c"
		"menu/wifi_scan.c"
		"menu/apps.c"
		"menu/app_inspect.c"
		"menu/firmware_update.c"
		"menu/message_dialog.c"
		"menu/menu_device_information.c"
		"menu/settings_clock.c"
		"menu/settings_clock_timezone.c"
		"menu/charging_mode.c"
		"menu/terminal.c"
		"menu/textedit.c"
		"menu/radio_update.c"
		"menu/nametag.c"
		"menu/menu_rftest.c"
		"menu/menu_repository_client.c"
		"menu/menu_repository_client_project.c"
		"menu/menu_wifi_info.c"
		"menu/menu_power_information.c"
		"menu/menu_filebrowser.c"
		"menu/menu_hardware_test.c"
		"menu/menu_brightness.c"
		"menu/settings_repository.c"
		"menu/owner.c"
		"menu/chat/chat.c"
		"menu/chat/lora_settings.c"
		"menu/chat/lora_presets.c"
		"menu/chat/messages.c"

		# Tests
		"hardware_test/test_keyboard_stuck_keys.c"
		"hardware_test/test_keyboard.c"

		# Fonts
		"chakrapetchmedium.c"
		"rajdhani.c"

		# Libraries
		"coprocessor_management.c"
		"wifi_ota.c"
		"icons.c"
		"usb_device.c"
		"esp_efuse_custom_table.c"
		"sdcard.c"
		"addon.c"
		"app_metadata_parser.c"
		"python.c"
		"http_download.c"
		"repository_client.c"
		"device_information.c"
		"filesystem_utils.c"
		"app_management.c"
		"ntp.c"
		"device_settings.c"
		"lora.c"
		"fastopen.c"

	PRIV_REQUIRES
		esp_lcd
		fatfs
		nvs_flash
		badge-bsp
		gui
		wpa_supplicant
		esp_https_ota
		app_update
		esp_bootloader_format
		custom-certificates
		wifi-manager
		esp-tls
		pax-gfx
		pax-codecs
		efuse
		timezone
		json
		freertos
		badgelink
		tanmatsu-wifi
		lwip
		eeprom
		${extra_requires}
	INCLUDE_DIRS
		"."
		"include"
		"hardware_test"
	EMBED_FILES
		${files}
)

if (FAT STREQUAL "1")
fatfs_create_spiflash_image(locfd ../fat/${DEVICE} FLASH_IN_PROJECT)
endif()