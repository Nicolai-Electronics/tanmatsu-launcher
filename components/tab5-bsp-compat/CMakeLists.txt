if(CONFIG_TANMATSU_LAUNCHER_M5STACK_TAB5)
    idf_component_register(
        SRCS "tab5_bsp_compat.c"
        REQUIRES badge-bsp m5stack_tab5_noglib esp_lcd_touch esp_driver_i2c
        WHOLE_ARCHIVE
    )
else()
    idf_component_register()
endif()
