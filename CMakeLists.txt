cmake_minimum_required(VERSION 3.10)

# Device variable
# Set to the name of the badge-bsp target
# Supported values:
# - tanmatsu
# - konsool
# - esp32-p4-function-ev-board
# - mch2022
# - kami
# - hackerhotel-2024

if(DEVICE STREQUAL "")
	set(DEVICE "$ENV{TARGET}")
endif()

if(DEVICE STREQUAL "")
	message(FATAL_ERROR "DEVICE not set")
endif()

# FAT variable
# Set to 1 to generate and/or flash the FAT partition image or to 0 to disable

if(FAT STREQUAL "")
	set(FAT "$ENV{FAT}")
endif()

if(FAT STREQUAL "")
	message(FATAL_ERROR "FAT not set")
endif()

# SDKCONFIG_DEFAULTS variable
# Usually you want to set this to "sdkconfigs/general;sdkconfigs/<DEVICE>"

if(SDKCONFIG_DEFAULTS STREQUAL "")
	set(SDKCONFIG_DEFAULTS "$ENV{SDKCONFIG_DEFAULTS}")
endif()

if(SDKCONFIG_DEFAULTS STREQUAL "")
	message(FATAL_ERROR "SDKCONFIG_DEFAULTS not set")
endif()

# SDKCONFIG variable
# Usually you want to set this to "sdkconfig_<DEVICE>"

if(SDKCONFIG STREQUAL "")
	set(SDKCONFIG "$ENV{SDKCONFIG}")
endif()

if(SDKCONFIG STREQUAL "")
	message(FATAL_ERROR "SDKCONFIG not set")
endif()

# IDF_TARGET variable
# Set to esp32, esp32c6 or esp32p4 depending on the target device

if(IDF_TARGET STREQUAL "")
	set(IDF_TARGET "$ENV{IDF_TARGET}")
endif()

if(IDF_TARGET STREQUAL "")
	message(FATAL_ERROR "IDF_TARGET not set")
endif()

#########

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(launcher)
