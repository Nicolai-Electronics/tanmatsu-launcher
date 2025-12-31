# SPDX-License-Identifier: MIT
# Tanmatsu Plugin SDK - RISC-V Toolchain Configuration
#
# This toolchain file configures CMake for building Tanmatsu plugins
# targeting the ESP32-P4 (RISC-V) processor.

cmake_minimum_required(VERSION 3.16)

# Target system
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR riscv32)

# Find the RISC-V toolchain from ESP-IDF
if(NOT DEFINED ENV{IDF_PATH})
    message(FATAL_ERROR "IDF_PATH environment variable not set. Please source ESP-IDF export.sh")
endif()

# Get the toolchain path from ESP-IDF tools
if(NOT DEFINED ENV{IDF_TOOLS_PATH})
    set(IDF_TOOLS_PATH "$ENV{HOME}/.espressif")
else()
    set(IDF_TOOLS_PATH "$ENV{IDF_TOOLS_PATH}")
endif()

# Find riscv32 toolchain
file(GLOB RISCV_TOOLCHAIN_DIRS "${IDF_TOOLS_PATH}/tools/riscv32-esp-elf/*")
list(SORT RISCV_TOOLCHAIN_DIRS ORDER DESCENDING)
if(RISCV_TOOLCHAIN_DIRS)
    list(GET RISCV_TOOLCHAIN_DIRS 0 RISCV_TOOLCHAIN_DIR)
    set(TOOLCHAIN_PREFIX "${RISCV_TOOLCHAIN_DIR}/riscv32-esp-elf/bin/riscv32-esp-elf-")
else()
    # Try to find it in PATH
    find_program(RISCV_GCC riscv32-esp-elf-gcc)
    if(RISCV_GCC)
        get_filename_component(TOOLCHAIN_BIN_DIR ${RISCV_GCC} DIRECTORY)
        set(TOOLCHAIN_PREFIX "${TOOLCHAIN_BIN_DIR}/riscv32-esp-elf-")
    else()
        message(FATAL_ERROR "Could not find riscv32-esp-elf toolchain. Please ensure ESP-IDF is properly installed.")
    endif()
endif()

# Set compilers
set(CMAKE_C_COMPILER "${TOOLCHAIN_PREFIX}gcc")
set(CMAKE_CXX_COMPILER "${TOOLCHAIN_PREFIX}g++")
set(CMAKE_ASM_COMPILER "${TOOLCHAIN_PREFIX}gcc")
set(CMAKE_AR "${TOOLCHAIN_PREFIX}ar")
set(CMAKE_RANLIB "${TOOLCHAIN_PREFIX}ranlib")
set(CMAKE_OBJCOPY "${TOOLCHAIN_PREFIX}objcopy")
set(CMAKE_OBJDUMP "${TOOLCHAIN_PREFIX}objdump")
set(CMAKE_SIZE "${TOOLCHAIN_PREFIX}size")

# ESP32-P4 architecture flags (RISC-V with F extension for FPU)
set(ARCH_FLAGS "-march=rv32imafc_zicsr_zifencei -mabi=ilp32f")

# Common C flags for position-independent code
set(CMAKE_C_FLAGS_INIT "${ARCH_FLAGS} -fPIC -ffunction-sections -fdata-sections -fno-common")
set(CMAKE_CXX_FLAGS_INIT "${ARCH_FLAGS} -fPIC -ffunction-sections -fdata-sections -fno-common -fno-exceptions -fno-rtti")
set(CMAKE_ASM_FLAGS_INIT "${ARCH_FLAGS}")

# Plugin-specific flags
set(PLUGIN_C_FLAGS "-Wall -Wextra -Wno-unused-parameter -Os")
set(PLUGIN_LINK_FLAGS "-shared -nostdlib -Wl,--gc-sections -Wl,--no-undefined")

# Don't try to compile test programs (cross-compiling)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Search paths
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
