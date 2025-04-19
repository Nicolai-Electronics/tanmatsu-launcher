# CMake file for compiling PAX as a library.

cmake_minimum_required(VERSION 3.13)

# Set language standards requested.
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Helpful for IDE users.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Add sources.
include(${CMAKE_CURRENT_LIST_DIR}/sources.cmake)
if(DEFINED PAX_COMPILE_CXX)
	set(PAX_CODECS_SRCS ${PAX_CODECS_SRCS_C})
	set(PAX_CODECS_INCLUDE ${PAX_CODECS_INCLUDE_C})
else()
	set(PAX_CODECS_SRCS ${PAX_CODECS_SRCS_C} ${PAX_CODECS_SRCS_CXX})
	set(PAX_CODECS_INCLUDE ${PAX_CODECS_INCLUDE_C} ${PAX_CODECS_INCLUDE_CXX})
endif()

# Create output object.
add_library(pax_codecs STATIC ${PAX_CODECS_SRCS})
target_include_directories(pax_codecs PUBLIC ${PAX_CODECS_INCLUDE})

# Link to PAX graphics
target_link_libraries(pax_codecs pax_graphics)

# Link to ZLIB
target_link_libraries(${TARGET} z)
