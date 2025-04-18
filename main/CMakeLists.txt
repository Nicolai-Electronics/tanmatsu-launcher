if (CONFIG_BSP_TARGET_TANMATSU)
list(APPEND files "tanmatsu_coprocessor.bin")
endif()

idf_component_register(
	SRCS
		"main.c"
		"common/display.c"
		"menu/home.c"
		"chakrapetchmedium.c"
		"coprocessor_management.c"
		"wifi_remote.c"
		"wifi_ota.c"
	PRIV_REQUIRES
		esp_lcd
		fatfs
		nvs_flash
		badge-bsp
		gui
		esp_wifi_remote
		esp_hosted
		wpa_supplicant
		esp_https_ota
		app_update
		esp_bootloader_format
		custom-certificates
		wifi-manager
		esp-tls
	INCLUDE_DIRS
		"."
	EMBED_FILES
		${files}
)

fatfs_create_spiflash_image(fat fat FLASH_IN_PROJECT)
