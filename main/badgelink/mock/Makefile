
# SPDX-Copyright-Text: 2025 Julian Scheffers
# SPDX-License-Identifier: MIT

PORT ?= /dev/pts/1

.PHONY: run
run: build
	./build/badgemock $(PORT)

.PHONY: valgrind
valgrind: build
	valgrind ./build/badgemock $(PORT)

.PHONY: gdb
gdb: build
	gdb ./build/badgemock -ex 'b main' -ex 'r $(PORT)'

.PHONY: build
build:
	cmake -B build
	cmake --build build
