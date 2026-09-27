# Require an installed host Clang before compiler detection or dependency downloads.
include_guard(GLOBAL)
if(NOT CMAKE_HOST_WIN32)
    return()
endif()

find_program(ERLANG_AOT_CLANG_EXECUTABLE NAMES clang
    HINTS "$ENV{ProgramFiles}/LLVM/bin" "$ENV{ProgramW6432}/LLVM/bin"
    DOC "Installed host Clang executable (never downloaded by CMake)")
if(NOT ERLANG_AOT_CLANG_EXECUTABLE)
    message(FATAL_ERROR "Clang is required on Windows. Install LLVM/Clang globally and add its bin directory to PATH, or set ERLANG_AOT_CLANG_EXECUTABLE.")
endif()
execute_process(COMMAND "${ERLANG_AOT_CLANG_EXECUTABLE}" --version
    RESULT_VARIABLE clang_result OUTPUT_VARIABLE clang_version ERROR_VARIABLE clang_error TIMEOUT 15)
if(NOT clang_result STREQUAL "0" OR NOT clang_version MATCHES "clang version")
    message(FATAL_ERROR "Cannot run installed Clang at ${ERLANG_AOT_CLANG_EXECUTABLE}: ${clang_result}\n${clang_error}")
endif()
message(STATUS "Installed Clang: ${ERLANG_AOT_CLANG_EXECUTABLE}")
