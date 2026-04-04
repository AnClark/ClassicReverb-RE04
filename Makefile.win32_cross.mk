#
# Classic Reverb RE-04 Makefile
#
# This Makefile is designed to automate the build and packaging process for the Classic Reverb RE-04 project.
# It checks for necessary dependencies, configures the build environment, compiles the project, and packages the output into a zip file.
#
# SPDX-License-Identifier: MIT
#

UNAME_O = $(shell uname -o)
UNAME_S = $(shell uname -s)
ARCH = $(shell uname -m)
OS_TYPE := Windows

PROJECT_NAME := $(shell sed -n 's/^project(\([^ )]*\).*/\1/p' CMakeLists.txt)
PROJECT_VERSION = $(shell sed -n 's/^project([^)]*VERSION \([0-9.]*\).*/\1/p' CMakeLists.txt)
GIT_COMMIT = $(shell git rev-parse --short=8 HEAD)

BUILD_DIR = $(TMPDIR)/build_$(PROJECT_NAME)_$(ARCH)_$(OS_TYPE)_CROSS
OUTPUT_FILE = $(BUILD_DIR)/$(PROJECT_NAME)-$(ARCH)-$(OS_TYPE)-$(PROJECT_VERSION)-$(GIT_COMMIT).zip
WIN32_TOOLCHAIN_FILE = $(BUILD_DIR)/Toolchain-mingw-w64-x86_64.cmake

$(info Cross-compiling for Windows.)

ifeq ($(UNAME_S),Linux)
  $(info Detected operating system: Linux)
  TMPDIR := /tmp
else ifeq ($(UNAME_S),Darwin)
  $(info Detected operating system: macOS)
else ifeq ($(UNAME_O),Msys)
  $(info Detected operating system: Windows (Msys2))
  $(error Detected Msys2 environment. If you want to build for Windows, please directly use Makefile instead of this one.)
else
  $(info Your platform ($(UNAME_S)) is not supported yet. Try compiling manually.)
endif

.PHONY: all check_dependencies configure build package clean

all: package

check_dependencies:
	@which x86_64-w64-mingw32-g++ > /dev/null || (echo "Error: x86_64-w64-mingw32-g++ is not installed." && exit 1)
	@which cmake > /dev/null || (echo "Error: cmake is not installed." && exit 1)
	@which ninja > /dev/null || (echo "Error: ninja is not installed." && exit 1)
	@which ccache > /dev/null || (echo "Error: ccache is not installed." && exit 1)
	@which zip > /dev/null || (echo "Error: zip is not installed." && exit 1)
	@which git > /dev/null || (echo "Error: git is not installed." && exit 1)
	@which sed > /dev/null || (echo "Error: sed is not installed." && exit 1)

configure: check_dependencies
	@mkdir -p $(BUILD_DIR)
	@echo "SET(CMAKE_SYSTEM_NAME Windows)" > $(WIN32_TOOLCHAIN_FILE)
	@echo "SET(CMAKE_SYSTEM_PROCESSOR x86_64)" >> $(WIN32_TOOLCHAIN_FILE)
	@echo "SET(CMAKE_C_COMPILER /usr/bin/x86_64-w64-mingw32-gcc)" >> $(WIN32_TOOLCHAIN_FILE)
	@echo "SET(CMAKE_CXX_COMPILER /usr/bin/x86_64-w64-mingw32-g++)" >> $(WIN32_TOOLCHAIN_FILE)
	@echo "SET(CMAKE_RC_COMPILER /usr/bin/x86_64-w64-mingw32-windres)" >> $(WIN32_TOOLCHAIN_FILE)
	@echo "SET(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32/ ${PREFIX_PATH})" >> $(WIN32_TOOLCHAIN_FILE)

	@cmake -S . -B $(BUILD_DIR) -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER_LAUNCHER=ccache -DCMAKE_C_COMPILER_LAUNCHER=ccache \
		-DCMAKE_TOOLCHAIN_FILE=$(WIN32_TOOLCHAIN_FILE)

build: configure
	@cmake --build $(BUILD_DIR) --parallel $(shell nproc)
	@ccache -s

package: build
	cd $(BUILD_DIR) && zip -r $(OUTPUT_FILE) bin/
	@echo "Packaged $(OUTPUT_FILE) successfully."

clean:
	cd $(BUILD_DIR) && ninja clean
	rm -rf $(OUTPUT_FILE)

distclean:
	rm -rf $(BUILD_DIR)
