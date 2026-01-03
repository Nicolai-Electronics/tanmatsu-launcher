# SPDX-License-Identifier: MIT
# Tanmatsu Plugin SDK - Plugin Build Helper
#
# Include this file in your plugin's CMakeLists.txt after setting
# TANMATSU_PLUGIN_SDK to the path of this SDK directory.
#
# Usage:
#   set(TANMATSU_PLUGIN_SDK "/path/to/tanmatsu-launcher/tools/plugin-sdk")
#   include(${TANMATSU_PLUGIN_SDK}/plugin-build.cmake)
#   build_tanmatsu_plugin(my-plugin "src/main.c;src/other.c")

cmake_minimum_required(VERSION 3.16)

# Get SDK paths
get_filename_component(PLUGIN_SDK_DIR "${CMAKE_CURRENT_LIST_DIR}" ABSOLUTE)
get_filename_component(LAUNCHER_DIR "${PLUGIN_SDK_DIR}/../.." ABSOLUTE)

# Set toolchain if not already set
if(NOT CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "${PLUGIN_SDK_DIR}/toolchain-plugin.cmake" CACHE PATH "" FORCE)
endif()

# Include directories for plugin API
set(PLUGIN_API_INCLUDE_DIRS
    "${LAUNCHER_DIR}/components/plugin-api/include"
    "${LAUNCHER_DIR}/components/badgeteam__badge-elf-api/include"
    "${LAUNCHER_DIR}/managed_components/robotman2412__pax-gfx/core/include"
)

# Stub library paths - link against BadgeELF libraries
set(PLUGIN_STUB_SOURCE "${PLUGIN_SDK_DIR}/lib/libplugin_stubs.c")
set(PLUGIN_STUB_LIB_DIR "${CMAKE_CURRENT_BINARY_DIR}/lib")
# Use pre-built fake libraries from badge-elf component
set(BADGE_ELF_FAKELIB_DIR "${LAUNCHER_DIR}/components/badgeteam__badge-elf/fakelib")

# Function to build a Tanmatsu plugin
function(build_tanmatsu_plugin PLUGIN_NAME PLUGIN_SOURCES)
    # Plugin output name
    set(PLUGIN_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${PLUGIN_NAME}.plugin")

    # No need to build stub library - use pre-built fakelibs from badge-elf

    # Create object library for compilation
    add_library(${PLUGIN_NAME}_obj OBJECT ${PLUGIN_SOURCES})

    # Include directories
    target_include_directories(${PLUGIN_NAME}_obj PRIVATE
        ${PLUGIN_API_INCLUDE_DIRS}
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    # Compiler flags for position-independent code
    target_compile_options(${PLUGIN_NAME}_obj PRIVATE
        -Wall
        -Wextra
        -Wno-unused-parameter
        -Os
        -fPIC
        -ffunction-sections
        -fdata-sections
        -fno-common
    )

    # Custom command to link the shared object
    # Note: Using COMMAND_EXPAND_LISTS to properly handle multiple object files
    add_custom_command(
        OUTPUT ${PLUGIN_OUTPUT}
        COMMAND ${CMAKE_C_COMPILER}
            -shared
            -nostdlib
            -Wl,--gc-sections
            -Wl,-T,${PLUGIN_SDK_DIR}/plugin.ld
            -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/${PLUGIN_NAME}.map
            -Wl,--no-as-needed
            -L${BADGE_ELF_FAKELIB_DIR}
            $<TARGET_OBJECTS:${PLUGIN_NAME}_obj>
            -lbadge
            -lpax-gfx
            -lpthread
            -lc
            -lgcc
            -o ${PLUGIN_OUTPUT}
        DEPENDS ${PLUGIN_NAME}_obj
        COMMENT "Linking plugin ${PLUGIN_NAME}.plugin"
        COMMAND_EXPAND_LISTS
    )

    # Custom target for the plugin
    add_custom_target(${PLUGIN_NAME} ALL
        DEPENDS ${PLUGIN_OUTPUT}
    )

    # Post-build: show plugin size
    add_custom_command(TARGET ${PLUGIN_NAME} POST_BUILD
        COMMAND ${CMAKE_SIZE} ${PLUGIN_OUTPUT}
        COMMENT "Plugin size:"
    )

    # Post-build: show section info
    add_custom_command(TARGET ${PLUGIN_NAME} POST_BUILD
        COMMAND ${CMAKE_OBJDUMP} -h ${PLUGIN_OUTPUT}
        COMMENT "Plugin sections:"
    )
endfunction()

# Function to install plugin to a target directory
function(install_tanmatsu_plugin PLUGIN_NAME INSTALL_DIR)
    # Create plugin directory
    set(PLUGIN_INSTALL_DIR "${INSTALL_DIR}/${PLUGIN_NAME}")
    set(PLUGIN_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${PLUGIN_NAME}.plugin")

    add_custom_command(TARGET ${PLUGIN_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${PLUGIN_INSTALL_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy "${PLUGIN_OUTPUT}" "${PLUGIN_INSTALL_DIR}/"
        COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/plugin.json" "${PLUGIN_INSTALL_DIR}/"
        COMMENT "Installing plugin to ${PLUGIN_INSTALL_DIR}"
    )
endfunction()

message(STATUS "Tanmatsu Plugin SDK loaded from: ${PLUGIN_SDK_DIR}")
message(STATUS "Plugin API headers: ${PLUGIN_API_INCLUDE_DIRS}")
