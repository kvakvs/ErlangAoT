# Override paths and configuration on the command line, e.g. BUILD_TYPE=Release.
CMAKE ?= cmake
CTEST ?= ctest
BUILD_DIR ?= build/debug
BUILD_TYPE ?= Debug
JOBS ?= 2
CMAKE_ARGS ?=

.PHONY: build test

# Reconfigure each time; let CMake control jobs independently of outer make flags.
build:
	$(CMAKE) -S . -B "$(BUILD_DIR)" $(CMAKE_ARGS) \
		-DCMAKE_BUILD_TYPE="$(BUILD_TYPE)" -DBUILD_TESTING=ON \
		-DERLANG_AOT_BUILD_COMPILER=ON -DERLANG_AOT_BUILD_RUNTIME=ON
	+MAKEFLAGS= $(CMAKE) --build "$(BUILD_DIR)" --config "$(BUILD_TYPE)" --parallel "$(JOBS)"

# Build first, propagate failures, and show diagnostics for failing tests.
test: build
	$(CTEST) --test-dir "$(BUILD_DIR)" -C "$(BUILD_TYPE)" --output-on-failure --no-tests=error
