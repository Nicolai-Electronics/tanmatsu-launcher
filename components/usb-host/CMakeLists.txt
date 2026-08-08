idf_build_get_property(idf_target IDF_TARGET)
if("${idf_target}" STREQUAL "esp32p4" OR "${idf_target}" STREQUAL "esp32s31")
idf_component_register(
    SRCS "src/hid_keyboard.c" "src/hid_mouse.c" "src/hid_gamepad.c" "src/hid_report.c"
    INCLUDE_DIRS "include"
    REQUIRES usb badge-bsp
)
endif()
