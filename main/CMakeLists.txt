if (CONFIG_BSP_TARGET_TANMATSU OR CONFIG_BSP_TARGET_KONSOOL OR CONFIG_BSP_TARGET_HACKERHOTEL_2026)
list(APPEND files "tanmatsu_coprocessor.bin")
endif()

if (CONFIG_IDF_TARGET_ESP32P4)
list(APPEND extra_requires esp-hosted-tanmatsu)
endif()

idf_component_register(
	SRCS
		"main.c"
		"common/display.c"
		"menu/home.c"
		"menu/settings.c"
		"menu/about.c"
		"menu/wifi.c"
		"menu/wifi_edit.c"
		"menu/wifi_scan.c"
		"menu/apps.c"
		"menu/firmware_update.c"
		"menu/message_dialog.c"
		"menu/device_information.c"
		"menu/settings_clock.c"
		"menu/settings_clock_timezone.c"
		"menu/charging_mode.c"
		"menu/terminal.c"
		"menu/textedit.c"
		"menu/radio_update.c"
		"chakrapetchmedium.c"
		"coprocessor_management.c"
		"wifi_remote.c"
		"wifi_ota.c"
		"icons.c"
		"usb_device.c"
		"esp_efuse_custom_table.c"
		"sdcard.c"
		"app_metadata_parser.c"
		"python.c"
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
		${extra_requires}
	INCLUDE_DIRS
		"."
		"include"
	EMBED_FILES
		${files}
)

fatfs_create_spiflash_image(locfd ../fat/${DEVICE} FLASH_IN_PROJECT)
