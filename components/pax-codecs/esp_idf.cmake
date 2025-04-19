idf_component_register(
	SRCS
	"src/pax_codecs.c"
	"libspng/spng/spng.c"
	INCLUDE_DIRS "include" "libspng/spng" "zlib"
	REQUIRES pax-gfx esp_rom
)

# Build static library, do not build test executables.
option(BUILD_SHARED_LIBS OFF)
option(BUILD_TESTING OFF)

# Import libraries.
add_subdirectory(zlib)

# Link libraries to component.
target_link_libraries(${COMPONENT_LIB} PUBLIC zlib)
