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

PROJECT_NAME := ClassicReverb-RE04
PROJECT_VERSION = $(shell sed -n 's/^project([^)]*VERSION \([0-9.]*\).*/\1/p' CMakeLists.txt)
GIT_COMMIT = $(shell git rev-parse --short=8 HEAD)

BUILD_DIR := $(CURDIR)/build
OUTPUT_FILE = $(BUILD_DIR)/$(PROJECT_NAME)-$(ARCH)-$(OS_TYPE)-$(PROJECT_VERSION)-$(GIT_COMMIT).zip

ifeq ($(UNAME_S),)
# On this situation, the platform is not unix-compatible. This is not supported by us.
  $(error You should use either Unix-compatible platform or Windows (Msys2) for running this Makefile.)
endif

ifeq ($(UNAME_S),Linux)
  $(info Detected operating system: Linux)
  OS_TYPE := Linux
else ifeq ($(UNAME_O),Msys)
  $(info Detected operating system: Windows (Msys2))
  OS_TYPE := Windows
else
  $(info Your platform ($(UNAME_S)) is not supported yet. Try compiling manually.)
endif

.PHONY: all check_dependencies configure build package clean

all: package

check_dependencies:
	@which g++ > /dev/null || (echo "Error: g++ is not installed." && exit 1)
	@which cmake > /dev/null || (echo "Error: cmake is not installed." && exit 1)
	@which ninja > /dev/null || (echo "Error: ninja is not installed." && exit 1)
	@which ccache > /dev/null || (echo "Error: ccache is not installed." && exit 1)
	@which zip > /dev/null || (echo "Error: zip is not installed." && exit 1)
	@which git > /dev/null || (echo "Error: git is not installed." && exit 1)
	@which sed > /dev/null || (echo "Error: sed is not installed." && exit 1)

configure: check_dependencies
	@cmake -S . -B $(BUILD_DIR) -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER_LAUNCHER=ccache -DCMAKE_C_COMPILER_LAUNCHER=ccache

build: configure
	@cmake --build $(BUILD_DIR)
	@ccache -s

package: build
	@cd $(BUILD_DIR) && zip -r $(OUTPUT_FILE) bin/
	@echo "Packaged $(OUTPUT_FILE) successfully."

clean:
	@cd $(BUILD_DIR) && ninja clean
	@rm -rf $(OUTPUT_FILE)

distclean:
	@rm -rf $(BUILD_DIR)
