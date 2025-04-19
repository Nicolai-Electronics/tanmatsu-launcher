
# Options
CC             ?=/usr/bin/gcc
PAX_PATH       ?=../pax-gfx
PAXC_BUILD_DIR ?=build
PAXC_CCOPTIONS ?=-c -fPIC -DPAXC_STANDALONE -Iinclude -I$(PAX_PATH)/src -Ilibspng/spng -Izlib
PAXC_LDOPTIONS ?=-shared
PAXC_LIBS      ?=-lz

# Sources
SOURCES        =src/pax_codecs.c \
				libspng/spng/spng.c
HEADERS        =include/pax_codecs.h \
				libspng/spng/spng.h

# Outputs
OBJECTS        =$(shell echo " $(SOURCES)" | sed -E 's/ (src|libspng)/ $(PAXC_BUILD_DIR)\/\1/g;s/\.c/.c.o/g')
OBJECTS_DEBUG  =$(shell echo " $(SOURCES)" | sed -E 's/ (src|libspng)/ $(PAXC_BUILD_DIR)\/\1/g;s/\.c/.c.debug.o/g')
PAXC_LIB_PATH ?=build/libpaxcodecs.so

# Actions
.PHONY: all debug clean

all: build/pax_codecs_lib.so
	@mkdir -p build
	@cp build/pax_codecs_lib.so $(PAXC_LIB_PATH)

debug: build/pax_codecs_lib.debug.so
	@mkdir -p build
	@cp build/pax_codecs_lib.debug.so $(PAXC_LIB_PATH)

clean:
	rm -rf build

# Regular files
build/pax_codecs_lib.so: $(OBJECTS)
	@mkdir -p $(shell dirname $@)
	$(CC) $(PAXC_LDOPTIONS) -o $@ $^ $(PAXC_LIBS)

build/%.o: % $(HEADERS)
	@mkdir -p $(shell dirname $@)
	$(CC) $(PAXC_CCOPTIONS) -o $@ $< $(PAXC_LIBS)

# Debug files
build/pax_codecs_lib.debug.so: $(OBJECTS_DEBUG)
	@mkdir -p $(shell dirname $@)
	$(CC) $(PAXC_LDOPTIONS) -o $@ $^ $(PAXC_LIBS)

build/%.debug.o: % $(HEADERS)
	@mkdir -p $(shell dirname $@)
	$(CC) $(PAXC_CCOPTIONS) -ggdb -o $@ $< $(PAXC_LIBS)
