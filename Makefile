# Override paths and configuration on the command line, e.g. BUILD_TYPE=Release.
CMAKE ?= cmake
CTEST ?= ctest
BUILD_DIR ?= build/debug
BUILD_TYPE ?= Debug
JOBS ?= 2
CMAKE_ARGS ?=
CLANG_FORMAT ?= $(shell command -v clang-format 2>/dev/null || xcrun --find clang-format 2>/dev/null)

.PHONY: build build_test test format fmt clean

# Reconfigure each time; let CMake control jobs independently of outer make flags.
# Build only the compiler executable and its dependencies for manual runs.
build:
	$(CMAKE) -S . -B "$(BUILD_DIR)" $(CMAKE_ARGS) \
		-DCMAKE_BUILD_TYPE="$(BUILD_TYPE)" -DBUILD_TESTING=OFF \
		-DERLANG_AOT_BUILD_COMPILER=ON -DERLANG_AOT_BUILD_RUNTIME=ON
	+MAKEFLAGS= $(CMAKE) --build "$(BUILD_DIR)" --config "$(BUILD_TYPE)" --target erlang_aot --parallel "$(JOBS)"

# Build all executables including those for testing
build_test:
	$(CMAKE) -S . -B "$(BUILD_DIR)" $(CMAKE_ARGS) \
		-DCMAKE_BUILD_TYPE="$(BUILD_TYPE)" -DBUILD_TESTING=ON \
		-DERLANG_AOT_BUILD_COMPILER=ON -DERLANG_AOT_BUILD_RUNTIME=ON
	+MAKEFLAGS= $(CMAKE) --build "$(BUILD_DIR)" --config "$(BUILD_TYPE)" --parallel "$(JOBS)"

# Build first, propagate failures, and show diagnostics for failing tests.
test: build_test
	$(CTEST) --test-dir "$(BUILD_DIR)" -C "$(BUILD_TYPE)" --output-on-failure --no-tests=error

# Format project C++ files, excluding generated and vendored build trees.
format fmt:
	@test -n "$(CLANG_FORMAT)" || { echo "clang-format is required" >&2; exit 1; }
	@find compiler runtime abi tests -type f \( -name '*.cpp' -o -name '*.cc' -o -name '*.cxx' -o -name '*.hpp' -o -name '*.h' -o -name '*.hh' -o -name '*.hxx' \) -print0 | xargs -0 "$(CLANG_FORMAT)" -i --style=file

clean:
	rm -rf build/ cmake-build*/
