MAKEFILE_PATH := $(abspath $(lastword $(MAKEFILE_LIST)))
MAKEFILE_DIR  := $(dir $(MAKEFILE_PATH))
MAKEFLAGS += --no-print-directory

CMAKE_BUILD_TYPE ?= Release
CMAKE_FLAGS := -DCMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE)

all: cmake

cmake: build/.ran-cmake
	cmake --build build

clean: build/.ran-cmake
	cmake --build build -t clean

build/.ran-cmake:
	mkdir -p build
	cd build && cmake $(CMAKE_FLAGS) $(MAKEFILE_DIR)
	touch $@

dist-clean:
	rm -rf build
