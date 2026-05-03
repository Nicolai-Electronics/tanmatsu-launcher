idf_build_get_property(idf_target IDF_TARGET)
if("${idf_target}" STREQUAL "esp32p4")
idf_component_register(
    SRCS "src/hid_keyboard.c"
    INCLUDE_DIRS "include"
    REQUIRES usb badge-bsp
)

endif()