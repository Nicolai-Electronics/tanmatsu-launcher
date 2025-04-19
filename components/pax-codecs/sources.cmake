
cmake_minimum_required(VERSION 3.13)

# C source files.
set(PAX_CODECS_SRCS_C
	${CMAKE_CURRENT_LIST_DIR}/src/pax_codecs.c
	${CMAKE_CURRENT_LIST_DIR}/libspng/spng/spng.c
)

# C include directories.
set(PAX_CODECS_INCLUDE_C
	${CMAKE_CURRENT_LIST_DIR}/include
	${CMAKE_CURRENT_LIST_DIR}/src
	${CMAKE_CURRENT_LIST_DIR}/libspng/spng
)

# C++ source files.
set(PAX_CODECS_SRCS_CXX
)

# C++ include directories.
set(PAX_CODECS_INCLUDE_CXX
)
