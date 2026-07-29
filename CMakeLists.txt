cmake_minimum_required(VERSION 3.10)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(launcher)

# The vendored zlib (pulled in by robotman2412/pax-codecs) uses pre-C99
# K&R style function definitions, which newer GCC versions reject under
# -Werror=old-style-definition. Silence just that warning for zlib.
foreach(zlib_target zlib zlibstatic)
    if(TARGET ${zlib_target})
        target_compile_options(${zlib_target} PRIVATE
            -Wno-error=old-style-definition
            -Wno-error=implicit-fallthrough
        )
    endif()
endforeach()
