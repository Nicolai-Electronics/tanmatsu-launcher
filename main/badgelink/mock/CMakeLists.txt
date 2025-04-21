
# SPDX-Copyright-Text: 2025 Julian Scheffers
# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.31)
project(badgemock)

set(CMAKE_EXPORT_COMPILE_COMMANDS true)
set(target badgemock)

add_executable(${target}
    ../badgelink_appfs.c
    ../badgelink_fs.c
    ../badgelink_nvs.c
    ../badgelink.c
    ../badgelink.pb.c
    ../cobs.c
    
    ../nanopb/pb_common.c
    ../nanopb/pb_encode.c
    ../nanopb/pb_decode.c
    
    src/appfs_mock/appfs.c
    
    src/esp_mock/esp_crc.c
    src/esp_mock/esp_err.c
    src/esp_mock/esp_log.c
    src/esp_mock/nvs.c
    
    src/freertos_mock/freertos.c
    
    src/main.c
)
target_include_directories(${target} PRIVATE .. ../nanopb src/appfs_mock src/esp_mock src/freertos_mock)
target_compile_options(${target} PRIVATE -Werror=all)
target_link_libraries(${target} PRIVATE pthread z)
target_compile_options(${target} PRIVATE -ggdb)
